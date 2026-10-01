// Block the guest render thread on the swap-complete event it is waiting for.
//
// sub_82327EE0's hot loop at 0x82327FCC repeatedly calls sub_82397688 and
// compares its result with the one-frame deadline at 0x82779C04. The accessor
// reads the 64-bit swap-complete counter at 0x82970A10. The producer is known:
// the GPU command processor invokes sub_823975B8 from the PM4 interrupt path,
// and that callback increments the same counter after a completed swap.
//
// These strong definitions replace codegen's weak aliases for the tiny counter
// accessor and producer. Only the exact hot-loop call site blocks; all other
// counter reads preserve their generated behavior. A bounded wait protects
// against a missing interrupt, shutdown, or a hot cvar toggle.

#pragma once

#include <algorithm>

#include "guest_pacing_stats.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(guest_swap_wait, false, "Guest",
                    "Block the render thread until swap completion advances "
                    "the guest frame counter");
REXCVAR_DEFINE_UINT32(guest_swap_wait_timeout_us, 5000, "Guest",
                      "Maximum host wait before rechecking the guest counter");
REXCVAR_DEFINE_BOOL(guest_swap_wait_stats, false, "Guest",
                    "Log guest swap-wait engagement and duration");
REXCVAR_DEFINE_BOOL(guest_vblank_wait_sleep, false, "Guest",
                    "Sleep instead of spinning in the game's presentation-interval wait");
REXCVAR_DEFINE_BOOL(guest_vblank_late_no_wait, false, "Guest",
                    "Start the next frame at once after a late one instead of waiting for "
                    "the next vblank");

namespace thps::swap_wait {

inline constexpr uint32_t kCounter = 0x82970A10;
inline constexpr uint32_t kDeadline = 0x82779C04;
inline constexpr uint32_t kWaitLoopReturn = 0x82327FD0;

inline std::mutex g_mutex;
inline std::condition_variable g_changed;
inline std::atomic<uint64_t> g_waits{0};
inline std::atomic<uint64_t> g_timeouts{0};
inline std::atomic<uint64_t> g_total_wait_us{0};
inline std::atomic<uint64_t> g_max_wait_us{0};

inline bool DeadlineReached(uint8_t *base) {
  return REX_LOAD_U64(kCounter) >= REX_LOAD_U32(kDeadline);
}

inline void RecordWait(uint64_t elapsed_us, bool reached) {
  const uint64_t waits = g_waits.fetch_add(1, std::memory_order_relaxed) + 1;
  g_total_wait_us.fetch_add(elapsed_us, std::memory_order_relaxed);
  if (!reached) {
    g_timeouts.fetch_add(1, std::memory_order_relaxed);
  }

  uint64_t previous = g_max_wait_us.load(std::memory_order_relaxed);
  while (previous < elapsed_us &&
         !g_max_wait_us.compare_exchange_weak(previous, elapsed_us,
                                              std::memory_order_relaxed)) {
  }

  if (REXCVAR_GET(guest_swap_wait_stats) && (waits & 0xFF) == 0) {
    const uint64_t total = g_total_wait_us.load(std::memory_order_relaxed);
    REXLOG_INFO("guest-swap-wait: waits={} mean_us={} max_us={} timeouts={}",
                waits, total / waits,
                g_max_wait_us.load(std::memory_order_relaxed),
                g_timeouts.load(std::memory_order_relaxed));
  }
}

} // namespace thps::swap_wait

// Generated guest functions expose weak ELF aliases for title hooks. Mach-O
// has no equivalent C/C++ alias attribute, so Apple retains the generated
// functions while these cvars remain available to the shared launcher preset.
#if !defined(__APPLE__)
REX_HOOK_RAW(sub_82397688) {
  REX_FUNC_PROLOGUE();

  if (REXCVAR_GET(guest_swap_wait) &&
      ctx.lr == thps::swap_wait::kWaitLoopReturn) {
    std::unique_lock lock(thps::swap_wait::g_mutex);
    if (!thps::swap_wait::DeadlineReached(base)) {
      const auto start = std::chrono::steady_clock::now();
      const bool reached = thps::swap_wait::g_changed.wait_for(
          lock,
          std::chrono::microseconds(REXCVAR_GET(guest_swap_wait_timeout_us)),
          [base] { return thps::swap_wait::DeadlineReached(base); });
      const auto elapsed =
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now() - start)
              .count();
      thps::swap_wait::RecordWait(
          static_cast<uint64_t>(std::max<int64_t>(0, elapsed)), reached);
    }
  }

  ctx.r3.u64 = REX_LOAD_U64(thps::swap_wait::kCounter);
}

// The game's presentation-interval wait, in the same function: with its
// interval setting non-zero it loops
//
//     loc_823280D0: bl sub_82397698 ; ld r11,-25608(r31) ; cmpld r3,r11 ; blt
//
// on the vblank counter at 0x82970A08 until it reaches the target stored at
// 0x82779BF8, busy the whole time. Measured on the Switch (native renderer,
// Free Skate): 29% of the render thread's samples, a third of a core taken
// from the threads the frame is waiting for. At that call site this sleeps in
// short steps while the counter is short of the target. The value returned is
// the counter as the original reads it; the wait only stops burning a core.
//
// A late frame: when the counter has already passed the target, the game sets
// the target to the counter plus one (the read returning to 0x823280C8) and
// waits for that next vblank. On the Xbox 360 that waited for the display's
// next refresh; here the vblank is a 60 Hz timer and presentation does not
// wait for the display, so it only rounded every late frame up to the next
// tick: a frame that needed 36 ms took 50, and every slow frame in the logs
// was a multiple of 16.7 ms. With guest_vblank_late_no_wait the loop is let
// through at once for a late frame (the counter is reported as the target at
// that one read); frames on time still wait, so the frame rate cap is kept,
// and the next read, which the game times its next frame from, is the real
// counter.
REX_HOOK_RAW(sub_82397698) {
  REX_FUNC_PROLOGUE();
  constexpr uint32_t kVblankCounter = 0x82970A08;
  constexpr uint32_t kIntervalTarget = 0x82779BF8;
  constexpr uint32_t kIntervalLoopReturn = 0x823280D4;
  constexpr uint32_t kLateTargetReturn = 0x823280C8;
  static thread_local bool late_frame = false;
  if (ctx.lr == kLateTargetReturn) {
    late_frame = true;
  } else if (ctx.lr == kIntervalLoopReturn && late_frame) {
    late_frame = false;
    if (REXCVAR_GET(guest_vblank_late_no_wait)) {
      thps::pacing::g_late_skips.fetch_add(1, std::memory_order_relaxed);
      const uint64_t counter = REX_LOAD_U64(kVblankCounter);
      const uint64_t target = REX_LOAD_U64(kIntervalTarget);
      ctx.r11.s64 = -2104033280;
      ctx.r3.u64 = std::max(counter, target);
      return;
    }
  }
  if (REXCVAR_GET(guest_vblank_wait_sleep) && ctx.lr == kIntervalLoopReturn &&
      REX_LOAD_U64(kVblankCounter) < REX_LOAD_U64(kIntervalTarget)) {
    const auto start = std::chrono::steady_clock::now();
    const auto limit = start + std::chrono::milliseconds(20);
    while (REX_LOAD_U64(kVblankCounter) < REX_LOAD_U64(kIntervalTarget) &&
           std::chrono::steady_clock::now() < limit) {
      std::this_thread::sleep_for(std::chrono::microseconds(250));
    }
    thps::pacing::g_vblank_wait_ns.fetch_add(
        uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
                     std::chrono::steady_clock::now() - start)
                     .count()),
        std::memory_order_relaxed);
  }
  // Generated sub_82397698 body.
  ctx.r11.s64 = -2104033280;
  ctx.r3.u64 = REX_LOAD_U64(ctx.r11.u32 + 2568);
}

REX_HOOK_RAW(sub_823975B8) {
  REX_FUNC_PROLOGUE();

  const bool enabled = REXCVAR_GET(guest_swap_wait);
  std::unique_lock<std::mutex> lock(thps::swap_wait::g_mutex, std::defer_lock);
  if (enabled) {
    lock.lock();
  }

  // Generated sub_823975B8 body, kept verbatim apart from named addresses.
  ctx.r11.s64 = -2104033280;
  ctx.r11.s64 = ctx.r11.s64 + 2312;
  ctx.r9.s64 = ctx.r11.s64 + 248;
  ctx.r10.u64 = REX_LOAD_U64(ctx.r11.u32 + 0);
  REX_STORE_U64(ctx.r11.u32 + 8, ctx.r10.u64);
  ctx.r10.u64 = REX_LOAD_U64(ctx.r11.u32 + 264);
  ctx.r10.s64 = ctx.r10.s64 + 1;
  REX_STORE_U64(ctx.r11.u32 + 264, ctx.r10.u64);
  ctx.r10.u64 = REX_LOAD_U64(ctx.r9.u32 + 0);
  REX_STORE_U64(ctx.r11.u32 + 0, ctx.r10.u64);
  ctx.r10.u64 = REX_QUERY_TIMEBASE();
  ctx.r9.s64 = -2104033280;
  ctx.r9.u64 = REX_LOAD_U64(ctx.r9.u32 + 2432);
  ctx.r10.u64 = ctx.r10.u64 - ctx.r9.u64;
  REX_STORE_U64(ctx.r11.u32 + 72, ctx.r10.u64);

  if (enabled) {
    lock.unlock();
    thps::swap_wait::g_changed.notify_one();
  }

}
#endif
