# FFmpeg: licence terms and how to relink

The released binaries include FFmpeg, used under the GNU Lesser General Public
License version 2.1 or later. This page exists to satisfy section 6 of that
licence and to record the facts it depends on.

## What is linked

FFmpeg is **statically** linked into the ReXGlue runtime library, which is in
turn linked into the game binary. It is not a replaceable shared library, so
section 6's relinking provision applies in its full form rather than the easier
"swap the DLL" one.

Components: `libavcodec`, `libavformat`, `libavutil`, `libswresample`,
`libswscale`, used by the runtime to decode the game's video.

## Exactly which FFmpeg

| | |
| --- | --- |
| Fork | `https://github.com/wmarti/FFmpeg.git` |
| Commit | `0604b464c7cb4ebc94940cf1f324a3b26b87717c` |
| Local modifications | none; the tree is used as the fork publishes it |

The fork commits per-platform configuration headers rather than generating them
at build time. All eight (`config_{android,linux,macos,windows}_{x86_64,aarch64}.h`)
carry:

```
#define CONFIG_GPL 0
#define CONFIG_NONFREE 0
#define CONFIG_VERSION3 0
FFMPEG_LICENSE "LGPL version 2.1 or later"
```

Verify it yourself:

```sh
for c in thirdparty/FFmpeg/config_*.h; do grep -m1 '^#define CONFIG_GPL ' "$c"; done
```

The Switch build uses `thirdparty/ffmpeg-overlay/config_switch_aarch64.h`,
added by `patches/rexglue-sdk/0004`, which includes the Linux/aarch64 header
above and only turns off what Horizon lacks, so the same three lines hold.

project8-sw is GPL-3.0 as a whole in any case. The LGPL configuration matters
to anyone reusing the SDK changes in a port under another licence.

## Relinking with your own FFmpeg

Everything needed is source-available, which is how the section 6 obligation is
met here: the "work that uses the Library" is this repository plus the ReXGlue
SDK, both published in full, so you can modify FFmpeg and rebuild rather than
being handed object files to relink.

1. Clone the fork at the commit above, or your own modified version of it.
   Keep the LGPL configuration; a GPL build changes the licence of everything
   downstream of it.
2. Build the ReXGlue SDK against your FFmpeg, with the patches in
   `patches/rexglue-sdk/` applied in the order that directory's README gives.
3. Rebuild the NRO against that SDK tree, following step 4 of
   [building.md](building.md); the Switch build compiles the SDK from source,
   FFmpeg included.

The result is a binary identical to the released one except for the FFmpeg you
substituted.

If some part of that is not enough for you to exercise your rights under the
licence, open an issue. Being able to do this is the point, not a formality.

## Other LGPL components

`libmspack` is also LGPL-2.1 and also statically linked. Everything above
applies to it; it lives at `thirdparty/libmspack` in the SDK tree.
