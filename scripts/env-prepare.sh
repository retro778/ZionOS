#!/bin/sh
# Step 1: prepare the isolated build environment (a Docker image).
# Usage: scripts/env-prepare.sh
set -eu
. "$(dirname "$0")/common.sh"
command -v docker >/dev/null || die "docker is required (see docs/BUILD.md)"
docker build -t "$BUILDER_IMAGE" "$ROOT/build/docker"
mkdir -p "$ROOT/out" "$ROOT/work"
# Record tool provenance for this builder image.
in_container -- sh -c 'dpkg-query -W -f "\${Package} \${Version}\n" live-build debootstrap xorriso squashfs-tools grub-common qemu-system-x86 ovmf cmake g++ qt6-base-dev debhelper' \
    > "$ROOT/out/builder-tool-versions.txt"
docker image inspect --format '{{.Id}}' "$BUILDER_IMAGE" >> "$ROOT/out/builder-tool-versions.txt"
echo "Builder image ready: $BUILDER_IMAGE"
