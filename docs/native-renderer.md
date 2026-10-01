# Switch native renderer

Status: implemented. Played on an OLED Switch in handheld mode (GPU at
768 MHz): 30 fps in Free Skate and Career nearly all the time, with a few slow
frames left in the busiest areas and while a level loads. Every setting below
was played with on hardware except `thps_pace_before_present`, which is
**implemented, unverified on hardware**, and says so.

## Why

With the SDK's Xenos backend (an emulated Xbox 360 GPU) the game ran at
1-5 fps on Switch. Most of that went to emulating the GPU's embedded memory
(EDRAM) on host render targets, and to write-watching guest memory, which
Horizon can only do by unmapping pages and emulating every fault.

nfsmw-nx (https://github.com/StevensND/nfsmw-nx) measured the same backend
at 1-3 fps in Need for Speed races and replaced it with a native renderer.
This one follows its design. `src/game/src/native/thps_native_system.*` is
adapted from its `app/src/nfsmw_nativo_sistema.*` (GPL-3.0); see `NOTICE`.

## How it works

`thps_renderer = "native"` in `thps_p8.toml` replaces the SDK's graphics
system with `src/game/src/native/`:

- **Command ring** (`thps_native_system.cpp`). Reads the game's PM4 stream on
  its own thread pinned to a core, keeps the register file, and raises the
  vblank and swap interrupts the game waits for.
- **Render targets** are plain Vulkan images keyed by EDRAM base, pitch,
  format and sample count. Nothing is copied in or out of an EDRAM buffer.
- **Resolves** are image copies into textures registered at the destination
  address. Texture fetches from that address, and the frontbuffer, use them;
  guest memory is not written. A texture sampled at a different size from
  the image (a 264x152 blur source in a 320-wide destination) gets a copy of
  exactly that size, refreshed by each resolve: texture coordinates span the
  whole image, and sampling the larger one drew the skater doubled, with a
  light halo. Depth is resolved by a compute pass into a float image; if a
  game samples a depth resolve as anything but a depth format, it gets the
  words the resolve would have left in memory, `(depth24 << 8) | stencil`.
- **Shaders** are translated at run time by the SDK's SPIR-V translator, on a
  worker thread. Pipelines are also created on a worker; a draw whose pipeline
  is not ready yet is skipped rather than stalling the frame. The driver's
  compiled pipelines are saved to the SD card and reloaded on the next run.
- **Textures** are untiled on worker threads. New textures, image creation
  included, are made off the ring thread and appear a frame or two later.
  Freeing GPU memory also happens on its own thread: on Horizon every
  allocation and every free is a kernel round trip.
- **Vertex and index data** are copied per submission into a stream buffer
  and deduplicated by content, checked at most once per frame per range.
- **Draw state** is cached by a hash of the render-state registers, which is
  updated one register write at a time, so an unchanged draw costs a lookup.

## Pacing

The game has no frame pacing of its own on this path. A frame is held until at
least `thps_min_frame_vblanks` vblanks have passed since the previous one (2
gives 30 fps). A frame that is late is not held, so it is not rounded up to the
next vblank.

The hold used to sit between the present and the interrupt that reports a
completed swap. An early frame then went on screen at once, and the next one,
if slightly late, 50 ms after it: a short gap and a long one, felt as a hitch
although the average stayed at 30 fps. With `thps_pace_before_present` the hold
comes before the present instead, after the frame's draws have been submitted
so the GPU works during it.

## Settings (`thps_p8.toml`)

| Setting | Default | Effect |
| --- | --- | --- |
| `thps_renderer` | | `"native"` selects this renderer. |
| `thps_min_frame_vblanks` | 1 | Minimum vblanks per frame; 2 for 30 fps. |
| `thps_pace_before_present` | false | Holds a frame for the cap before presenting it, not after: an early frame used to go on screen at once and a slightly late next one 50 ms later, felt as a hitch. Implemented, unverified. |
| `thps_native_async_pipelines` | true | Create pipelines on a worker. |
| `thps_native_msaa` | true | Off renders every target with one sample. |
| `thps_native_max_anisotropy` | 16 | Cap on anisotropic filtering. |
| `thps_native_depth24` | false | Depth in 32 bits per pixel: 24-bit depth with stencil for the game's unorm depth, float without stencil for its float depth (shadow maps), instead of float with stencil, which NVIDIA GPUs keep in 64. A resolve of 24-bit depth is converted to float by a compute pass. |
| `thps_native_gpu_profile` | false | Logs GPU time by render pass and resolve, timed on one frame in 16. |
| `thps_native_record_thread` | false | Makes the Vulkan calls of draws on a second thread while the ring thread prepares the next draw. |
| `guest_vblank_wait_sleep` | false | The game's presentation-interval wait sleeps instead of spinning. |
| `guest_vblank_late_no_wait` | false | A frame that finished late starts the next one at once, instead of waiting for the next tick of the 60 Hz vblank timer. Without it every late frame was rounded up to a multiple of 16.7 ms (a 36 ms frame took 50). Frames on time still wait, so the frame rate cap is kept. |
| `guest_job_wait_yield` | false | The main thread's wait for a job slot, and its task scheduler while no task is ready, yield instead of spinning. |

The two guest waits are replaced in `src/game/src/guest_swap_wait.h` and
`src/game/src/guest_job_wait.h`. Each returns exactly what the original loop
would have; only the spinning is gone. On the Xbox 360 a spinning hardware
thread is nearly free; on the Switch the game has three cores, and the spin
takes time from the threads it is waiting for.

## Known limits

- The GPU's reported timestamps read about 1.6 times low on this driver, so
  the "GPU ms" in the log understates GPU time.
- Objects can be missing for a moment while their pipeline compiles on first
  sight; textures can appear a frame or two late in a new area.
- With multisampling on, depth resolves of multisampled targets are not done.
  With `thps_native_msaa = false` they are.

## Rules carried over from nfsmw-nx's experience

- Measure at stock clocks.
- Structures that are hashed have no padding
  (`std::has_unique_object_representations_v`).
- No atomics for counters touched by one thread (the Cortex-A57 has no fast
  atomics).
