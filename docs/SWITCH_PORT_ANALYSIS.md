# Nintendo Switch feasibility audit

Status: **experimental probes; runtime and game execution are not established**.
The supplied title-takeover hardware logs confirm C++23 threads/TLS and tiny
simultaneous shared-memory aliases. Reprotection of SharedMem returned `0xD401`;
both corrected Vulkan probes presented 300 frames, with visible triangles and
clean return confirmed by the user. The upstream GPU readback smoke also passed. See [the reprotection audit](SWITCH_MEMORY_REPROTECTION.md). No runtime
or Project 8 integration proceeds until the outstanding gates are resolved.

## Revisions and scope

The target is Horizon homebrew, AArch64, little endian, 64-bit pointers, libnx,
devkitA64 and NRO. It is not Linux on Switch.

- Project8Recomp uses ReXGlue **v0.10.0**, commit
  `f5337cdc947ff6d4c4196737e2c807a48f2a1fc2`, followed by the patches in
  [series](../patches/rexglue-sdk/series). The first two patches were applied
  before auditing. Current upstream main is not silently substituted.
- switch-nvk inspected: `6eec707da3ad5f86c64f748226583202801bfd03`.
- nxvk inspected: `e028d428c3b1f2dabadd81645c53cddfc8d7ab51`.
- Build environment: existing devkitPro Docker image
  `sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282`,
  devkitA64 GCC 15.2.0, additional host Clang 19.1.7.

SDK paths below refer to that pinned SDK plus this repository's overlay.
Links point at upstream source; overlay differences are in
[the runtime patch](../patches/rexglue-sdk/0001-thp8-runtime-stack.patch).
No game dump, recompiled translation unit, keys or game assets were needed.

## Subsystems

These classifications describe the work still required, not a certification.

| Subsystem | Classification | Source evidence and Switch consequence |
|---|---|---|
| Build system | moderate Switch adaptation | SDK [CMakeLists.txt](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/CMakeLists.txt) enforces Clang 18+/AppleClang 16+, C++23, 64-bit pointers, and adds both target runtime and host codegen. Cross builds must separate native tools from Horizon libraries. Project [src/game/CMakeLists.txt](../src/game/CMakeLists.txt) already distinguishes `THPS_P8_REXGLUE_HOST_EXECUTABLE`; it also requires locally generated CMake input. |
| Platform identity | trivial Switch adaptation | SDK [include/rex/platform.h](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/include/rex/platform.h) already detects AArch64, but rejects Horizon. New patch adds explicit Switch detection, endian/pointer assertions and a platform-only target. Desktop OS flags stay false. |
| Generated CPU code | already portable at the architecture level; hardware correctness unknown | [resources/templates/codegen/pch_h.inja](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/resources/templates/codegen/pch_h.inja), `REX_LOAD_*`, `REX_STORE_*`, `REX_FUNC_PROLOGUE`, and [src/codegen/builders/memory.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/codegen/builders/memory.cpp) emit C++ with byte swaps and base-relative guest addresses. ARM64 intrinsics already exist in `include/rex/ppc/intrinsics.h`; this is not proof that all PPC instructions or floating-point behavior work on this toolchain. |
| Runtime startup | moderate Switch adaptation | [src/system/runtime.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/system/runtime.cpp), `Runtime::Setup`, installs exception support, creates memory, kernel state and dispatch, then injected input/audio/GPU systems and VFS. A blank-runtime executable still depends on real memory/exception implementations. |
| Guest address space/backing | major blocker | [src/system/xmemory.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/system/xmemory.cpp), `Memory::Initialize`, `map_info`, `MapViews`, and `PhysicalHeap` require simultaneous shared aliases and subrange protection. Tiny coherent Horizon aliases are hardware-confirmed; mutable permissions, fault recovery and scaling remain unresolved. Detailed layout below. |
| Host virtual memory | major blocker | [src/core/memory_posix.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/core/memory_posix.cpp), `AllocFixed`, `Protect`, `CreateFileMappingHandle`, `MapFileView`, use mmap/mprotect/shm_open/ftruncate and Linux map introspection. These are not enabled by defining Switch as POSIX. |
| Fault handling and memory watches | major blocker | [src/system/mmio_handler.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/system/mmio_handler.cpp), `MMIOHandler::Install` / `ExceptionCallback`, and [src/core/exception_handler_posix.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/core/exception_handler_posix.cpp) rely on recoverable host faults and register context. `src/core/seh_posix.cpp` explicitly asserts Linux or macOS. SVC permission success alone does not establish fault recovery. |
| Executable guest memory | already portable for static dispatch | `xmemory.cpp::ToPageAccess` produces R/RW/no-access, not executable guest mappings. [function_dispatcher.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/system/function_dispatcher.cpp), `AllocateThunk`, assigns guest addresses to compiled `PPCFunc` pointers via `SetFunction`; it does not emit native instructions. No JIT/RWX requirement was found in this guest path. Generic host memory APIs expose executable permissions, so this conclusion is scoped to static guest execution. |
| Xenos GPU plugin | moderate Switch adaptation | [src/graphics/CMakeLists.txt](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/graphics/CMakeLists.txt) builds `rexgpu-xenos` SHARED. [src/system/gpu_plugin_loader.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/system/gpu_plugin_loader.cpp), `LoadGpuPlugin`, opens a library and resolves the ABI/factory symbols. [plugin_main.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/graphics/plugin_main.cpp), `rex_gpu_create`, is the existing factory to reuse for Switch-only static registration after the gates pass. |
| Vulkan device/emulation | diagnostic presentation passed; Xenos emulation untested | [src/ui/vulkan/vulkan_device.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/ui/vulkan/vulkan_device.cpp), `VulkanDevice::Create`, rejects devices without independentBlend, fragmentStoresAndAtomics or vertexPipelineStoresAndAtomics for GPU emulation. Optional features/extensions are queried separately. See [capability report](SWITCH_VULKAN_CAPABILITIES.md). |
| Vulkan loader and presentation | moderate Switch adaptation | [vulkan_instance.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/ui/vulkan/vulkan_instance.cpp), `VulkanInstance::Create`, loads a desktop loader. [vulkan_presenter.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/ui/vulkan/vulkan_presenter.cpp) selects desktop surface types and manages swapchains. `include/rex/ui/vulkan/api.h`, `include/rex/ui/surface.h` and `src/ui/CMakeLists.txt` need an explicit VI/NWindow branch, static ICD lookup and no X11/Wayland fallback. No shader translator change is justified yet. |
| SDL/window/event loop | moderate Switch adaptation | [src/ui/window_sdl.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/ui/window_sdl.cpp), `windowed_app_context_sdl.cpp`, and [thirdparty/CMakeLists.txt](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/thirdparty/CMakeLists.txt) select SDL3 and native desktop window handles; Linux explicitly enables X11/Wayland and desktop audio. libnx lifecycle/window ownership must be connected deliberately. The probes avoid SDL. |
| Filesystem | moderate Switch adaptation | [src/core/filesystem_posix.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/core/filesystem_posix.cpp), `OpenFile`, `FileHandle::Read`, `OpenExisting`, and `src/filesystem/devices/host_path_entry.cpp` use stdio, positional I/O, metadata, rename and deletion behavior. `MappedMemory` still uses mmap; user modules call `OpenMapped`. A buffered read implementation and explicit sdmc:/ paths need testing. |
| Threads | major blocker for full semantics | [src/core/threading_posix.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/core/threading_posix.cpp), `PosixCondition`, uses pthread creation/TLS, priorities/affinity, signal-based suspend/APCs, semaphores and cancellation. libnx/newlib pthread availability does not establish these desktop semantics. The small jthread/TLS probe only tests an initial subset. |
| Synchronization | moderate Switch adaptation | Same threading source implements waitable objects, mutex/condition/event/semaphore waits; `src/core/atomic_posix.cpp` and `src/core/mutex.cpp` supply atomics/locks. Wait-all, deadlines, cancellation and recursive/abandoned mutex behavior require targeted tests. No always-success replacements are supplied. |
| Fibers | moderate Switch adaptation | [src/core/fiber_posix.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/core/fiber_posix.cpp), `Fiber::Create` / `SwitchTo`, use getcontext/makecontext/swapcontext; `include/rex/thread/fiber.h` selects desktop contexts. A real Horizon context backend is required. |
| Audio | moderate Switch adaptation | [src/audio/CMakeLists.txt](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/audio/CMakeLists.txt) unconditionally includes SDL audio and links FFmpeg avcodec/avutil; `src/audio/xma_decoder.cpp` implements decoding. FFmpeg's platform configuration and SDL/libnx audio output need separate cross builds. No dummy backend is being represented as working audio. |
| Input | moderate Switch adaptation | [src/input/CMakeLists.txt](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/input/CMakeLists.txt) selects SDL gamepads on non-Windows; `src/input/sdl/sdl_input_driver.cpp` owns controller behavior. libnx pad input works only as an unverified probe exit path here. |
| Other dynamic libraries | moderate Switch adaptation | [src/core/dynlib_posix.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/core/dynlib_posix.cpp), `DynamicLibrary`, uses dlopen/dlsym/dlclose. Vulkan loader, GPU plugin and optional desktop tooling require explicit static/disabled branches; a global fake dlopen is not an implementation. |
| Desktop launcher/supervisor | major adaptation, outside the probes | [src/launcher/src/platform_posix.inc](https://github.com/R-drg/Project8Recomp/blob/main/src/launcher/src/platform_posix.inc), [src/game/src/launcher](../src/game/src/launcher), and launcher CMake are desktop process/UI infrastructure. Keep the SDK-free launcher boundary and dump gate intact; neither is part of the first Switch executable. |

## Exact guest memory contract

`Memory::Initialize` creates a backing object of `0x120000000` plus one host
allocation granule, rounded to that granule. That is **4.5 GiB of logical backing
extent**, not a claim that 4.5 GiB is resident RAM. The physical guest heap is
`0x20000000` bytes (**512 MiB**) and initialization precommits its raw view for
GPU access. Virtual/image heaps and host/GPU allocations are additional budget
items. A Switch budget cannot be inferred from the Xbox 360's RAM size alone.

The mapping table is authoritative; an older introductory comment describing
low-memory aliasing does not match all the entries in `map_info`.

| Offset from host base B | Size | Backing-object offset |
|---|---:|---:|
| 0x00000000 | 0x40000000 | 0x00000000 |
| 0x40000000 | 0x3F000000 | 0x40000000 |
| 0x7F000000 | 0x01000000 | 0x100000000 |
| 0x80000000 | 0x10000000 | 0x80000000 |
| 0x90000000 | 0x10000000 | 0x80000000 |
| 0xA0000000 | 0x20000000 | 0x100000000 |
| 0xC0000000 | 0x20000000 | 0x100000000 |
| 0xE0000000 | 0x20000000 | 0x100001000 |
| 0x100000000 | 0x20000000 | 0x100000000 |

Non-macOS `MapViews` tries host bases `1ULL << n`, starting at n=32. macOS
already obtains a free contiguous arena, so the portable contract is a stable
base with the required offsets, not necessarily absolute host address 4 GiB.
Generated loads/stores add B to a 32-bit guest address. They do not perform
a software page-table lookup on every access. Large-host-page compensation
for the E alias appears in `REX_PHYS_HOST_OFFSET`; Switch's 4 KiB case needs
the E view's actual 4 KiB backing bias.

The aliases must exist simultaneously. Copying memory on a view change does
not satisfy this contract. Page access changes, queried protection, and
recoverable write faults are also needed by MMIO/write watches. `ToPageAccess`
does not map guest code executable; native code is compiled into the NRO.

## Memory experiment and its limits

[memory_probe/main.cpp](../switch_probe/memory_probe/main.cpp) allocates only
one 4 KiB backing object per test group and releases it before the next group.
It reserves a 4.5 GiB *virtual* range using `virtmemFindAslr` and
`virtmemAddReservation` under `virtmemLock`; this does not allocate 512 MiB.
It records applet type, total/used process memory and ASLR region information
using `svcGetInfo` before attempting mappings.

The physical test represents backing offset 0x1000 and maps it at
B+0x7F001000, B+0xA0001000, B+0xC0001000, B+0xE0000000 and B+0x100001000.
It writes through each alias and reads all other aliases. A second object
tests B+0x80000000 and B+0x90000000. It requests R, no-access and RW on one
view with `svcSetMemoryPermission`, verifies reported permissions with
`svcQueryMemory`, then unmaps and closes handles.

libnx's installed `switch/kernel/svc.h` documents:

- `svcMapMemory` (0x04) removes source access, so it is not simultaneous aliases.
- `svcCreateSharedMemory` (0x50) is privileged; `envIsSyscallHinted` must be
  checked before calling it. Missing availability is logged as **BLOCKED**.
- `svcMapSharedMemory` / `svcUnmapSharedMemory` (0x13/0x14) operate on handles.
- `svcSetMemoryPermission` (0x02) allows no-access/R/RW, not execute permission;
  whether a given memory state permits changes is still tested, not assumed.

These declarations are also in [libnx svc.h](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/kernel/svc.h)
and [virtmem.h](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/kernel/virtmem.h).

**This experiment can fail without proving every Horizon solution impossible.**
In particular, loader syscall permissions may disallow creating shared memory.
No untested process-memory or transfer-memory alternative is claimed to work.
The shared-memory alias route is hardware-confirmed for these tiny groups.
The result is ALIASING_PASS, separately from
REPROTECTION_UNSUPPORTED_WITH_SHAREDMEM. R/None/RW requests all returned
`0xD401`, with SharedMem state `0x6` and RW unchanged. Cleanup succeeded.
See [the exact protection-use classification](SWITCH_MEMORY_REPROTECTION.md).
Fault recovery, scalable backing/subranges, handle limits, remapping and GPU
visibility remain untested. Used-memory accounting includes the startup heap;
it must not be interpreted as live game allocations.

## Compiler findings

An actual SDK configure using devkitA64 GCC 15.2.0 fails at the SDK's Clang-only
guard. Removing that guard alone would not establish compatibility:
`-ffp-model=strict`, generated Clang attributes and builtins also need attention.
The desktop requirements have not been relaxed.

[switch-clang.cmake](../tools/cross/switch-clang.cmake) uses host Clang 19 with
`--target=aarch64-none-elf`, devkitA64's newlib/libstdc++ headers, and the official
NintendoSwitch CMake platform. Linking uses devkitA64 g++ and `switch.specs`,
preserving libnx's startup objects and NRO linker layout. This is a real Clang
compile followed by a GCC-driver link, not a wrapper pretending GCC is Clang.

Compiler evidence during bring-up:

1. Clang rejects `-mtp=soft` with `invalid thread pointer reading mode`.
2. Bare-metal Clang does not discover devkitA64's C++ headers automatically;
   explicit target include paths are necessary (`array` was initially missing).
3. The experimental toolchain uses `-femulated-tls`; devkitA64's `libgcc.a`
   contains `__emutls_get_address`. The C++23 jthread/atomic/thread_local probe
   passed the supplied Horizon hardware run. This confirms the diagnostic's
   jthread/atomic/TLS behavior, not all future SDK cross-library TLS uses.
4. The actual patched SDK `include/rex/platform.h` compiles into a platform
   NRO with Horizon/ARM64/LE64 assertions and desktop flags asserted false.

The SDK platform patch deliberately refuses a full Switch runtime configure.
`REXGLUE_SWITCH_PLATFORM_PROBE_ONLY=ON` builds only the header contract. There
are no fake memory/thread/GPU implementations behind that target.

## Platform-condition audit

[SWITCH_PLATFORM_AUDIT.tsv](SWITCH_PLATFORM_AUDIT.tsv) indexes matching lines
and the required treatment by source file. The accompanying generator is
[audit_platform.py](../tools/switch/audit_platform.py). It covers the requested
WIN32, APPLE, linux, UNIX, POSIX, dlopen, mmap, pthread, SDL, X11 and Wayland
patterns in first-party SDK and Project8 source/build files. It is an inventory
for migration review, not evidence those code paths compile for Switch.
Uninitialized third-party submodule internals are not covered; their complete
platform audit remains a blocker before runtime enablement.

The important branch rules are:

- `WIN32`/`APPLE`: keep existing desktop branches; insert an explicit Switch
  case before generic `else()` branches that currently mean Linux.
- `__linux__`, `__gnu_linux__`, `REX_PLATFORM_LINUX`: remain false on Horizon.
- `UNIX`/`POSIX`: not a platform capability guarantee. Official devkitPro
  `CMAKE_SYSTEM_NAME=NintendoSwitch` must not activate `pthread rt dl`, X11,
  Wayland, ELF desktop loader, `/proc`, shm_open or mmap code by inference.
- `dlopen`: static GPU factory and ICD lookup, after hardware gates.
- `mmap`: real SVC-backed semantics or an identified blocking failure;
  file buffering may replace read-only file maps, but not guest aliases.
- `pthread`: basic thread creation is insufficient for guest suspend/APC,
  scheduling, robust mutex and signal handling requirements.
- `SDL`: audit the specific Switch SDL3 integration or implement isolated
  libnx adapters; do not assume SDL2 portlibs satisfy SDL3 APIs.
- X11/Wayland: excluded; VI/NWindow gets its own surface type and functions.

## Remaining gates

1. Preserve the hardware-confirmed C++23/TLS and SharedMemory alias results.
   The failed SharedMem permission change does not invalidate alias coherence.
2. Establish recoverable faults and scalable backing/protection semantics;
   the protection audit identifies actual correctness dependencies.
3. Preserve the passed upstream smoke and corrected Vulkan/combined probe
   results: device enumeration, required-feature enablement and 300 presentations
   per probe passed, with visible triangles and clean return confirmed by the
   user. Real Xenos rendering and optional-feature fallback behavior remain
   untested. The combined test does not gate Vulkan on reprotection.
4. Resolve the distribution-policy conflict: both inspected NVK Switch ports
   declare their new code GPL-2.0-or-later. nxvk statically links its driver.
   This repository's current policy forbids GPL-linked shipped binaries.
   Local probe evaluation does not change that policy, and no driver-linked
   release or game binary is being published.
5. Complete third-party platform audit/cross builds (SDL3, FFmpeg, SPIR-V tools,
   logging/config libraries, SIMD and other runtime dependencies).
6. Only then implement isolated Horizon memory/thread/exception/file backends,
   VI surface + static Vulkan entrypoint lookup, and static GPU registration.
7. Build the real `rexglue-switch-test.nro`, initialize memory/provider/Xenos,
   present a blank frame and verify shutdown on hardware. **This target is not
   implemented yet; the platform-header NRO is not a runtime NRO.**
8. Game execution is outside the current milestone. No game code was generated.

Build and device handoff commands are in [switch_probe/README.md](../switch_probe/README.md).

## Build verification on this host

| Check | Result |
|---|---|
| devkitA64 GCC C++23 memory and thread probes | Built and linked NROs |
| Clang 19 C++23 memory, jthread/TLS and SDK platform-header probes | Built and linked NROs |
| Pinned nxvk native tools, Rust sysroot, driver/support archives | Built successfully after Rust download transport retry |
| Vulkan triangle and combined memory/Vulkan probes | Built and linked NROs with GCC and Clang clients |
| Diagnostic vertex/fragment SPIR-V | `spirv-val` passed |
| Combined ELF | readelf reports ELF64, little endian, AArch64 |
| SDK patch sequence | All three patches apply to a pristine v0.10.0 source archive |
| Repository gates | No-game-content and patch-series checks passed; git diff whitespace check passed |
| Full Switch SDK configure | Intentionally fails before desktop backend selection |
| Launcher configure | Host lacks CMake; container attempt stops at missing SDL3 package. Full launcher CTest unavailable |
| Existing launcher settings tests | Compiled natively with Clang 19; all assertions passed |
| NOTICE regeneration | Attempted; fails on missing SDK dependency license files in the minimal checkout (including Snappy/volk paths). Existing NOTICE preserved; no release staged |

No Windows or macOS testing is claimed. Switch hardware findings above come
from the user's supplied logs and confirmation of visible triangles and clean
return in both corrected Vulkan probes. The new probes and toolchains are separate from desktop builds.
