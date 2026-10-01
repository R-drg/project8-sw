# Switch Vulkan capabilities

**Hardware result: corrected Vulkan probes pass on Tegra X1.** The supplied
standalone and combined logs identify `NVIDIA Tegra X1 (NVK gm20b)`, API
`1.4.354`, and each records 300 successful presentations with required features
enabled. The user confirmed a visible colored triangle and clean return to
Homebrew Menu for both probes. The unchanged upstream smoke verified all 1,024
buffer words and completed teardown. `nvk_mesa.log` was empty.

Shared-memory aliases and cleanup pass; SharedMem reprotection still returns
`0xD401` with RW unchanged. This establishes diagnostic GPU execution and VI
presentation, not ReXGlue or game correctness. Firmware/CFW/hbmenu versions and
docked/handheld mode were not supplied for this run.

## Driver selected for local evaluation

nxvk at `e028d428c3b1f2dabadd81645c53cddfc8d7ab51` was selected because its
[Makefile](https://github.com/PalindromicBreadLoaf/nxvk/blob/e028d428c3b1f2dabadd81645c53cddfc8d7ab51/Makefile)
builds named static driver/support archives and stages a portlib package, and
its [VI example](https://github.com/PalindromicBreadLoaf/nxvk/blob/e028d428c3b1f2dabadd81645c53cddfc8d7ab51/switch/smoke/nvk_vi_swapchain.c)
uses `VK_NN_vi_surface` with `VkViSurfaceCreateInfoNN::window = nwindowGetDefault()`.
Applications resolve functions through `vk_icdGetInstanceProcAddr`, with no
desktop Vulkan loader.

The other inspected implementation, switch-nvk at
`6eec707da3ad5f86c64f748226583202801bfd03`, also has an
[NWindow WSI implementation](https://github.com/HayatoG/switch-nvk/blob/6eec707da3ad5f86c64f748226583202801bfd03/winsys/wsi/wsi_common_switch.c).
That alone does not establish compatibility with ReXGlue. No performance or
feature ranking is inferred from README claims.

The selected nxvk driver and support archives were actually built with the
repository's Docker build, including native mesa_clc/vtn_bindgen2, its Horizon
Rust standard library, and 1,230 cross-build steps. The first Docker attempt
failed downloading Rust; setting `RUSTUP_USE_CURL=1` in the temporary Dockerfile
allowed the same pinned Rust toolchain to download. No driver code was changed.

The standalone Vulkan probe linked with those archives, and its own GLSL vertex
and fragment shaders compiled to SPIR-V and were packaged into its NRO RomFS.
The supplied hardware run additionally confirms execution of the diagnostic triangle.

The upstream `switch/smoke/nvk_compat.c` is linked explicitly. It includes
temporary POSIX compatibility stubs (for example regex functions that do not
perform matching and a fixed physical-memory report). They are inherited
experimental driver scaffolding, not implementations to copy into ReXGlue.
The memory probe uses actual `svcGetInfo`, not that `sysconf` memory estimate.

Both ports declare new Switch code GPL-2.0-or-later. These local driver-linked
experiments are not release artifacts and do not satisfy the repository's
current no-GPL-linked-shipping policy. Root NOTICE generation does not cover
NVK or libnx; distribution needs a deliberate dependency/licensing review.

## Actual-result matrix

| Requirement | Driver result | Probe behavior |
|---|---|---|
| `independentBlend` | reported true; enabled | Logs VkPhysicalDeviceFeatures value; refuses device creation if false; enables it if true |
| `fragmentStoresAndAtomics` | reported true; enabled | Same mandatory gate |
| `vertexPipelineStoresAndAtomics` | reported true; enabled | Same mandatory gate |
| `VK_KHR_swapchain` | exposed; swapchain presentation passed | Requires extension, enables it, creates/acquires/presents swapchain |
| `VK_KHR_surface`, `VK_NN_vi_surface` | exposed; VI surface creation passed | Requires both instance extensions; creates real VI surface |
| `geometryShader` | reported true | Logs feature; not required by default probe |
| `fillModeNonSolid` | reported true | Logs feature; not required by default probe |
| `VK_KHR_dynamic_rendering` | exposed; dynamicRendering=1 | Logs extension and dynamicRendering feature; also queries core 1.3 path |
| `VK_EXT_fragment_shader_interlock` | not exposed; all three interlock bits=0 | Logs extension and pixel/sample/shading-rate interlock bits |
| `VK_EXT_shader_stencil_export` | not exposed | Logs extension |
| `VK_KHR_image_format_list` | exposed | Logs extension; API version is available to assess promotion to core |
| `VK_KHR_sampler_ycbcr_conversion` | exposed; ycbcr=1 | Logs extension and samplerYcbcrConversion bit, including core 1.1 |
| `VK_EXT_custom_border_color` | exposed; both feature bits=1 | Logs extension and both custom-border-color feature bits |
| `VK_EXT_robustness2` | exposed; all three feature bits=1 | Logs extension, robustBufferAccess2, robustImageAccess2 and nullDescriptor |
| `VK_KHR_spirv_1_4` | exposed; API 1.4.354 | Logs extension and device API version; triangle uses basic SPIR-V |
| Texture / render-target formats | queried in both logs; per-format runtime compatibility not established | Queries all format identifiers collected from pinned ReXGlue texture_cache.cpp/render_target_cache.cpp; logs linear/optimal/buffer format flags and sampled/color/depth/storage image-query results |
| MSAA | global masks 0xf (1/2/4/8 samples); rendering tested at one sample | Logs framebuffer/sample/storage count masks and per-format image sample counts |
| Device, queue and VI presentation | passed; 300 successful presentations per probe | Requires a graphics queue supporting the VI surface, creates swapchain, draws/presents 300 frames |
| Triangle visible on screen | user confirmed for both probes; clean return to Homebrew Menu | Human confirmation required; successful QueuePresent alone is insufficient |

ReXGlue's `vulkan_require_geometry_shader` and
`vulkan_require_fill_mode_non_solid` default to `!REX_PLATFORM_MAC` in the pinned
`vulkan_device.cpp`. An eventual Switch branch must deliberately choose and
test the existing fallback settings if either feature is absent; the probe's
three-bit minimum gate does not prove the SDK's current default configuration
will accept that device.

## Logs and interpretation

[vulkan_probe/main.cpp](../switch_probe/vulkan_probe/main.cpp) enumerates all
instance/device extensions, prints all `VkPhysicalDeviceFeatures` fields, device
identity/API/driver versions, every named limit and sparse property, MSAA limits and the full returned
`VkPhysicalDeviceProperties` structure as hex bytes. All queried results go to
`sdmc:/switch/project8-vulkan-probe.log`. No feature values are hardcoded as
supported. Negative image-format results are logged as results, not hidden.

The combined probe first checks aliases and safe cleanup, then performs the
same Vulkan test. Unsupported SharedMem reprotection is reported separately
and does not skip Vulkan. This is not a complete runtime-memory pass. Its log is
`sdmc:/switch/project8-feasibility-probe.log`.

The probe enables the three required core feature bits, but a triangle does
not exercise all their behavior. A successful result establishes only early
device/presentation feasibility. Real Xenos shaders, EDRAM emulation, barriers,
texture conversions, GPU memory budgets and sustained rendering still require
later tests. In particular, extension presence and a claimed Vulkan API version
do not establish correct implementations of those operations.

Use [the build and handoff commands](../switch_probe/README.md), run in full
application mode/title takeover, and record Horizon/CFW/hbmenu versions, console
model, docked/handheld mode, returned logs and whether the triangle was visible.
Only replace “not measured” entries with actual device observations.

## NXVK bring-up correction and upstream control

The reference is the pinned `switch/smoke/nvk_harness.h`, functions
`nvk_bringup` and `nvk_bringup_ex`, used by unchanged `nvk_smoke.c`.
The corrected client matches its application/automatic-heap globals,
`NVK_I_WANT_A_BROKEN_VULKAN_DRIVER=1` set before ICD use, Vulkan 1.3 instance
request, direct ICD function lookup, and debug-utils callback during and after
instance creation. It retains the diagnostic's VI surface and mandatory-feature
checks; the upstream smoke remains a separate control with its own entrypoint.

In `src/nouveau/vulkan/nvk_physical_device.c::nvk_physical_device_create`, NXVK
rejects nonconformant devices with `VK_ERROR_INCOMPATIBLE_DRIVER` unless that
environment option is enabled. This supplies a concrete source explanation for
why the omission can prevent enumeration. The corrected hardware run now
enumerates one device; this supports the diagnosis, although the corrected
client changed multiple bring-up settings together.

The new logs print the environment-setting result and value, configured and
actual applet types, automatic heap setting, process memory accounting, resolved
ICD entrypoint addresses, and each physical enumeration call's VkResult/count.
Driver debug messages are flushed into the same per-probe log. Zero devices
remain an explicit failure, not a feature-support result.

The unchanged upstream smoke was built using
`bash switch/build/build-nro.sh nvk_smoke` in the NXVK build container.
The smoke source, harness, compatibility/chain sources and build script have
no diff against the pinned commit. It tests a GPU buffer fill/readback, not VI
presentation or the three ReXGlue feature requirements. Hardware readback and
teardown passed in the supplied log. For retests, run it first and retain `sdmc:/nvk_smoke.log` and `sdmc:/nvk_mesa.log`;
then run corrected `vulkan_probe.nro`, then `switch_feasibility_probe.nro`.
The upstream smoke can return exit code zero after a failure, so inspect its
log for successful readback and `nvk_smoke PASSED`.
