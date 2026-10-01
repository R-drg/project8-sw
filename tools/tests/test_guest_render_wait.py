#!/usr/bin/env python3
"""Exercise the actual render-wait hook with synthetic guest memory (no dump)."""
import pathlib
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]


class RenderWaitTest(unittest.TestCase):
    def test_completion_and_fallback(self):
        with tempfile.TemporaryDirectory() as directory:
            work = pathlib.Path(directory)
            headers = work / "rex"
            headers.mkdir()
            (headers / "platform.h").write_text("#define REX_PLATFORM_SWITCH 1\n")
            (headers / "cvar.h").write_text(
                "#define REXCVAR_DEFINE_BOOL(n,v,...) inline bool n=v\n"
                "#define REXCVAR_GET(n) (n)\n"
            )
            (headers / "hook.h").write_text(r'''
#pragma once
#include <atomic>
#include <cstdint>
union Register { uint64_t u64; uint32_t u32; };
struct PPCContext { Register r1{}, r3{}, r4{}, r30{}; uint64_t lr{}; };
#define REX_EXTERN(n) extern "C" void n(PPCContext&, uint8_t*)
#define REX_HOOK_RAW(n) extern "C" void n(PPCContext& ctx, uint8_t* base)
inline uint32_t load32(uint8_t* base, uint32_t address) {
  return __builtin_bswap32(std::atomic_ref(*reinterpret_cast<uint32_t*>(base + address)).load());
}
inline void store32(uint8_t* base, uint32_t address, uint32_t value) {
  std::atomic_ref(*reinterpret_cast<uint32_t*>(base + address)).store(__builtin_bswap32(value));
}
#define REX_LOAD_U32(a) load32(base, (a))
#define REX_STORE_U32(a,v) store32(base, (a), (v))
#define REX_LOAD_U8(a) base[(a)]
REX_EXTERN(sub_823A9DD8);
REX_EXTERN(sub_8234DF28);
REX_EXTERN(__imp__RtlEnterCriticalSection);
REX_EXTERN(__imp__RtlLeaveCriticalSection);
''')
            source = work / "test.cpp"
            source.write_text(r'''
#include <cassert>
#include <future>
#include <sys/mman.h>
#include "src/game/src/guest_render_wait.h"

unsigned fallback_calls = 0, enters = 0, leaves = 0;
uint32_t fence_status = 1;
constexpr uint32_t request = 0x1000, thread_state = 0x2000, stack = 0x4000;
REX_HOOK_RAW(__imp__sub_82327D00) { ++fallback_calls; }
REX_HOOK_RAW(__imp__sub_823BF8B0) { ++fallback_calls; ctx.r3.u64 = fence_status; }
REX_HOOK_RAW(sub_823A9DD8) { ctx.r3.u64 = 42; ctx.lr = 0x1111; }
REX_HOOK_RAW(sub_8234DF28) { ctx.r3.u64 = thread_state; ctx.lr = 0x2222; }
REX_HOOK_RAW(__imp__RtlEnterCriticalSection) {
  assert(ctx.r3.u32 == request + 28); ++enters;
}
REX_HOOK_RAW(__imp__RtlLeaveCriticalSection) {
  assert(ctx.r3.u32 == request + 28); ++leaves;
}
PPCContext context(uint32_t flags) {
  PPCContext ctx{};
  ctx.r1.u64 = stack; ctx.lr = 0x12345678; ctx.r30.u64 = 0xabcdef;
  ctx.r3.u64 = request; ctx.r4.u64 = flags;
  return ctx;
}
void preserved(const PPCContext& ctx) {
  assert(ctx.r1.u64 == stack && ctx.lr == 0x12345678 && ctx.r30.u64 == 0xabcdef);
}
int main() {
  auto* base = static_cast<uint8_t*>(mmap(nullptr, size_t(1) << 32,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0));
  assert(base != MAP_FAILED);
  auto ctx = context(3);
  sub_82327D00(ctx, base);
  assert(fallback_calls == 1 && enters == 0);
  guest_render_wait_backoff = true;
  store32(base, thread_state, 42);
  sub_82327D00(ctx, base);
  assert(enters == 0); preserved(ctx);

  store32(base, thread_state, 99);
  ctx = context(3);
  sub_82327D00(ctx, base); // No waiter: publish without waiting or clearing it.
  assert(load32(base, request) == 1 && enters == 1 && leaves == 1);
  preserved(ctx);
  base[request + 16] = 1;
  for (uint32_t flags = 0; flags != 4; ++flags) {
    store32(base, request, 0);
    store32(base, request + 4, 7); // Stale completion must be reset.
    store32(base, request + 8, 0);
    store32(base, request + 12, 0);
    ctx = context(flags);
    auto done = std::async(std::launch::async, [&] { sub_82327D00(ctx, base); });
    while (!load32(base, request)) std::this_thread::yield();
    assert(done.wait_for(std::chrono::milliseconds(3)) == std::future_status::timeout);
    assert(load32(base, request + 4) == 0);
    store32(base, request + 4, 7); // Only the producer completes the wait.
    assert(done.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    done.get();
    assert(load32(base, request) == 0);
    assert(load32(base, request + 8) == (flags & 1 ? 1u : 0u));
    assert(load32(base, request + 12) == (flags & 2 ? 1u : 0u));
    preserved(ctx);
  }
  assert(enters == 9 && leaves == 9);
  assert(load32(base, 0x82779BF0) == 4 && load32(base, 0x82779BF4) > 0);
  for (bool enabled : {false, true}) {
    guest_render_wait_backoff = enabled;
    for (uint32_t result : {0u, 1u}) {
      fence_status = result;
      ctx = context(0);
      sub_823BF8B0(ctx, base);
      assert(ctx.r3.u32 == result); preserved(ctx);
    }
  }
  assert(fallback_calls == 5);
  munmap(base, size_t(1) << 32);
}
''')
            subprocess.run(
                ["c++", "-std=c++20", "-O2", "-pthread", "-I", str(work),
                 "-I", str(ROOT), str(source), "-o", str(work / "test")],
                check=True,
            )
            subprocess.run([str(work / "test")], check=True, timeout=10)


if __name__ == "__main__":
    unittest.main()
