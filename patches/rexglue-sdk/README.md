# ReXGlue v0.10.0 runtime patches

Project8Recomp uses the ReXGlue SDK `v0.10.0` tag plus the patches listed in this
directory. They are required build inputs, not optional tuning. The runtime
patch carries the game's correctness fixes, Windows pacing mitigation, Steam
Deck performance paths, developer-console fixture support, and default-off
diagnostic counters. The second patch updates the bundled MoltenVK to 1.4.2 so
the Apple Silicon build presents its rendered frames correctly.

`series` is the machine-readable apply order. Apply it from a pristine SDK
checkout:

```sh
git checkout v0.10.0
while IFS= read -r patch; do
  git apply --check --index "/path/to/Project8Recomp/patches/rexglue-sdk/$patch"
  git apply --index "/path/to/Project8Recomp/patches/rexglue-sdk/$patch"
done < /path/to/Project8Recomp/patches/rexglue-sdk/series
git submodule sync --recursive
git submodule update --init --recursive
```

`--index` is required because the MoltenVK revision is a submodule gitlink. A
plain working-tree-only `git apply` does not advance the recorded commit for an
uninitialized submodule.

## What the overlay carries

- Deadline-based waits, bounded vblank catch-up, timer and ring blocking, and
  other pacing fixes. These mitigate the high CPU use and low frame rate
  reported on Windows, though Windows remains experimental until it is tested
  on more real Windows systems.
- Native residency, prefetch, index and vertex paths, plus the accepted sampler,
  texture and descriptor reuse paths used by the performance preset.
- Save diagnostics, stale-process protection, delete-on-close behavior, and the
  command-driven fixture hooks used for repeatable testing.
- Reproducible source-path mapping and a useful clang-cl warning level for
  portable release builds.
- Default-off performance counters and draw-census capture used to validate
  renderer changes. They do not run during normal play.

The third patch records the early Horizon platform contract. The fourth patch
replaces its runtime gate with the Horizon memory, exception, threading, input,
audio and VI surface adapters adapted from nfsmw-nx's BSD-licensed SDK changes.
It links `rexgpu-xenos` statically and preserves the Project8 runtime fixes in
the first patch. The merge decisions and deferred NFSMW-specific changes are in
[the reuse analysis](../../docs/NFSMW_NX_REUSE_ANALYSIS.md). On Switch
hardware the combined build reaches the game's main menu but is not playable;
[known issues](../../docs/known-issues.md) has the current state.

Every new patch must be listed exactly once in `series`.
The fifth patch preserves the subsequent Switch profiling, audio batching,
rendering and memory experiments from the live SDK, plus bounded diagnostics
for the loading-screen crash. Hot-read unwatching defaults to disabled. These
changes are implemented, unverified; a successful cross build does not establish
rendering correctness or stability. The diagnostic records `q31` and candidate
guest registers around the suspect image read in `logs/rex/rex_crash.log`.

The sixth patch saves FP control/status at exception entry and restores SIMD
and FP state after the resume trap. It adds a startup register-preservation
probe and retains suspect image-read diagnostics per thread. Hardware
verification of the correction is pending.

`tools/check_patch_series.sh` checks both directions: an unlisted patch and a
missing listed patch are both failures.
