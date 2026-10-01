#pragma once

#include <chrono>
#include <thread>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/platform.h>

// Switch profiles of Free Skate show these two waits occupying nearly two
// cores during shader compilation. Sleeping lets their actual producers run.
// Keep the original implementations available for comparison on the same dump.
REXCVAR_DEFINE_BOOL(guest_render_wait_backoff, false, "Guest",
                    "Sleep briefly while waiting for render work to complete");

#if REX_PLATFORM_SWITCH
REX_EXTERN(__imp__sub_82327D00);
REX_EXTERN(__imp__sub_823BF8B0);

REX_HOOK_RAW(sub_82327D00) {
  if (!REXCVAR_GET(guest_render_wait_backoff)) {
    __imp__sub_82327D00(ctx, base);
    return;
  }

  const uint32_t request = ctx.r3.u32;
  const uint32_t flags = ctx.r4.u32;
  struct Frame {
    PPCContext& ctx;
    uint64_t stack, link;
    ~Frame() { ctx.r1.u64 = stack; ctx.lr = link; }
  } frame{ctx, ctx.r1.u64, ctx.lr};
  ctx.r1.u32 -= 128;
  REX_STORE_U32(ctx.r1.u32, uint32_t(frame.stack));
  sub_823A9DD8(ctx, base);
  const uint32_t caller_thread = ctx.r3.u32;
  sub_8234DF28(ctx, base);
  if (caller_thread == REX_LOAD_U32(ctx.r3.u32)) return;

  ctx.r3.u64 = request + 28;
  __imp__RtlEnterCriticalSection(ctx, base);
  REX_STORE_U32(request + 4, 0);
  REX_STORE_U32(request, 1);
  ctx.r3.u64 = request + 28;
  __imp__RtlLeaveCriticalSection(ctx, base);
  if (!REX_LOAD_U8(request + 16)) return;

  if (flags & 1) REX_STORE_U32(request + 8, 1);
  if (flags & 2) REX_STORE_U32(request + 12, 1);
  uint32_t polls = 0;
  while (!REX_LOAD_U32(request + 4)) {
    // Preserve the diagnostic poll counter, but never manufacture completion
    // or abandon the request on a host timeout. Only the producer releases it.
    REX_STORE_U32(0x82779BF4, REX_LOAD_U32(0x82779BF4) + 1);
    if (++polls == 64) {
      polls = 0;
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
  }
  ctx.r3.u64 = request + 28;
  __imp__RtlEnterCriticalSection(ctx, base);
  REX_STORE_U32(request, 0);
  ctx.r3.u64 = request + 28;
  __imp__RtlLeaveCriticalSection(ctx, base);
  REX_STORE_U32(0x82779BF0, REX_LOAD_U32(0x82779BF0) + 1);
}

REX_HOOK_RAW(sub_823BF8B0) {
  // This helper is called inside GPU-fence polling loops. Run its original
  // watchdog and status logic every time; back off only on its "keep waiting"
  // result. In particular, do not replace a pending fence with a completed one.
  __imp__sub_823BF8B0(ctx, base);
  if (REXCVAR_GET(guest_render_wait_backoff) && ctx.r3.u32) {
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }
}
#endif
