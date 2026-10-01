#!/usr/bin/env bash
# Local driver evaluation only; this does not stage a distributable release.
set -euo pipefail
nxvk_source="${1:-/tmp/p8-nxvk}"
revision=e028d428c3b1f2dabadd81645c53cddfc8d7ab51
if [[ ! -d "$nxvk_source/.git" ]]; then
    git init "$nxvk_source"
    git -C "$nxvk_source" remote add origin https://github.com/PalindromicBreadLoaf/nxvk
    git -C "$nxvk_source" fetch --depth 1 origin "$revision"
    git -C "$nxvk_source" checkout --detach FETCH_HEAD
fi
if [[ "$(git -C "$nxvk_source" rev-parse HEAD)" != "$revision" ]]; then
    echo "Expected nxvk $revision; use a separate checkout at that revision." >&2
    exit 1
fi
context="$(mktemp -d /tmp/project8-nxvk-docker.XXXXXX)"
cp -R "$nxvk_source/switch/docker/." "$context/"
# Rustup's default HTTP transport failed while fetching the pinned compiler on
# this host. Use its curl transport without changing the compiler revision.
sed -i '/^ENV RUST_NIGHTLY=/i ENV RUSTUP_USE_CURL=1' "$context/Dockerfile"
docker build -t project8-nxvk-probe "$context"
docker run --rm -v "$nxvk_source:/work" -w /work project8-nxvk-probe make CONTAINER=
echo "Archives: $nxvk_source/switch/build/pkg/lib"
