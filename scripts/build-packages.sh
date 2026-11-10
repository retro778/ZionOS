#!/bin/sh
# Step 2: build the ZionOS .deb packages inside the builder container and
# publish them into a local APT repository (out/repo).
# Usage: scripts/build-packages.sh [package-dir-name ...]
set -eu
. "$(dirname "$0")/common.sh"

if ! inside_container; then
    in_container -- scripts/build-packages.sh "$@"
    exit $?
fi

[ $# -gt 0 ] || set -- $(ls /src/packages)
OUT=/src/out/packages
mkdir -p "$OUT"
export DEB_BUILD_OPTIONS="${DEB_BUILD_OPTIONS:-parallel=$(nproc)}"
: "${SOURCE_DATE_EPOCH:=$(git -c safe.directory=/src -C /src log -1 --format=%ct 2>/dev/null || date +%s)}"
export SOURCE_DATE_EPOCH

for pkg in "$@"; do
    echo "=== building $pkg ==="
    B=$(mktemp -d /tmp/zbuild.XXXXXX)
    # Sources are copied out of the bind mount so the tree stays clean and
    # no root-owned build products appear in the repository.
    rsync -a --exclude build --exclude gen "/src/packages/$pkg/" "$B/$pkg/"
    (cd "$B/$pkg" && dpkg-buildpackage -us -uc -b)
    rm -f "$OUT/$(sed -n 's/^Source: //p' "$B/$pkg/debian/control")"_*.buildinfo
    cp "$B"/*.deb "$B"/*.buildinfo "$B"/*.changes "$OUT/"
    lintian --tag-display-limit 0 --allow-root --suppress-tags no-manual-page,bogus-mail-host,bogus-mail-host-in-debian-changelog "$B"/*.changes || true
    rm -rf "$B"
done

# Local development APT repository (unsigned unless ZIONOS_SIGN_KEY is set;
# see docs/RELEASE.md).
scripts/make-repo.sh
chown -R "$HOST_UID:$HOST_GID" /src/out
ls -l "$OUT"/*.deb
