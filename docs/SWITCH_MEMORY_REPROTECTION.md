# Guest page protection after the first Switch hardware run

The supplied title-takeover logs establish **ALIASING_PASS** and
**REPROTECTION_UNSUPPORTED_WITH_SHAREDMEM**. These are separate findings.
The 4.5 GiB virtual reservation, five physical aliases, two image aliases and
write/read coherence passed. All requested R/None/RW changes returned `0xD401`;
`svcQueryMemory` continued to report state `0x6` (SharedMem), permission `3`
(RW). Unmapping and closing the handles succeeded. The original summary was
incorrectly reporting the combined result as a generic memory failure.

This proves the small shared-backing alias model on the tested loader. It does
not prove scalable allocation or host fault recovery. The shared-memory mapping
implementation remains intact; its result reporting is now separated from
reprotection and cleanup. The combined probe proceeds to Vulkan when aliases
and cleanup pass, even when reprotection is unsupported.

## Source audit

Sources are ReXGlue `f5337cdc947ff6d4c4196737e2c807a48f2a1fc2` with this
repository's [runtime overlay](../patches/rexglue-sdk/0001-thp8-runtime-stack.patch).
Paths and function names below identify the patched implementation; upstream
links alone do not include the overlay.

| Use | Exact source/function | Classification and consequence |
|---|---|---|
| Initial allocation / commitment | [xmemory.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/system/xmemory.cpp): `Memory::Initialize`, `BaseHeap::AllocFixed`, `BaseHeap::AllocRange`; [memory_posix.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/core/memory_posix.cpp): `AllocFixed` | Required allocation semantics, not a write-watch optimization. Desktop commit over an existing reservation changes permissions. A Horizon implementation could initially map backing with the requested access; the failed SetMemoryPermission test alone does not prove commitment impossible. |
| Loaded image sections | [xex_module.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/system/xex_module.cpp): `XexModule::LoadContinue`, section-protection loop before `PopulateBinaryData` | Pre-entry bootstrap requests R for code/readonly data and RW for data. Calls to `heap->Protect` ignore their return. Thus this loop does not itself abort loading on failed reprotection, but intended protection and heap metadata cannot be claimed correct. No evidence establishes that Project 8 tolerates this. |
| Guest protection syscalls | [xboxkrnl_memory.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/kernel/xboxkrnl/xboxkrnl_memory.cpp): `NtProtectVirtualMemory`, `NtAllocateVirtualMemory`, `MmSetAddressProtect`; `BaseHeap::Protect`, `PhysicalHeap::Protect` | Guest-visible semantics. `BaseHeap::Protect` fails before updating metadata when host protection fails for guest pages at least as large as host pages; `NtProtectVirtualMemory` returns ACCESS_DENIED. Physical protection updates parent and guest views. Which calls Project 8 executes during bootstrap is unknown without a trace. |
| GPU MMIO registration | [graphics_system.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/graphics/graphics_system.cpp): `GraphicsSystem::SetupGuestGpu`; `Memory::AddVirtualMappedRange`; [mmio_handler.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/system/mmio_handler.cpp): `ExceptionCallback` | Runtime initialization requests no-access at guest `0x7FC80000`, rounded to host pages. `AddVirtualMappedRange` returns false before registering callbacks if the commit/protect fails. Its caller does not check that return. This is a correctness dependency of the current setup, not proof of a guaranteed startup abort. |
| CPU writes invalidating GPU data | [shared_memory.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/graphics/shared_memory.cpp): `MakeRangeValid`, `RangeWrittenByGpu`, `FireWatches`; `Memory::EnablePhysicalMemoryAccessCallbacks`, `PhysicalHeap::EnableAccessCallbacks`, `TriggerCallbacks` | Required for the existing cached rendering path. R protection on A/C/E views causes CPU stores to fault, invalidate cached ranges, then restore access. The arming code ignores host Protect failure, so accepting a no-op would silently lose invalidations. The overlay explicitly states that `gpu_watch_rearm_oracle` produces stale geometry; it is not a correctness-preserving fallback. Data-provider/no-access watches are TODO/asserted, not an additional implemented requirement. |
| Texture / primitive caches | [pipeline/texture/cache.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/graphics/pipeline/texture/cache.cpp): `WatchMemoryRange` call sites; [primitive_processor.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/graphics/primitive_processor.cpp): `EnablePhysicalMemoryAccessCallbacks` call | Consumers of write invalidation. Disabling watches globally is not justified by a blank triangle or successful CPU startup. |
| Release/decommit | `BaseHeap::Release`, `Decommit`, `SyncHostPageAccess` in xmemory.cpp | `protect_on_release` defaults false: optional diagnostic protection on release. The desktop decommit syscall is commented out; metadata is changed. `SyncHostPageAccess` returns immediately when guest pages are at least host-page sized, so do not claim every decommit requires host reprotection on 4 KiB Horizon. |
| Larger-host-page reconciliation | `BaseHeap::SyncHostPageAccess` | Applies when guest pages are smaller than host pages, preserving access/watch semantics over shared host pages. Not the 4 KiB Horizon case under test. |
| Save/restore | `BaseHeap::Save`, `Restore` | Temporarily requests RW and restores access around state serialization. Required for that feature, not evidence of a normal-bootstrap dependency. |
| Recovering watched faults | `MMIOHandler::ExceptionCallback`, `Memory::AccessViolationCallback`, `PhysicalHeap::TriggerCallbacks` | Requires a recoverable exception context, callback execution, RW restoration and retry. No such Horizon path has been implemented or tested. Aliasing alone cannot establish it. |

The generated-code [pch_h.inja](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/resources/templates/codegen/pch_h.inja)
uses direct base-relative loads/stores. Its `REX_MM_*` variants explicitly call
MMIO handlers, but [builders/memory.cpp](https://github.com/rexglue/rexglue-sdk/blob/f5337cdc947ff6d4c4196737e2c807a48f2a1fc2/src/codegen/builders/memory.cpp)
selects them using MMIO analysis/options; plain stores also exist. This is not
proof that all guest accesses can safely run with every page RW. No game-generated
code was produced or inspected for this audit. `ToPageAccess` and compiled
function dispatch do not require executable guest backing or a JIT.

**Conclusion:** reprotection is requested during runtime/image bootstrap and is
required for correctness of the existing fault-driven GPU cache invalidation.
Some uses are optional, and the image loader does not enforce failure, but
there is no evidence for declaring it unnecessary during Project 8 bootstrap.
The current SharedMem permission operation is unsuitable; the working aliases
are still useful and must be retained. No runtime protection call was stubbed.

## Narrow alternative investigation

Because real correctness dependencies exist, the installed libnx
`switch/kernel/svc.h` declarations were checked:

- `svcUnmapSharedMemory` (0x14) and `svcMapSharedMemory` (0x13) expose unmapping
  and a permission argument on mapping. A **separate future experiment** could
  test unmap/remap of a tiny view as R then RW while retaining another alias.
  The signature does not guarantee that the handle's owner permissions permit
  different access, that a subrange can be mapped, or that such transitions are
  safe around concurrent faulting threads. No-access might require leaving a
  reserved view unmapped. None of these semantics have been tested or claimed.
- `svcMapMemory` (0x04) explicitly makes the source inaccessible, so it is not
  a replacement for simultaneous accessible aliases.
- `svcMapProcessMemory` (0x74) is privileged and documented for RW mappings of
  another process's memory. Its declaration does not establish same-process
  aliasing or mutable per-view permissions. Availability/state constraints and
  fault recovery remain unknown; no speculative backend was implemented.

The corrected hardware handoff passed NXVK presentation and retained the proven
SharedMemory alias result. A protection experiment must independently prove
mapping permissions, coherence, safe transition/retry and scalability before
being considered a ReXGlue backend.
