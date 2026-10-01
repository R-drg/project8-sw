// Stop the game's main thread from spinning a core away while it waits for its
// job threads.
//
// WHAT WAS MEASURED (Switch, native renderer, Free Skate busy area): the main
// thread spent 16% of its samples in sub_821E6928, a loop that re-reads one
// word of a job table until a job thread clears it:
//
//     r10 = [0x82731F38 + 24], r9 = [+8], r8 = [+4]   ; loaded once
//   loop:
//     v = (r3 == -1) ? r8 - r10 : [r9 + r3 * 32]
//     if (v != 0) goto loop
//
// On the Xbox 360 a spinning hardware thread is nearly free. On the Switch the
// game has three cores for all its threads, and the spin competes with the job
// threads that have to finish before it can end.
//
// WHAT THIS DOES: the same loop, which yields the core once it has spun for a
// while (and sleeps briefly after a long wait). The result and the registers
// the original leaves behind are the same; nothing is skipped or faked. The
// r3 == -1 form compares two values loaded once, so it cannot change while
// spinning and is left to the original.
//
// The same cvar covers the task scheduler that calls it, sub_823760D0: it walks
// the frame's task list dispatching every task whose two dependencies are
// done, and walks it again straight away while tasks remain, so with nothing
// ready it spins until a job thread finishes one (12% of the main thread in a
// busy Free Skate area). The replacement below is the generated code of that
// function, unchanged except that a pass dispatching nothing is followed by a
// yield, and after a long wait a short sleep.
//
// A cvar, default OFF, as the house rule is for guest-behaviour patches.

#pragma once

#include <chrono>
#include <cstdint>
#include <thread>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/memory/utils.h>
#include <rex/platform.h>

REXCVAR_DEFINE_BOOL(guest_job_wait_yield, false, "Guest",
                    "Yield the core while the main thread waits for a job slot to clear");

namespace thps::job_wait {
// Called between passes over a wait: spin briefly, then yield, then sleep.
inline void Backoff(uint32_t& spins) {
  ++spins;
  if (spins < 64) return;
  if (spins < 64 + 2048) {
    std::this_thread::yield();
  } else {
    std::this_thread::sleep_for(std::chrono::microseconds(50));
  }
}
}  // namespace thps::job_wait

#if REX_PLATFORM_SWITCH
REX_EXTERN(__imp__sub_821E6928);
REX_EXTERN(__imp__sub_823760D0);
REX_EXTERN(__savegprlr_24);
REX_EXTERN(__restgprlr_24);
REX_EXTERN(sub_82216E58);
REX_EXTERN(sub_821E5890);
REX_EXTERN(sub_821E5FB0);
REX_EXTERN(sub_82376068);
REX_EXTERN(sub_821E6928);

REX_HOOK_RAW(sub_823760D0) {
  if (!REXCVAR_GET(guest_job_wait_yield)) {
    __imp__sub_823760D0(ctx, base);
    return;
  }
  REX_FUNC_PROLOGUE();
  uint32_t ea{};
  uint32_t spins = 0;
  bool dispatched = false;
  ctx.r12.u64 = ctx.lr;
  ctx.lr = 0x823760D8;
  __savegprlr_24(ctx, base);
  ea = -256 + ctx.r1.u32;
  REX_STORE_U32(ea, ctx.r1.u32);
  ctx.r1.u32 = ea;
  ctx.r3.s64 = 1250820096;
  ctx.r4.s64 = 0;
  ctx.r3.u64 = ctx.r3.u64 | 27644;
  ctx.lr = 0x823760EC;
  sub_82216E58(ctx, base);
  ctx.r25.s64 = -2105475072;
  ctx.r9.u64 = ctx.r3.u32 == 0 ? 32 : __builtin_clz(ctx.r3.u32);
  ctx.r29.s64 = 1;
  ctx.r9.u64 = __builtin_rotateleft64(ctx.r9.u32 | (ctx.r9.u64 << 32), 27) & 0x1;
  ctx.r10.u64 = REX_LOAD_U32(ctx.r25.u32 + 28020);
  ctx.r24.u64 = ctx.r9.u64 ^ 1;
  ctx.r11.u64 = REX_LOAD_U32(ctx.r10.u32 + 0);
  ctx.r8.u64 = REX_LOAD_U32(ctx.r10.u32 + 4);
  ctx.r11.u64 = ctx.r8.u64 - ctx.r11.u64;
  ctx.xer.ca = (ctx.r11.s32 < 0) & ((ctx.r11.u32 & 0x3) != 0);
  ctx.r27.s64 = ctx.r11.s32 >> 2;
  ctx.r11.s64 = -2106392576;
  ctx.r28.s64 = ctx.r11.s64 + 8056;
  ctx.r11.s64 = -2106720256;
  ctx.r26.s64 = ctx.r11.s64 + 27160;
loc_82376124:
  dispatched = false;
  ctx.r30.u64 = REX_LOAD_U32(ctx.r10.u32 + 0);
  ctx.r11.u64 = REX_LOAD_U32(ctx.r10.u32 + 4);
  ctx.cr6.compare<uint32_t>(ctx.r30.u32, ctx.r11.u32, ctx.xer);
  if (ctx.cr6.eq) goto loc_82376204;
loc_82376134:
  ctx.cr6.compare<int32_t>(ctx.r27.s32, 0, ctx.xer);
  if (ctx.cr6.eq) goto loc_8237620C;
  ctx.r31.u64 = REX_LOAD_U32(ctx.r30.u32 + 0);
  ctx.r11.u64 = REX_LOAD_U8(ctx.r31.u32 + 12);
  ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
  if (!ctx.cr6.eq) goto loc_823761F4;
  ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 4);
  ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
  if (ctx.cr6.eq) goto loc_82376168;
  ctx.r11.u64 = REX_LOAD_U8(ctx.r11.u32 + 13);
  ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
  ctx.r11.u64 = ctx.r29.u64;
  if (ctx.cr6.eq) goto loc_8237616C;
loc_82376168:
  ctx.r11.s64 = 0;
loc_8237616C:
  ctx.r11.u64 = ctx.r11.u32 & 0xFF;
  ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
  if (!ctx.cr6.eq) goto loc_823761F4;
  ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
  ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
  if (ctx.cr6.eq) goto loc_82376194;
  ctx.r11.u64 = REX_LOAD_U8(ctx.r11.u32 + 13);
  ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
  ctx.r11.u64 = ctx.r29.u64;
  if (ctx.cr6.eq) goto loc_82376198;
loc_82376194:
  ctx.r11.s64 = 0;
loc_82376198:
  ctx.r11.u64 = ctx.r11.u32 & 0xFF;
  ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
  if (!ctx.cr6.eq) goto loc_823761F4;
  dispatched = true;
  ctx.r3.s64 = ctx.r1.s64 + 80;
  REX_STORE_U8(ctx.r31.u32 + 12, ctx.r29.u8);
  ctx.lr = 0x823761B0;
  sub_821E5890(ctx, base);
  ctx.r11.u64 = REX_LOAD_U32(ctx.r28.u32 + 48);
  REX_STORE_U32(ctx.r1.u32 + 80, ctx.r31.u32);
  REX_STORE_U32(ctx.r1.u32 + 140, ctx.r26.u32);
  REX_STORE_U32(ctx.r1.u32 + 148, ctx.r11.u32);
  ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 16);
  ctx.cr6.compare<int32_t>(ctx.r11.s32, 1, ctx.xer);
  if (ctx.cr6.eq) goto loc_823761E4;
  ctx.r11.u64 = ctx.r24.u32 & 0xFF;
  ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
  if (ctx.cr6.eq) goto loc_823761E4;
  ctx.r3.s64 = ctx.r1.s64 + 80;
  ctx.lr = 0x823761E0;
  sub_821E5FB0(ctx, base);
  goto loc_823761EC;
loc_823761E4:
  ctx.r3.s64 = ctx.r1.s64 + 80;
  ctx.lr = 0x823761EC;
  sub_82376068(ctx, base);
loc_823761EC:
  ctx.r10.u64 = REX_LOAD_U32(ctx.r25.u32 + 28020);
  ctx.r27.s64 = ctx.r27.s64 + -1;
loc_823761F4:
  ctx.r11.u64 = REX_LOAD_U32(ctx.r10.u32 + 4);
  ctx.r30.s64 = ctx.r30.s64 + 4;
  ctx.cr6.compare<uint32_t>(ctx.r30.u32, ctx.r11.u32, ctx.xer);
  if (!ctx.cr6.eq) goto loc_82376134;
loc_82376204:
  ctx.cr6.compare<int32_t>(ctx.r27.s32, 0, ctx.xer);
  if (ctx.cr6.gt) {
    // The only change: nothing was ready on this pass, so the tasks left
    // wait on job threads; give them the core.
    if (dispatched) {
      spins = 0;
    } else {
      thps::job_wait::Backoff(spins);
    }
    goto loc_82376124;
  }
loc_8237620C:
  ctx.r3.u64 = REX_LOAD_U32(ctx.r28.u32 + 48);
  ctx.lr = 0x82376214;
  sub_821E6928(ctx, base);
  ctx.r1.s64 = ctx.r1.s64 + 256;
  __restgprlr_24(ctx, base);
  return;
}


REX_HOOK_RAW(sub_821E6928) {
  constexpr uint32_t kJobTable = 0x82731F38;
  if (!REXCVAR_GET(guest_job_wait_yield) || ctx.r3.s32 == -1) {
    __imp__sub_821E6928(ctx, base);
    return;
  }
  const uint32_t r10 = REX_LOAD_U32(kJobTable + 24);
  const uint32_t r9 = REX_LOAD_U32(kJobTable + 8);
  const uint32_t r8 = REX_LOAD_U32(kJobTable + 4);
  const uint32_t slot = r9 + ((ctx.r3.u32 << 5) & 0xFFFFFFE0u);
  // A volatile read: other threads clear the slot.
  const volatile uint32_t* word = reinterpret_cast<const volatile uint32_t*>(base + slot);
  uint32_t spins = 0;
  while (*word != 0) {
    thps::job_wait::Backoff(spins);
  }
  ctx.r8.u64 = r8;
  ctx.r9.u64 = r9;
  ctx.r10.u64 = r10;
  ctx.r11.u64 = 1;
}
#endif
