#!/bin/sh
# Step 6: generate checksums and release metadata for the built ISO.
# Usage: scripts/release-metadata.sh [ISO]
#        ZIONOS_SIGN_KEY=<gpg key id> scripts/release-metadata.sh    also signs SHA256SUMS
# Output (next to the ISO): SHA256SUMS, SHA512SUMS, <iso>.buildinfo.json,
# and SHA256SUMS.asc when a signing key is given. Without a key the release
# is explicitly marked UNSIGNED.
set -eu
. "$(dirname "$0")/common.sh"
ISO="${1:-$(ls -t "$ROOT"/out/zionos-*-amd64.iso | grep -v minimal | head -1)}"
[ -f "$ISO" ] || die "ISO not found"
DIR=$(dirname "$ISO"); NAME=$(basename "$ISO"); BASE="${NAME%.iso}"
cd "$DIR"
FILES="$NAME"; [ -f "$BASE.packages" ] && FILES="$FILES $BASE.packages"
# shellcheck disable=SC2086
sha256sum $FILES > SHA256SUMS
# shellcheck disable=SC2086
sha512sum $FILES > SHA512SUMS

GIT_COMMIT=$(git -C "$ROOT" rev-parse HEAD 2>/dev/null || echo unknown)
GIT_DIRTY=$([ -z "$(git -C "$ROOT" status --porcelain 2>/dev/null)" ] && echo false || echo true)
IMAGE_ID=$(docker image inspect --format '{{.Id}}' "$BUILDER_IMAGE" 2>/dev/null || echo unknown)
SIGNED=false
if [ -n "${ZIONOS_SIGN_KEY:-}" ]; then
    gpg --batch --yes -u "$ZIONOS_SIGN_KEY" --armor --detach-sign -o SHA256SUMS.asc SHA256SUMS
    SIGNED=true
else
    rm -f SHA256SUMS.asc
fi
cat > "$BASE.buildinfo.json" <<JSON
{
  "name": "ZionOS",
  "version": "$ZIONOS_VERSION",
  "codename": "$ZIONOS_CODENAME",
  "architecture": "amd64",
  "base": {"distribution": "Debian", "suite": "$DEBIAN_SUITE",
           "point_release": "$(grep -m1 '^base-files' "$BASE.packages" 2>/dev/null | awk '{print $2}')"},
  "iso": {"file": "$NAME", "bytes": $(stat -c %s "$NAME"),
          "sha256": "$(sha256sum "$NAME" | cut -d' ' -f1)"},
  "package_count": $( [ -f "$BASE.packages" ] && wc -l < "$BASE.packages" || echo null ),
  "source": {"git_commit": "$GIT_COMMIT", "dirty_worktree": $GIT_DIRTY},
  "builder": {"image": "$BUILDER_IMAGE", "image_id": "$IMAGE_ID", "tool": "live-build"},
  "built_utc": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "checksums_signed": $SIGNED,
  "reproducibility": "repeatable build process; bit-for-bit reproducibility not verified"
}
JSON
echo "Wrote $DIR/SHA256SUMS, SHA512SUMS, $BASE.buildinfo.json (signed: $SIGNED)"
[ "$SIGNED" = true ] || echo "NOTE: release is UNSIGNED (no ZIONOS_SIGN_KEY). See docs/RELEASE.md."
