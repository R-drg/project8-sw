<h1 align=center>Tony Hawk's Project 8 — Nintendo Switch port</h1>

A native Nintendo Switch port of the Xbox 360 version of **Tony Hawk's Project 8** (2006).

It's not an emulator. The PowerPC code of the game's `default.xex` is statically recompiled to C++ with
[ReXGlue](https://github.com/rexglue/rexglue-sdk) and compiled for the Switch, and the game is drawn by a native
Vulkan renderer running on NVK (Mesa). It targets **a steady 30 FPS**.

project8-sw is built from [Project8Recomp](https://github.com/R-drg/Project8Recomp), the recompilation of the game
for PC, and follows the design of [nfsmw-nx](https://github.com/StevensND/nfsmw-nx), StevensND's Switch port of Need
for Speed: Most Wanted, whose native renderer it adapts. See [Credits](#credits).

> [!NOTE]
> You need your own copy of the Xbox 360 game: a disc image (`.iso`) or the extracted disc with its `default.xex`.
> Supported: the 2006 retail disc (`default.xex` SHA-256 `cfc73234…a5dd`). Other releases are refused at start-up,
> because the program is compiled from that exact executable.

## How to install

1. Open the installer page: **https://r-drg.github.io/project8-sw-installer/**
2. Choose your format (**Disc image (.iso)** or **XEX format**, the folder with `default.xex` and `DATA`) and press
   **Create project8-sw.zip**. Everything runs in your browser and the game files never leave your computer: the
   page checks your copy, downloads the build and puts the two together.
3. Extract the downloaded `project8-sw.zip` into `sdmc:/switch/`.

Your `project8-sw` folder should look like this:

```text
/switch/project8-sw/
  project8-sw.nro
  thps_p8.toml              (settings)
  LICENSES.txt              (licenses of the software in the NRO)
  game/                     (the files of your disc: default.xex, DATA)
```

The game creates `thps_p8/` (saves and the graphics pipeline cache) and `logs/` next to the NRO the first time it
runs.

Launch it in title takeover mode: hold **R** while starting any installed game to open the Homebrew Menu, then start
`project8-sw.nro`. A forwarder should also work if you set it to a **39-bit address space**. Applet mode (from the
Album) gives homebrew much less memory and is not supported.

> [!NOTE]
> **Updating:** open the installer page again and press **Create project8-sw-update.zip**. It only contains
> `project8-sw.nro`, `thps_p8.toml` and `LICENSES.txt`, so there is no need to copy the game files again: extract it
> over your existing folder. It replaces `thps_p8.toml`; note any setting you changed first.

> [!CAUTION]
> **The first minutes** after installing or updating compile the graphics pipelines the game has not shown yet, so
> objects can be missing for a moment and the frame rate can dip. The pipelines are saved on the SD card and later
> sessions start smoothly.

**Coming from a development build** in `/switch/project8/`: move its `thps_p8/` folder (your saves) into
`/switch/project8-sw/`, and its `game/` folder too if you do not want to extract the disc again.

## Controls

Each Switch button acts as the Xbox 360 button with the same letter, so the letter in the on-screen prompts is the
button to press.

<div align="center">

| Switch | Xbox 360 |
| --- | --- |
| A | A |
| B | B |
| X | X |
| Y | Y |
| ZR / ZL | RT / LT |
| R / L | RB / LB |
| + | Start |
| − | Back |
| Sticks / D-pad | Sticks / D-pad |

</div>

To use the face buttons by position instead, as on an Xbox pad (B acts as A, A as B, Y as X and X as Y), add
`input_xbox_layout = true` to `thps_p8.toml`.

## Settings

`thps_p8.toml`, next to the NRO, holds the settings, and each one is described in the file. The renderer's settings
are explained in more detail in [docs/native-renderer.md](docs/native-renderer.md#settings-thps_p8toml).

## Performance

Tested on an OLED Switch in handheld mode, with the GPU at 768 MHz (set by sys-clk): Free Skate and Career run at a
locked 30 FPS nearly all the time. A few frames still take longer in the busiest areas (2,000-3,000 draws per frame)
and while a level loads. Stock handheld clocks and docked mode were not measured for this release.

The game had no frame pacing of its own on this path; the port caps it at 30 FPS and spaces the frames evenly
(`thps_min_frame_vblanks` and `thps_pace_before_present` in `thps_p8.toml`).

## Known issues

- **Career cutscenes freeze.** The video shows its first frame, partly decoded (green and magenta blocks), and stays
  on it with no sound until the cutscene's time is over; the game then carries on normally. The renderer receives
  the video frames and the video decoder keeps running, so the cause is most likely the audio path that the video
  is timed by. Not fixed yet.
- Objects can be missing for a moment while their pipeline is compiled the first time they appear, and textures can
  appear a frame or two late in a new area.

The full list, with what is known about each, is in [docs/known-issues.md](docs/known-issues.md).

## How it works

- **The game's code is translated, not emulated.** [ReXGlue](https://github.com/rexglue/rexglue-sdk) translates every
  function of `default.xex` to C++, which is then compiled for the Switch, and gives the game what it expects from an
  Xbox 360: its system, files, audio and controllers. The Horizon (Switch) layer of the SDK is in
  `patches/rexglue-sdk/`.
- **Its own renderer.** Emulating the Xbox 360 GPU ran the game at 1-5 FPS on the Switch. Instead, the port reads the
  commands the game sends to the GPU and draws the same frame with Vulkan: render targets are plain images, resolves
  are image copies, shaders are translated once and cached, textures are untiled on worker threads. The design comes
  from nfsmw-nx; [docs/native-renderer.md](docs/native-renderer.md) explains it.
- **The game's busy waits sleep.** The game spins while it waits for the GPU, a vblank or a job slot. That is nearly
  free on the Xbox 360 and costs a whole core on the Switch, so those waits are replaced by ones that sleep and return
  exactly what the original loop would have.
- **Its own graphics driver.** Vulkan runs on NVK, from the Mesa project, through
  [mesa-switch](https://github.com/danfromtico/mesa-switch), with this port's changes in `patches/mesa-switch/`:
  cheaper descriptor binding and draws on the Switch's Maxwell GPU, and two shader compiler changes taken from
  nfsmw-nx.

## Documentation

<div align="center">

| Document | What it explains |
| --- | --- |
| [docs/building.md](docs/building.md) | How to build the NRO and the graphics driver, step by step |
| [docs/native-renderer.md](docs/native-renderer.md) | How the port draws the game with Vulkan, and every renderer setting |
| [docs/known-issues.md](docs/known-issues.md) | What does not work yet, and what is known about it |
| [docs/RELINKING.md](docs/RELINKING.md) | Rebuilding with a modified FFmpeg or libmspack (LGPL) |
| [docs/SWITCH_PORT_ANALYSIS.md](docs/SWITCH_PORT_ANALYSIS.md) | The study that started the port |
| [docs/NFSMW_NX_REUSE_ANALYSIS.md](docs/NFSMW_NX_REUSE_ANALYSIS.md) | What was taken from nfsmw-nx, and why |
| [docs/SWITCH_MEMORY_REPROTECTION.md](docs/SWITCH_MEMORY_REPROTECTION.md) | Watching guest memory on Horizon |
| [docs/SWITCH_VULKAN_CAPABILITIES.md](docs/SWITCH_VULKAN_CAPABILITIES.md) | What NVK offers on the Switch |

</div>

## Build

devkitA64 and libnx from [devkitPro](https://devkitpro.org), CMake, Ninja, Docker, the ReXGlue SDK v0.10.0 with
`patches/rexglue-sdk/`, the Vulkan driver built from mesa-switch with `patches/mesa-switch/`, and your own copy of
the game for the code generator. The whole process is in [docs/building.md](docs/building.md).

<div align="center">

| Folder | Contents |
| --- | --- |
| `src/game/` | The game: hooks, native renderer (`src/native/`), Switch entry point, CMake project |
| `src/common/` | The table of supported executables, used by the start-up check |
| `config/` | The code generator's configuration: addresses, sizes, names. No game code |
| `patches/rexglue-sdk/` | The SDK changes: Horizon runtime, memory, input, GPU, in apply order (`series`) |
| `patches/mesa-switch/` | The graphics driver changes and its build script |
| `release/` | The settings file that goes into the package |
| `tools/` | Toolchain files, the notice generator and the release checks |
| `switch_probe/` | The hardware probes the port started from (threads, memory, Vulkan) |
| `docs/` | Documentation |

</div>

## Credits

- **Activision and Neversoft** — creators of Tony Hawk's Project 8. This is an unofficial, fan-made port with no
  affiliation.
- **[Project8Recomp](https://github.com/R-drg/Project8Recomp)** and its contributors — the recompilation project this
  port is built from: the code generator configuration, the game's hooks, the start-up check and the first Switch
  runtime.
- **[StevensND](https://github.com/StevensND)** — [nfsmw-nx](https://github.com/StevensND/nfsmw-nx), whose native
  renderer `src/game/src/native/` adapts (GPL-3.0), whose Mesa changes `patches/mesa-switch/0002` takes (MIT), and
  whose measurements on the Switch this port relied on; and
  [nfsmw-nx-installer](https://github.com/StevensND/nfsmw-nx-installer), the installer page that
  [project8-sw-installer](https://github.com/R-drg/project8-sw-installer) is adapted from.
- **[Tom Clay](https://github.com/rexglue/rexglue-sdk)** — the ReXGlue SDK, built on the work of the
  **[Xenia](https://xenia.jp)** team.
- **[danfromtico](https://github.com/danfromtico/mesa-switch)** and **[NaGaa95](https://github.com/NaGaa95/mesa-switch)**
  — mesa-switch: Mesa, NVK and NAK on Horizon. And the **[Mesa](https://mesa3d.org)** developers.
- **[devkitPro](https://devkitpro.org) and [switchbrew](https://github.com/switchbrew/libnx)** — devkitA64 and libnx.
- The other libraries listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), with their licence texts in
  [NOTICE](NOTICE).

## Legal

No affiliation with Activision, Neversoft or Tony Hawk, Inc. Tony Hawk's Project 8 and its marks belong to their
respective owners. This repository contains no assets and no program code from the original game: the translated
code is generated from your own `default.xex` when you build. The released NRO is that translation compiled for the
Switch; it holds no game assets and not the original executable, and it refuses to start without your own copy of
the supported disc, which the installer page puts next to it on your own computer. Users must provide their own
legally obtained copy. The NRO's icon is the game's cover art (`src/game/switch/icon.jpg`), © Activision
Publishing, Inc., used only to identify the game in the Homebrew Menu. Running
homebrew requires custom firmware, which violates Nintendo's terms of service and can get a console banned — your
call.

Source code is provided under the GNU General Public License, version 3 (see [LICENSE](LICENSE)), the licence of the
nfsmw-nx code it adapts. Code contributed to Project8Recomp under BSD-3-Clause keeps that notice
([LICENSES/BSD-3-Clause.txt](LICENSES/BSD-3-Clause.txt)); the SDK changes in `patches/rexglue-sdk/` are under the
SDK's BSD-3-Clause licence and the driver changes in `patches/mesa-switch/` under Mesa's MIT licence, so other ports
can reuse them.
