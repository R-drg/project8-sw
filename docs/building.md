# Building the port yourself

## In short

Players do not need this page: the [installer page](https://r-drg.github.io/project8-sw-installer/) makes the
package from their own copy of the game, with the released NRO. This page is for building the NRO (the Switch
program) yourself, from the source code in this repository.

The steps, in order: prepare the SDK, build the graphics driver, translate the game to C++, build the NRO, and copy
everything to the SD card. The release was built on Linux, inside Docker, with exactly these steps.

## What you need

- Docker. The Switch toolchain runs in an image built from `tools/switch/Dockerfile`: devkitPro's devkitA64 image
  (devkitA64, libnx and the portlibs) plus clang 19 and glslang.
- Git, CMake 3.25 or newer, Ninja, Python 3, and a host C++ compiler (clang) to build the code generator.
- Your own copy of Tony Hawk's Project 8 for Xbox 360, extracted, with the supported `default.xex`
  (SHA-256 `cfc732340e55defda400e25f03231aa9bb65fd9545b618212f69a4952384a5dd`). Nothing from the game is in this
  repository, and nothing made from it may be added to it: `tools/check_no_game_content.sh` refuses it.
- About 16 GB of RAM. The translated game code is large.

```sh
docker build -t project8-switch-clang tools/switch
```

## 1. Prepare the SDK

**Why:** the port runs on the ReXGlue SDK v0.10.0 plus this port's changes: the Horizon (Switch) runtime, memory
handling, input, crash handling, and the GPU work the native renderer reuses (the shader translator).

```sh
git clone https://github.com/rexglue/rexglue-sdk sdk
git -C sdk checkout v0.10.0        # commit f5337cd, which the patches apply to
git -C sdk submodule update --init --recursive
while read p; do git -C sdk apply --index "$PWD/patches/rexglue-sdk/$p"; done < patches/rexglue-sdk/series
```

Apply them in the order of `patches/rexglue-sdk/series`. With all of them applied the tree is exactly the one the
release was built with. `tools/check_patch_series.sh` checks that the series lists every patch once.

## 2. Build the graphics driver

**Why:** Vulkan on the Switch is NVK, Mesa's driver for NVIDIA GPUs, ported to Horizon by mesa-switch. It is linked
into the NRO, and this port changes it (see [patches/mesa-switch/README.md](../patches/mesa-switch/README.md)).

```sh
git clone https://github.com/danfromtico/mesa-switch mesa
git -C mesa checkout d4a00ea0ab3
for p in patches/mesa-switch/0*.patch; do git -C mesa apply "$PWD/$p"; done
cd mesa && ./build-switch-p8.sh
```

The script builds its own Docker image (Rust and Meson for an AArch64 Horizon target) and leaves the driver's static
libraries in `mesa/builddir-switch/`. Merge them into one `lib/libvulkan.a`, check and index it with
`tools/switch/stage-mesa-archive.py`, and put `libnak_rs.a` next to it. That folder (the one holding `lib/`) is the
"driver SDK" of the next steps.

## 3. Translate the game to C++

**Why:** static recompilation reads the game's PowerPC code from `default.xex` and writes it out as C++. The
configuration that says how (addresses, sizes, function starts) is in `config/`; the output is a translation of your
copy of the game and must never be published.

Build the SDK once for your PC to get its `rexglue` tool, then:

```sh
cp config/thps_p8_manifest.toml config/thps_p8_manifest.local.toml
# In the copy, replace REPLACE_ME with the folder of your extracted disc.
rexglue codegen config/thps_p8_manifest.local.toml
```

It writes `generated/default/`, which git ignores. [config/README.md](../config/README.md) has the details.

## 4. Build the NRO

```sh
docker run --rm -v "$PWD:/project" -v /path/to/sdk:/sdk -v /path/to/driver-sdk:/mesa -w /project \
  project8-switch-clang sh -c '
    cmake -S src/game -B build/switch -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=/project/tools/switch/switch-rexglue-gcc.cmake \
      -DREXSDK_DIR=/sdk -DREXGLUE_SWITCH_NVK_SDK=/mesa &&
    cmake --build build/switch'
```

The result is `build/switch/thps_p8.nro`, published as `project8-sw.nro`, and next to it the unstripped
`build/switch/thps_p8`, which loads at address 0, so `addr2line` takes the offsets of a crash log directly.

The SDK's FFmpeg and libmspack (LGPL-2.1) are linked statically. To relink with a modified version of either, change
it in the SDK tree and run step 4 again; [RELINKING.md](RELINKING.md) has the details.

## 5. Copy it to the SD card

```text
/switch/project8-sw/
  project8-sw.nro           build/switch/thps_p8.nro, renamed
  thps_p8.toml              release/thps_p8.toml
  LICENSES.txt              NOTICE
  game/                     your extracted disc: default.xex and DATA
```

At start-up the game checks the SHA-256 of `game/default.xex` and refuses any other executable, because the code in
the NRO was compiled from that one. There is deliberately no setting to turn the check off.

## Before publishing anything

```sh
tools/check_no_game_content.sh
tools/check_patch_series.sh
tools/make_notice.py --sdk /path/to/sdk > NOTICE
```
