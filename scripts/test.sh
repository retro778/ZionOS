#!/bin/sh
# Step 5: run tests.
# Usage: scripts/test.sh unit                      C++ unit tests (+ASan/UBSan), static analysis
#        scripts/test.sh live [bios|uefi|both] [ISO] [--desktop]
#        scripts/test.sh install [bios|uefi] [ISO] install to a blank qcow2, reboot into it
#        scripts/test.sh all
set -eu
. "$(dirname "$0")/common.sh"
if ! inside_container; then
    in_container --device /dev/kvm -- scripts/test.sh "$@"
    exit $?
fi
MODE="${1:-all}"; [ $# -gt 0 ] && shift
default_iso() { ls -t /src/out/zionos-*-amd64.iso | grep -v minimal | head -1; }
RES=/src/out/test-results; mkdir -p "$RES"
rc=0
case "$MODE" in
    unit)
        tests/unit.sh || rc=$? ;;
    live)
        FW="${1:-both}"; ISO="${2:-$(default_iso)}"; shift 2 2>/dev/null || true
        python3 tests/vm/test_live_boot.py "$ISO" --firmware "$FW" --out "$RES" "$@" || rc=$? ;;
    install)
        FW="${1:-uefi}"; ISO="${2:-$(default_iso)}"
        python3 tests/vm/test_install.py "$ISO" --firmware "$FW" --out "$RES" || rc=$? ;;
    all)
        tests/unit.sh || rc=$?
        ISO=$(default_iso)
        python3 tests/vm/test_live_boot.py "$ISO" --firmware both --desktop --out "$RES" || rc=$?
        python3 tests/vm/test_install.py "$ISO" --firmware uefi --out "$RES" || rc=$?
        python3 tests/vm/test_install.py "$ISO" --firmware bios --out "$RES" || rc=$? ;;
    *) die "unknown mode $MODE" ;;
esac
chown -R "$HOST_UID:$HOST_GID" /src/out
exit $rc
