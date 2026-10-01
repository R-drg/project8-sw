# Switch feasibility probes

**Hardware: toolchain/TLS, tiny shared aliases and Vulkan diagnostics passed.**
Both corrected Vulkan probes presented 300 frames; the user confirmed visible
triangles and clean return to Homebrew Menu. The unchanged upstream smoke passed
GPU buffer readback. SharedMem reprotection remains unsupported by the tested
SVC. Use full application mode/title takeover. No game files are needed.

Run in this order: **upstream `nvk_smoke` → corrected `vulkan_probe` → corrected
`switch_feasibility_probe`**. The combined probe reports ALIASING_PASS and
REPROTECTION_UNSUPPORTED_WITH_SHAREDMEM separately and continues to Vulkan when
aliases/cleanup pass. It does not declare the full ReXGlue memory model solved.

## Build the probes

Run commands from the Project8Recomp repository root. Native devkitPro users
can run the same CMake commands without Docker, with DEVKITPRO and Clang 19
installed. The Docker commands use the existing devkitPro image and keep build
tools off the host.

```sh
docker build -t project8-switch-clang switch_probe
docker run --rm -v "$PWD:/work" -w /work project8-switch-clang \
  sh -c 'cmake -S switch_probe -B build/switch-clang -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/work/tools/cross/switch-clang.cmake \
    -DCMAKE_BUILD_TYPE=Release && cmake --build build/switch-clang'
```

Outputs: `build/switch-clang/memory_probe.nro` and `toolchain_probe.nro`.
The latter uses C++23, strict floating-point compiler settings, jthread,
atomics and thread-local storage. It displays/logs its result then exits after
300 console updates. The memory probe logs its results and waits for `+`.

An optional GCC-only diagnostic build (not the SDK compiler choice):

```sh
docker run --rm -v "$PWD:/project" -w /project devkitpro/devkita64:latest \
  sh -c 'cmake -S switch_probe -B build/switch-gcc -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/project/tools/cross/switch-devkita64.cmake \
    -DCMAKE_BUILD_TYPE=Release && cmake --build build/switch-gcc'
```

Build the pinned external NVK driver, then the Vulkan and combined probes:

```sh
bash tools/switch/build-nxvk.sh /tmp/p8-nxvk
docker run --rm -v "$PWD:/project" -v /tmp/p8-nxvk:/work -w /project \
  project8-switch-clang sh -c 'cmake -S switch_probe -B build/switch-nvk-clang -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/project/tools/cross/switch-clang.cmake \
    -DCMAKE_BUILD_TYPE=Release -DSWITCH_BUILD_VULKAN_PROBE=ON \
    -DNXVK_SOURCE=/work -DGLSLANG_VALIDATOR=/usr/bin/glslangValidator && \
    cmake --build build/switch-nvk-clang'
```

Outputs under `build/switch-nvk-clang/vulkan_probe/`:

- `vulkan_probe.nro`: device capabilities, feature gate, shader triangle, VI.
- `switch_feasibility_probe.nro`: aliases/cleanup first, then Vulkan;
  unsupported SharedMem reprotection does not skip Vulkan.

Build the upstream control without changing its sources, harness or script:

```sh
git -C /tmp/p8-nxvk diff --exit-code HEAD -- switch/smoke switch/build/build-nro.sh
docker run --rm -v /tmp/p8-nxvk:/work -w /work project8-nxvk-probe \
  bash switch/build/build-nro.sh nvk_smoke
```

Output: `/tmp/p8-nxvk/switch/smoke/out/nvk_smoke.nro`. This is the upstream
buffer-fill/readback smoke, not a triangle; inspect its logs for
`nvk_smoke PASSED`. Exit code alone is insufficient. If you previously used
NXVK's ladder runner, its unchanged chain helper honors `sdmc:/nxvk_run/state`;
use a clean control setup without stale runner state.

The driver is compiled by its own GCC/Rust toolchain; the client uses Clang
C++23 and the devkitA64 linker. Shaders are original diagnostic GLSL compiled
at build time and included in RomFS. No renderer was rewritten.

The NVK build helper clones source only if the directory is absent, checks the
pinned revision, builds its documented image and archives, and uses a temporary
Docker context for the Rust download workaround. It does not install NVK into
your host or modify the project's normal desktop dependency selection.

## Earlier ReXGlue platform contract probe

This limited header and C++23 check predates the merged Horizon runtime test
described below. Use the real `rexglue-switch-test.nro` for the SDK integration.

```sh
git clone --branch v0.10.0 --depth 1 https://github.com/rexglue/rexglue-sdk /tmp/p8-switch-rexglue
while IFS= read -r patch; do
  git -C /tmp/p8-switch-rexglue apply --check --index "$PWD/patches/rexglue-sdk/$patch"
  git -C /tmp/p8-switch-rexglue apply --index "$PWD/patches/rexglue-sdk/$patch"
done < patches/rexglue-sdk/series
docker run --rm -v "$PWD:/work" -v /tmp/p8-switch-rexglue:/sdk -w /work \
  project8-switch-clang sh -c 'cmake -S /sdk -B build/rex-switch-platform -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/work/tools/cross/switch-clang.cmake \
    -DCMAKE_BUILD_TYPE=Release -DREXGLUE_SWITCH_PLATFORM_PROBE_ONLY=ON && \
    cmake --build build/rex-switch-platform'
```

Use a fresh SDK directory for those clone/apply commands; the audit session's
existing `/tmp/p8-switch-rexglue` already contains the applied changes.
No SDK submodules are needed for this intentionally limited header target.
The platform-only CMake option checks headers without building the runtime.

## Copy and run

Set `SWITCH_SD` to the mounted SD card. These commands copy only original probe
executables; do not use the game release packager.

```sh
SWITCH_SD=/media/rodrigo/SWITCH
mkdir -p "$SWITCH_SD/switch/project8-probes"
cp /tmp/p8-nxvk/switch/smoke/out/nvk_smoke.nro \
   build/switch-nvk-clang/memory_probe.nro \
   build/switch-nvk-clang/toolchain_probe.nro \
   build/switch-nvk-clang/vulkan_probe/vulkan_probe.nro \
   build/switch-nvk-clang/vulkan_probe/switch_feasibility_probe.nro \
   "$SWITCH_SD/switch/project8-probes/"
sync
```

Safely unmount the SD card, put it back in the Switch, and open Homebrew Menu
through your configured title takeover. Launch `nvk_smoke`, then
`vulkan_probe`, then `switch_feasibility_probe`. Retain all logs, even on failure.
The standalone toolchain and memory probes are available for isolated retests.
Only the Vulkan probes are expected to display a triangle; the smoke is headless.

For the DBI MTP mount used in this session, the same copy commands work with:

```sh
SWITCH_SD='/run/user/1000/gvfs/mtp:host=-_DBI_XTW70195766340/1: SD Card'
```

The corrected binaries are also staged together in `build/switch-handoff/`
with `SHA256SUMS`. Its README identifies this as the corrected hardware handoff.

For network loading, put Homebrew Menu into its netloader, then use the actual
Switch IP. `nxlink -a` uploads and runs the NRO; the probes write logs to SD.

```sh
SWITCH_IP=192.168.1.100
"${DEVKITPRO:-/opt/devkitpro}/tools/bin/nxlink" -a "$SWITCH_IP" \
  build/switch-nvk-clang/vulkan_probe/switch_feasibility_probe.nro
```

If nxlink is only installed in Docker, use the host network:

```sh
docker run --rm --network host -v "$PWD:/project:ro" devkitpro/devkita64:latest \
  /opt/devkitpro/tools/bin/nxlink -a "$SWITCH_IP" \
  /project/build/switch-nvk-clang/vulkan_probe/switch_feasibility_probe.nro
```

Log files are overwritten on each run:

```text
sdmc:/nvk_smoke.log
sdmc:/nvk_mesa.log
sdmc:/switch/project8-toolchain-probe.log
sdmc:/switch/project8-memory-probe.log
sdmc:/switch/project8-vulkan-probe.log
sdmc:/switch/project8-feasibility-probe.log
```

After remounting the SD card on Linux:

```sh
mkdir -p build/switch-hardware-results
cp "$SWITCH_SD"/switch/project8-*-probe.log \
   "$SWITCH_SD/nvk_smoke.log" "$SWITCH_SD/nvk_mesa.log" \
   build/switch-hardware-results/
```

Record whether the colored triangle appeared for roughly 300 presented frames,
whether the app returned cleanly, and whether `+` exits the memory probe. Include
Horizon/CFW/hbmenu versions, console model and docked/handheld mode with the logs.
Do not attach game data. A missing log or crash is a failure to investigate,
not a pass. The Vulkan probe uses SD logging to avoid a console framebuffer
competing with the Vulkan surface for NWindow.

## What the result can establish

The combined test exercises tiny aliases at actual ReXGlue relative addresses,
separately reported permission requests, a device with required feature bits enabled, a swapchain,
SPIR-V pipeline, draw submission and VI presentation. It does **not** initialize
ReXGlue or execute Project 8. A Vulkan pass does not waive unsupported reprotection. Fault recovery, sparse
memory scaling, runtime platform adapters and Xenos correctness remain untested.
The [protection audit](../docs/SWITCH_MEMORY_REPROTECTION.md) classifies why the
existing GPU watch path cannot simply treat protection changes as no-ops.

See [the architecture audit](../docs/SWITCH_PORT_ANALYSIS.md),
[capability matrix](../docs/SWITCH_VULKAN_CAPABILITIES.md) and
[known issues](../docs/KNOWN_ISSUES.md). Driver-linked probes are local experiments,
not approved release artifacts under this repository's GPL boundary policy.

## Imported ReXGlue Horizon runtime test

The real merged SDK now builds `rexglue-switch-test.nro` with static
`rexgpu-xenos` and the pinned NVK archive. It is a local diagnostic linked
against that driver, not a release binary. Its on-console checks cover guest
reservation, five mirror views, lazy commit/mapping, logical read-only
protection, unmapping a watched page, shadow access, watched-write recovery and
concurrent read faults. The result is **implemented, unverified on Switch
hardware**. Build success does not establish any of those results.

The built NRO is at `build/rexglue-switch-test/rexglue-switch-test.nro` and is
staged at `/tmp/project8-switch-handoff/rexglue-switch-test.nro`. Copy it to
`sdmc:/switch/project8-probes/`, run it in application mode/title takeover,
and inspect `sdmc:/switch/project8-rexglue-test.log`. Every line should report
`PASS`, including `SUMMARY PASS`. Any crash or missing log is a failure to
investigate. The program waits for `+` before exiting. No game files are
required by this SDK test.

The Project8 game target is in `src/game/cmake/switch.cmake`. It requires the
locally generated `generated/default/` tree from a supported disc. The target
uses the standard Xenos Vulkan renderer.

## Local Project8 game build and hardware handoff

The user's ISO matched the supported retail executable. Native ReXGlue codegen
produced the ignored translation units, and the Horizon build produced
`build/project8-switch-final/thps_p8.nro`, staged as
`/tmp/project8-switch-handoff/thps_p8.nro`. This confirms compilation and
linking only. Project8 boot, input, audio, memory fault recovery and its first
frame are **unverified on Switch hardware**.

Keep the extracted disc beside the game NRO on the SD card. With `SWITCH_SD`
set to the mounted card's root:

```sh
mkdir -p "$SWITCH_SD/switch/project8/game"
cp /tmp/project8-switch-handoff/thps_p8.nro "$SWITCH_SD/switch/project8/"
cp -a /tmp/project8-owned-disc/. "$SWITCH_SD/switch/project8/game/"
sync
```

Open `thps_p8.nro` through Homebrew Menu in application mode/title takeover.
The app checks that `game/default.xex` matches its generated code before it
starts the runtime. Preserve any logs from `switch/project8/logs/` and report
the last visible screen or crash; a successful NRO build is not evidence that
the game runs. The disc data and generated code stay local and must never be
committed or distributed.
