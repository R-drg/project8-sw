# Known issues

As of v1.0.0, played on an OLED Switch in handheld mode.

## Career cutscenes freeze

**Impact:** the career mode's video cutscenes (Bink movies, `DATA/MOVIES/BIK`) show their first frame, partly
decoded (green and magenta blocks), and stay on it with no sound until the cutscene's time is over; the game then
carries on normally. **Workaround:** none needed beyond waiting, or skipping the cutscene.

What is known: the renderer receives the video's frames (a 1280x720 brightness plane and two 640x360 colour planes,
uploaded every frame with new content), the Bink decoding thread keeps working and its file reads succeed. Bink
paces its video by its audio, and the audio of the cutscenes is never heard, so the likeliest cause is the audio
path (the game's XAudio2 code or the SDK's audio output) never advancing Bink's audio clock. Not yet investigated
further.

## A few slow frames in the busiest areas

**Impact:** Free Skate and Career hold 30 fps nearly all the time; a few frames still take 50-80 ms in the areas with
2,000-3,000 draws per frame, and while a level loads.

In those frames the time goes to the game's own CPU work and to looking up the textures of every draw. The release
also applies the 30 fps cap before showing a frame instead of after (`thps_pace_before_present = true`): before, an
early frame went on screen at once and a slightly late one 50 ms after it, a hitch even at 30 fps on average. That
change is **implemented, unverified on hardware**; set it to `false` to get the pacing of the last tested build.

Only handheld mode with the GPU at 768 MHz (set by sys-clk) was measured. Stock clocks and docked mode were not.

## First appearance of objects and textures

Objects can be missing for a moment the first time their pipeline is compiled, and textures can appear a frame or
two late in a new area. Compiled pipelines are saved in `thps_p8/` on the SD card, so this happens once per
installation and update.

## Keep the frame rate cap

With `thps_min_frame_vblanks = 1` the game runs uncapped. On PC, uncapped, career cutscenes play several times too
fast; the in-level cutscene update follows the frame rate. Keep the shipped value, 2.

## Fixed during development

For anyone who meets them in a modified build, the visual bugs found on hardware and what caused them:

- **Black screen, then the game closing, after a minute in a busy area:** GPU memory freed while a frame split over
  several submissions was still using it.
- **The skater drawn doubled, with a light halo:** a resolve copied into a texture larger than the game samples, so
  texture coordinates covered the wrong area. Resolves are now copied at exactly the sampled size.
- **The Create-a-Skater skater drawn plain white:** a native version of a vertex unpacking function read the wrong
  word of a vector; it now matches the translated code, which it was checked against.
- **Broken world geometry (huge stretched polygons):** a shader found in the cache by a sample of its code that
  matched a different shader at the same address; and, separately, 1D textures, which are drawn empty again.
