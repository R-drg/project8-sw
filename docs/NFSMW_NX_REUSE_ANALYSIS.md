# nfsmw-nx ReXGlue SDK reuse analysis

Sources: [nfsmw-nx porting guide](https://github.com/StevensND/nfsmw-nx/blob/a4232df8e3888a1af96659dcf911f4915fca04aa/docs/porting-another-game.md), [platform notes](https://github.com/StevensND/nfsmw-nx/blob/a4232df8e3888a1af96659dcf911f4915fca04aa/docs/platform-notes.md), [SDK licensing](https://github.com/StevensND/nfsmw-nx/blob/a4232df8e3888a1af96659dcf911f4915fca04aa/THIRD_PARTY_NOTICES.md). Compared nfsmw-nx `a4232df8` with upstream ReXGlue `c94f5ebd` (the v0.10.0 tag; source tree identical to Project8 checkout `f5337cdc`) and Project8 patches `0001`–`0003`.

The porting guide calls the Horizon SDK reusable; its platform notes detail the 4.5 GiB guest window, code-memory shadow, lazy commits, ProcessMem permission limitation, eight exception slots, and thread scheduling. Those are implementation evidence from nfsmw-nx hardware, not verification on Project8 hardware.

A = generic Horizon layer to reuse; B = generic ReXGlue improvement; C = NFSMW policy or instrumentation to exclude; D = overlap with Project8 patch requiring a merge; E = third-party licensing-sensitive. D and E take precedence in the inventory below; some rows also contain A/B work, described in the note. “Merged” means included in the local SDK integration patch, not hardware verified.

| SDK path | Class | Disposition and reason |
|---|:---:|---|
| `CMakeLists.txt` | A | merged; Generic Horizon adapter or build wiring |
| `README.upstream.md` | C | defer; NFSMW upstream attribution copy |
| `cmake/rexglue_helpers.cmake` | A | merged; Generic Horizon adapter or build wiring |
| `cmake/rexglue_switch.cmake` | A | added; Generic Horizon adapter or build wiring |
| `include/rex/audio/audio_system.h` | C | defer; NFSMW audio callback hook |
| `include/rex/audio/sdl/sdl_audio_driver.h` | C | defer; NFSMW SDL diagnostic changes |
| `include/rex/audio/switch/switch_audio_system.h` | A | added; Generic Horizon adapter or build wiring |
| `include/rex/audio/xma/context.h` | C | defer; NFSMW audio profiling |
| `include/rex/filesystem/devices/host_path_device.h` | A | defer; Generic Horizon adapter or build wiring |
| `include/rex/filesystem/devices/host_path_entry.h` | A | defer; Generic Horizon adapter or build wiring |
| `include/rex/filesystem/devices/host_path_file.h` | C | defer; NFSMW streaming cache declarations |
| `include/rex/filesystem/devices/stfs_xbox.h` | A | merged; Generic Horizon adapter or build wiring |
| `include/rex/filesystem/file.h` | A | defer; Generic Horizon adapter or build wiring |
| `include/rex/graphics/primitive_processor.h` | D | defer; Project8 primitive path differs; defer. |
| `include/rex/input/input_system.h` | B | defer; input enumeration throttling |
| `include/rex/input/switch/switch_input_driver.h` | A | added; Generic Horizon adapter or build wiring |
| `include/rex/logging/types.h` | C | defer; NFSMW logging flush policy |
| `include/rex/memory/utils.h` | A | merged; Generic Horizon adapter or build wiring |
| `include/rex/platform/dynlib.h` | A | merged; Generic Horizon adapter or build wiring |
| `include/rex/platform.h` | A | merged; Generic Horizon adapter or build wiring |
| `include/rex/system/xfile.h` | C | defer; NFSMW WMV movie tracking |
| `include/rex/thread/fiber.h` | A | merged; Generic Horizon adapter or build wiring |
| `include/rex/ui/overlay/debug_overlay.h` | C | defer; NFSMW overlay position |
| `include/rex/ui/overlay/settings_overlay.h` | C | defer; NFSMW menu filtering |
| `include/rex/ui/presenter.h` | D | defer; NFSMW presentation queue differs from Project8 pacing; defer. |
| `include/rex/ui/renderdoc_api.h` | A | merged; Generic Horizon adapter or build wiring |
| `include/rex/ui/surface.h` | A | merged; Generic Horizon adapter or build wiring |
| `include/rex/ui/surface_switch.h` | A | added; Generic Horizon adapter or build wiring |
| `include/rex/ui/switch_apm.h` | C | defer; NFSMW clock controls |
| `include/rex/ui/switch_imgui_input.h` | A | added; Generic Horizon adapter or build wiring |
| `include/rex/ui/switch_saltynx.h` | A | added; Generic Horizon adapter or build wiring |
| `include/rex/ui/switch_sysclk.h` | C | defer; NFSMW clock controls |
| `include/rex/ui/vulkan/api.h` | A | merged; Generic Horizon adapter or build wiring |
| `include/rex/ui/vulkan/device.h` | C | defer; NFSMW native-renderer Vulkan features |
| `include/rex/ui/vulkan/functions/instance_nn_vi_surface.inc` | A | added; Generic Horizon adapter or build wiring |
| `include/rex/ui/vulkan/instance.h` | A | merged; Generic Horizon adapter or build wiring |
| `include/rex/ui/vulkan/presenter.h` | D | defer; NFSMW speculative acquire path is tuned for its renderer; defer. |
| `include/rex/ui/window_switch.h` | A | added; Generic Horizon adapter or build wiring |
| `include/rex/ui/windowed_app_context_switch.h` | A | added; Generic Horizon adapter or build wiring |
| `resources/templates/codegen/pch_h.inja` | B | merged; GCC generated-code compatibility merged |
| `resources/templates/init/rexglue_cmake.inja` | A | defer; Generic Horizon adapter or build wiring |
| `src/audio/CMakeLists.txt` | A | merged; Generic Horizon adapter or build wiring |
| `src/audio/audio_system.cpp` | C | defer; NFSMW audio hook and diagnostics |
| `src/audio/sdl/sdl_audio_driver.cpp` | C | defer; NFSMW SDL diagnostic changes |
| `src/audio/switch/switch_audio_system.cpp` | A | added; Generic Horizon adapter or build wiring |
| `src/audio/xma_context.cpp` | C | defer; NFSMW audio profiling and behavior |
| `src/audio/xma_decoder.cpp` | C | defer; NFSMW decoder tuning |
| `src/codegen/builders/builder_context.h` | B | defer; share_registers support; useful after first frame |
| `src/codegen/builders/context.cpp` | B | defer; share_registers support; useful after first frame |
| `src/codegen/builders/control_flow.cpp` | B | defer; share_registers support; useful after first frame |
| `src/core/CMakeLists.txt` | A | merged; Generic Horizon adapter or build wiring |
| `src/core/clock_posix.cpp` | A | merged; Generic Horizon adapter or build wiring |
| `src/core/dynlib_switch.cpp` | A | added; Generic Horizon adapter or build wiring |
| `src/core/exception_handler_switch.cpp` | A | added; Generic Horizon adapter or build wiring |
| `src/core/fiber_switch.cpp` | A | added; Generic Horizon adapter or build wiring |
| `src/core/filesystem_posix.cpp` | B | merged; generic Switch sdmc path handling merged |
| `src/core/guest_memory_switch.cpp` | A | added; Generic Horizon adapter or build wiring |
| `src/core/guest_memory_switch.h` | A | added; Generic Horizon adapter or build wiring |
| `src/core/logging.cpp` | C | defer; NFSMW SD logging settings and experimental async path |
| `src/core/mapped_memory_switch.cpp` | A | added; Generic Horizon adapter or build wiring |
| `src/core/memory_switch.cpp` | A | added; Generic Horizon adapter or build wiring |
| `src/core/resume_ctx.h` | A | added; Generic Horizon adapter or build wiring |
| `src/core/rex_decode.h` | A | added; Generic Horizon adapter or build wiring |
| `src/core/seh_switch.cpp` | A | added; Generic Horizon adapter or build wiring |
| `src/core/switch_libc_supplement.c` | A | added; Generic Horizon adapter or build wiring |
| `src/core/threading_posix.cpp` | D | CONFLICT; Project8 already fixes short waits; retained finer native duration and merged Horizon suspension. |
| `src/core/threading_switch.cpp` | A | added; Generic Horizon adapter or build wiring |
| `src/core/threading_switch.h` | A | added; Generic Horizon adapter or build wiring |
| `src/core/timer_queue.cpp` | D | defer; Project8 already changed timer waits; defer replacement. |
| `src/filesystem/devices/host_path_device.cpp` | A | defer; Generic Horizon adapter or build wiring |
| `src/filesystem/devices/host_path_entry.cpp` | A | defer; Generic Horizon adapter or build wiring |
| `src/filesystem/devices/host_path_file.cpp` | C | defer; NFSMW streaming caches and SD heuristics |
| `src/filesystem/virtual_file_system.cpp` | A | defer; Generic Horizon adapter or build wiring |
| `src/graphics/CMakeLists.txt` | A | merged; Generic Horizon adapter or build wiring |
| `src/graphics/command_processor.cpp` | D | defer; Project8 pacing changes overlap NFSMW ring pacing; defer. |
| `src/graphics/primitive_processor.cpp` | A | defer; Generic Horizon adapter or build wiring |
| `src/graphics/vulkan/command_processor.cpp` | D | defer; Project8 GPU patch overlaps native-renderer callbacks and counters; deferred. |
| `src/graphics/vulkan/pipeline_cache.cpp` | D | defer; Project8 renderer cache changes; defer NFSMW tuning. |
| `src/graphics/vulkan/primitive_processor.cpp` | D | defer; Project8 native primitive path differs; defer. |
| `src/graphics/vulkan/render_target_cache.cpp` | D | defer; Project8 renderer changes; defer. |
| `src/graphics/vulkan/shared_memory.cpp` | D | defer; Project8 write-watch changes; NFSMW changes require separate reconciliation. |
| `src/graphics/vulkan/texture_cache.cpp` | D | defer; Project8 texture cache changes; defer NFSMW tuning. |
| `src/input/CMakeLists.txt` | A | merged; Generic Horizon adapter or build wiring |
| `src/input/input_system.cpp` | B | merged; input enumeration throttling useful, excluded until hardware input test |
| `src/input/switch/switch_input_driver.cpp` | A | added; Generic Horizon adapter or build wiring |
| `src/kernel/xam/xam_content.cpp` | C | defer; NFSMW save path policy |
| `src/kernel/xboxkrnl/xboxkrnl_audio.cpp` | A | defer; Generic Horizon adapter or build wiring |
| `src/kernel/xboxkrnl/xboxkrnl_audio_xma.cpp` | A | defer; Generic Horizon adapter or build wiring |
| `src/kernel/xboxkrnl/xboxkrnl_io.cpp` | C | defer; NFSMW WMV tracking and streaming policy |
| `src/system/CMakeLists.txt` | A | merged; Generic Horizon adapter or build wiring |
| `src/system/gpu_plugin_loader.cpp` | A | merged; Generic Horizon adapter or build wiring |
| `src/system/xam/content_manager.cpp` | C | defer; NFSMW save container policy |
| `src/system/xmemory.cpp` | D | merged; Project8 guest heap patch overlaps; merged reservation and lazy physical allocation, omitted NFSMW read-only bypass. |
| `src/system/xthread.cpp` | D | merged; Merged Horizon scheduling into Project8 thread implementation. |
| `src/ui/CMakeLists.txt` | A | merged; Generic Horizon adapter or build wiring |
| `src/ui/imgui_drawer.cpp` | A | merged; Generic Horizon adapter or build wiring |
| `src/ui/overlay/debug_overlay.cpp` | C | defer; NFSMW overlay position |
| `src/ui/overlay/settings_overlay.cpp` | C | defer; NFSMW menu cvar hiding |
| `src/ui/presenter.cpp` | D | defer; NFSMW presentation queue differs from Project8 pacing; defer. |
| `src/ui/renderdoc_api.cpp` | A | merged; Generic Horizon adapter or build wiring |
| `src/ui/rex_app.cpp` | A | merged; Generic Horizon adapter or build wiring |
| `src/ui/runtime_switch.cpp` | A | added; Generic Horizon adapter or build wiring |
| `src/ui/switch_apm.cpp` | C | defer; NFSMW clock configuration and tuning |
| `src/ui/switch_crash_hooks.c` | A | added; Generic Horizon adapter or build wiring |
| `src/ui/switch_elf_stubs.c` | A | added; Generic Horizon adapter or build wiring |
| `src/ui/switch_perf.cpp` | C | defer; NFSMW profiler; Horizon adapter should not depend on it |
| `src/ui/switch_saltynx.cpp` | A | added; Generic Horizon adapter or build wiring |
| `src/ui/switch_sysclk.cpp` | C | defer; NFSMW clock controls |
| `src/ui/vulkan/vulkan_device.cpp` | D | defer; NFSMW native-renderer feature requests are irrelevant to Xenos first-frame path. |
| `src/ui/vulkan/vulkan_instance.cpp` | A | merged; Generic Horizon adapter or build wiring |
| `src/ui/vulkan/vulkan_presenter.cpp` | D | merged; Project8 Vulkan pacing patch overlaps; only VI surface and format changes selected. |
| `src/ui/window_switch.cpp` | A | added; Generic Horizon adapter or build wiring |
| `src/ui/windowed_app_context_switch.cpp` | A | added; Generic Horizon adapter or build wiring |
| `src/ui/windowed_app_main_switch.cpp` | A | added; Generic Horizon adapter or build wiring |
| `switch_compat/endian.h` | A | added; Generic Horizon adapter or build wiring |
| `switch_compat/netinet/ip.h` | A | added; Generic Horizon adapter or build wiring |
| `thirdparty/CMakeLists.txt` | A | merged; Generic Horizon adapter or build wiring |
| `thirdparty/FFmpeg/config.h` | E | added; LGPL FFmpeg platform configuration; kept as build overlay |
| `thirdparty/FFmpeg/config_nfsmw_wmv3.h` | E | defer; LGPL WMV3 selection for NFSMW; excluded |
| `thirdparty/FFmpeg/config_switch_aarch64.h` | E | added; LGPL FFmpeg Horizon configuration; build overlay |
| `thirdparty/FFmpeg/libavcodec/fft_template.c` | E | defer; LGPL NFSMW transform cache; excluded |
| `thirdparty/FFmpeg/libavcodec/mdct_template.c` | E | defer; LGPL NFSMW transform cache; excluded |
| `thirdparty/ffmpeg-overlay/codec_list.c` | E | defer; LGPL WMV3 registry; excluded |

## Integration decisions

- Project8 runtime and MoltenVK patches remain first in `series`; the Horizon integration applies afterward. `src/core/threading_posix.cpp` keeps Project8’s finer wait timing. `src/system/xmemory.cpp` takes lazy physical allocation and the chosen guest base, but retains logical read-only semantics.
- The imported guest memory and exception adapter uses `virtmemAddReservation`, code-memory aliases, `svcMapProcessMemory`, logical permission metadata and unmapping of watched pages. Shadow reads and writes are handled by the exception path; it reserves separate stacks and dumps for concurrent faults. These mechanisms still require a Switch run for this integration.
- The input, audio and filesystem adapters are imported where needed to link the runtime. NFSMW-specific input throttling, profiler and debug-overlay shortcuts, audio tuning, filesystem caches, WMV hooks, game callbacks and native renderer are excluded. Static `rexgpu-xenos` registration remains the default graphics route.
- nfsmw-nx says its Horizon SDK additions are BSD-3-Clause under the SDK license. Its app is GPL-3.0 and is not imported. FFmpeg changes are LGPL-sensitive; the generic Switch config is kept as an overlay and the NFSMW WMV3 edits are excluded. The local pinned NXVK driver is GPL-2.0-or-later: test NROs linked against it are local diagnostics and must pass the repository GPL boundary review before any release.
- The local NVK adapter adds independent newlib compatibility functions for that pinned experimental driver. They are absent from nfsmw-nx's SDK diff and are included only when `REXGLUE_SWITCH_NVK_SOURCE` is set. Its disk cache is disabled because Horizon file locking is not implemented here.

## Build and current limit

The patched SDK configured and built with devkitA64 GCC 15.2 in Docker. `rexglue-switch-test.nro` linked the static Xenos Vulkan plugin and pinned NVK archive and was packaged as an NRO. The final artifact is staged at `/tmp/project8-switch-handoff/rexglue-switch-test.nro`. The build verifies symbols and cross-compilation, not hardware behavior.

The repository NOTICE generator was run against this SDK checkout but stops because its dependency list still requires Snappy and volk, which are absent from ReXGlue v0.10.0. Refreshing the release notice and reviewing the pinned NXVK GPL boundary remain release tasks; this driver-linked NRO is a local diagnostic.

The user's ISO passed the repository's exact disc identity check. Its 5,127 files were extracted locally to `/tmp/project8-owned-disc`; native ReXGlue codegen produced 237 ignored files under `generated/default`. The game target compiled those units and linked the same static SDK and Xenos plugin into `build/project8-switch-final/thps_p8.nro`. This is a build result, not a hardware boot or first frame. The Project8 app now looks for `game/` beside its NRO on Switch and runs the existing dump gate there.

The final game link exposed two NFSMW profiler/overlay symbols in the imported input driver. Those title-specific shortcuts were removed from the reusable SDK patch; the game then linked successfully. A small host-tool compatibility fallback for this container's older C++ standard library is also included. The refreshed four-patch series applies cleanly to a fresh upstream v0.10.0 checkout.

The first console launch stopped at the dump gate because the interrupted SD transfer had left `game/default.xex` at zero bytes. Its replacement was read back from the SD card with the supported hash, and all 5,127 game files then matched the extracted disc's total size. The next console launch reached Vulkan initialization but returned to Home because direct NVK supplies no loader-owned `vkEnumerateInstanceLayerProperties` entry point. The Switch-only static ICD path now reports zero instance layers in that case. The pinned experimental NVK path also sets its documented device-creation opt-in before runtime startup. A further console run created a Vulkan instance with `VK_NN_vi_surface` but crashed while NVK WSI queried physical-device properties. The driver uses weak entrypoint references and its own build script requires `libnvk.a` to be linked whole; the ReXGlue Switch link now does so. The corrected NRO was copied to the SD card and read back with a matching SHA-256. Its hardware result is pending.

The next hardware run created NVK's Tegra X1 device and a 1280×720 swapchain, mapped the guest arena, initialized the generated function table, and entered translated guest code. The screen stayed black and the console froze; the final log entry was an `NtCreateFile` failure for `GAME:\\data\\compressed\\`, a directory present on the extracted disc as `DATA/COMPRESSED`. The host-path VFS already compares names case-insensitively, but its POSIX directory enumeration classified directories using `dirent.d_type`. That field may be `DT_UNKNOWN` on the SD filesystem, preventing recursive indexing. The imported SDK now classifies each entry from the existing `stat` result and skips failed `stat` calls. The rebuilt NRO was copied to the SD card and read back with a matching SHA-256; this cause and fix remain unverified until the next hardware run.

Hardware results for the merged memory and exception path remain pending. The prior SharedMemory probe only verified aliasing and presentation; it does not verify this imported implementation.
