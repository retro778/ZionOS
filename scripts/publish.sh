#!/bin/sh
# Publish ZionOS packages (and optionally the ISO) to the update server.
#
#   scripts/publish.sh            packages in out/packages -> signed APT repository
#   scripts/publish.sh --iso      also upload the ISO with signed checksums
#   scripts/publish.sh --dry-run  build and sign locally, upload nothing
#
# Signing happens on THIS machine with the ZionOS archive key in your GPG
# keyring (you are asked for its passphrase). The server only ever receives
# already-signed files; it holds no key. Settings: release.conf.
set -eu
. "$(dirname "$0")/common.sh"
. "$ROOT/release.conf"
ISO_TOO=0; DRY=0
for a in "$@"; do
    case "$a" in --iso) ISO_TOO=1 ;; --dry-run) DRY=1 ;; *) die "unknown option $a" ;; esac
done
FPR_FILE="$ROOT/packages/zionos-system/keyring/fingerprint"
KEY="${ZIONOS_SIGN_KEY:-$(cat "$FPR_FILE" 2>/dev/null || true)}"
[ -n "$KEY" ] || die "no signing key configured (see docs/RELEASE.md, 'Creating the archive key')"
gpg --list-secret-keys "$KEY" >/dev/null 2>&1 || die "secret key $KEY is not in your GPG keyring"
ls "$ROOT"/out/packages/*.deb >/dev/null 2>&1 || die "no packages; run scripts/build-packages.sh"

PUB="$ROOT/out/publish"
rm -rf "$PUB/repo"; mkdir -p "$PUB/repo"
# 1. Repository tree and indexes (inside the builder container).
in_container -e ZIONOS_SUITE="$ZIONOS_SUITE" -- sh -c '
    set -eu
    R=/src/out/publish/repo
    mkdir -p "$R/pool/main" "$R/dists/$ZIONOS_SUITE/main/binary-amd64"
    for deb in /src/out/packages/*.deb; do
        case "$deb" in *-dbgsym_*) continue ;; esac
        cp "$deb" "$R/pool/main/"
    done
    cd "$R"
    apt-ftparchive packages pool/main > "dists/$ZIONOS_SUITE/main/binary-amd64/Packages"
    gzip -9nk "dists/$ZIONOS_SUITE/main/binary-amd64/Packages"
    xz -9k "dists/$ZIONOS_SUITE/main/binary-amd64/Packages"
    apt-ftparchive \
        -o APT::FTPArchive::Release::Origin=ZionOS \
        -o APT::FTPArchive::Release::Label=ZionOS \
        -o APT::FTPArchive::Release::Suite="$ZIONOS_SUITE" \
        -o APT::FTPArchive::Release::Codename="$ZIONOS_SUITE" \
        -o APT::FTPArchive::Release::Architectures=amd64 \
        -o APT::FTPArchive::Release::Components=main \
        -o APT::FTPArchive::Release::Description="ZionOS packages" \
        release "dists/$ZIONOS_SUITE" > /tmp/Release
    mv /tmp/Release "dists/$ZIONOS_SUITE/Release"
    chown -R "$HOST_UID:$HOST_GID" /src/out/publish
'
# 2. Sign on the host.
D="$PUB/repo/dists/$ZIONOS_SUITE"
gpg --batch --yes -u "$KEY" --clearsign -o "$D/InRelease" "$D/Release"
gpg --batch --yes -u "$KEY" -abs -o "$D/Release.gpg" "$D/Release"
gpg --armor --export "$KEY" > "$PUB/repo/zionos-archive-keyring.asc"
gpg --verify "$D/InRelease" 2>/dev/null || die "signature verification failed"
echo "Signed repository: $(ls "$PUB/repo/pool/main" | wc -l) packages, suite $ZIONOS_SUITE, key $KEY"

if [ $ISO_TOO = 1 ]; then
    ISO=$(ls -t "$ROOT"/out/zionos-*-amd64.iso | grep -v minimal | head -1)
    ZIONOS_SIGN_KEY="$KEY" "$ROOT/scripts/release-metadata.sh" "$ISO"
fi
[ $DRY = 0 ] || { echo "dry run: nothing uploaded ($PUB/repo)"; exit 0; }

# 3. Upload. Package files first, indexes last, so clients never see an
#    index that refers to a file that is not there yet.
rsync -a --info=stats1 "$PUB/repo/pool" "$PUB/repo/zionos-archive-keyring.asc" "$ZIONOS_REPO_SSH:$ZIONOS_REPO_DIR/repo/"
rsync -a --delete-after "$PUB/repo/dists" "$ZIONOS_REPO_SSH:$ZIONOS_REPO_DIR/repo/"
rsync -a --delete-after "$PUB/repo/pool" "$ZIONOS_REPO_SSH:$ZIONOS_REPO_DIR/repo/"
if [ $ISO_TOO = 1 ]; then
    O="$ROOT/out"; B=$(basename "$ISO" .iso)
    rsync -a --partial --info=progress2 "$ISO" "$O/SHA256SUMS" "$O/SHA256SUMS.asc" "$O/SHA512SUMS" \
        "$O/$B.packages" "$O/$B.buildinfo.json" "$ZIONOS_REPO_SSH:$ZIONOS_REPO_DIR/iso/"
fi
echo "Published to $ZIONOS_REPO_URL"
