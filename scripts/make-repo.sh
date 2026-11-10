#!/bin/sh
# Build a flat-file APT repository in out/repo from out/packages/*.deb.
# Runs inside the builder container (called by build-packages.sh).
set -eu
REPO=/src/out/repo
rm -rf "$REPO"; mkdir -p "$REPO/pool"
# keep only the newest version of each package
cp /src/out/packages/*.deb "$REPO/pool/"
cd "$REPO"
apt-ftparchive packages pool > Packages
gzip -9nk Packages
apt-ftparchive \
    -o APT::FTPArchive::Release::Origin=ZionOS \
    -o APT::FTPArchive::Release::Label=ZionOS \
    -o APT::FTPArchive::Release::Suite=zionos-dev \
    -o APT::FTPArchive::Release::Codename=aleph \
    -o APT::FTPArchive::Release::Architectures="amd64 all" \
    release . > Release
if [ -n "${ZIONOS_SIGN_KEY:-}" ]; then
    gpg --batch --yes -u "$ZIONOS_SIGN_KEY" --clearsign -o InRelease Release
    gpg --batch --yes -u "$ZIONOS_SIGN_KEY" -abs -o Release.gpg Release
fi
