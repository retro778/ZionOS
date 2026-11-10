#!/bin/sh
# Step 3: build the ZionOS live/installation ISO with live-build inside the
# (privileged) builder container.
# Usage: scripts/build-iso.sh [minimal|desktop]     (default: desktop)
#        ZIONOS_CLEAN=1 scripts/build-iso.sh ...     full rebuild (drops chroot cache)
set -eu
. "$(dirname "$0")/common.sh"
PROFILE="${1:-desktop}"

if ! inside_container; then
    [ -d "$ROOT/distro/profiles/$PROFILE" ] || die "unknown profile: $PROFILE"
    ls "$ROOT"/out/packages/*.deb >/dev/null 2>&1 || die "run scripts/build-packages.sh first"
    # --privileged: live-build needs mount(2)/chroot(2) inside the container.
    # It never touches host disks; it works only in work/lb-<profile>.
    in_container --privileged -e ZIONOS_CLEAN=${ZIONOS_CLEAN:-} \
        -e ZIONOS_MIRROR=${ZIONOS_MIRROR:-} -e ZIONOS_SKIP_APPS=${ZIONOS_SKIP_APPS:-} -- scripts/build-iso.sh "$PROFILE"
    exit $?
fi

LB=/src/work/lb-$PROFILE
mkdir -p "$LB"
cd "$LB"
[ -n "${ZIONOS_MIRROR:-}" ] || unset ZIONOS_MIRROR
: "${SOURCE_DATE_EPOCH:=$(git -C /src log -1 --format=%ct 2>/dev/null || date +%s)}"
export SOURCE_DATE_EPOCH

if [ -d .build ]; then
    if [ -n "${ZIONOS_CLEAN:-}" ]; then lb clean --purge; else lb clean; fi
fi
# Refresh configuration from the repository (keeps cache/).
rm -rf auto config
cp -a /src/distro/auto auto
cp -a /src/distro/config config
mkdir -p config/package-lists config/packages.chroot
cp /src/distro/profiles/"$PROFILE"/*.list.chroot config/package-lists/
if [ -d "/src/distro/profiles/$PROFILE/config" ]; then
    cp -a "/src/distro/profiles/$PROFILE/config/." config/
fi
# Debug-symbol packages stay in out/packages; they are not part of the image.
for deb in /src/out/packages/*.deb; do
    case "$deb" in *-dbgsym_*) ;; *) cp "$deb" config/packages.chroot/ ;; esac
done
# Allow building the desktop image before the applications exist.
if [ -n "${ZIONOS_SKIP_APPS:-}" ]; then rm -f config/package-lists/zionos-apps.list.chroot; fi
# Boot menu background is rendered from the SVG source of the artwork package.
rsvg-convert -w 1024 -h 768 /src/packages/zionos-system/artwork/zionos-boot.svg \
    -o config/bootloaders/grub-pc/splash.png

lb config
lb build

ISO=$(ls zionos-*.iso | head -1)
[ -f "$ISO" ] || { echo "ISO was not produced" >&2; exit 1; }
SUFFIX=""; [ "$PROFILE" = desktop ] || SUFFIX="-$PROFILE"
DEST=/src/out/zionos-${ZIONOS_VERSION}${SUFFIX}-amd64.iso
cp "$ISO" "$DEST"
# Build provenance: exact package set and inputs of this image.
cp chroot.packages.live "${DEST%.iso}.packages" 2>/dev/null || \
    cp binary/live/filesystem.packages "${DEST%.iso}.packages" 2>/dev/null || true
chown "$HOST_UID:$HOST_GID" /src/out/*
ls -lh "$DEST"
