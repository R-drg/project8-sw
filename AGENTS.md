# Agent guide

Read this before changing anything here.

## What this repository is

The Nintendo Switch port of Tony Hawk's Project 8 (Xbox 360), built from
Project8Recomp and following the design of nfsmw-nx. Its one goal: someone who
owns the game ends up with a build that plays well on a Switch. The installer
page is a separate repository, project8-sw-installer.

## The rule that overrides everything

**No game content is ever committed or published.** Not assets, audio, video,
textures, `default.xex`, disc images, decrypted intermediates, platform keys,
shader dumps, or the translation units the recompiler generates from a dump.

`tools/check_no_game_content.sh` enforces it against the file listing, not the
ignore rules. Run it before any commit that adds files. If you are unsure
whether something counts: it counts.

## Layout

```
src/game/          the game's host code; src/native/ is the native renderer
src/common/        the supported-executable table used by the start-up check
config/            recompiler configuration: addresses, sizes, names
patches/           the SDK and driver changes, in apply order
release/           the settings file that goes into the package
tools/             toolchain files, the notice generator, release checks
switch_probe/      the hardware probes the port started from
docs/              player and contributor documentation
```

## Things that are the way they are on purpose

**The dump gate has no override flag.** The NRO ships prebuilt, compiled from
one exact `default.xex`; that gate is the only guarantee a player's files
match it.

**Generated sources are never committed.** They are a mechanical translation
of the player's own executable, regenerated locally from `config/`.

**The patch series must reproduce the SDK the release was built with.** A
release NRO built from an SDK tree with changes not in `patches/` cannot be
published: its source would be incomplete. Regenerate the patch before
publishing.

## Verification, and its limit

Everything that matters here (frame rate, pacing, visuals, input) is only
known once someone has played it on a Switch. Say "implemented, unverified on
hardware" and put it in `docs/known-issues.md` until then. A build that
compiles is not evidence that it plays.

## Comments

Explain why, not what, particularly when the obvious approach was tried and
failed. Do not cite documents that are not in this repository.

## Before publishing

```sh
tools/check_no_game_content.sh
tools/check_patch_series.sh
tools/make_notice.py --sdk /path/to/rexglue-sdk > NOTICE
```

## Licence

GPL-3.0 (`LICENSE`). `src/game/src/native/` adapts nfsmw-nx (GPL-3.0) and
says so at the top of each adapted file. Code contributed to Project8Recomp
under BSD-3-Clause keeps its notice (`LICENSES/BSD-3-Clause.txt`). The SDK
patches stay under the SDK's BSD-3-Clause licence and the Mesa patches under
MIT, so do not put GPL code into them. Every release ships its complete
corresponding source.

## Tone in player-facing text

The README and the installer page avoid developer vocabulary in what players
must do: say what is wrong in terms of the disc and the game, then what to do.
Contributor docs may use exact hashes and file names when those are the task.
