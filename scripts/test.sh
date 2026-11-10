#!/bin/sh
# Step 5: run tests (inside the builder container; /dev/kvm is passed through).
# Usage: scripts/test.sh unit                        C++ unit tests (-Werror, ASan/UBSan), catalogue check, cppcheck
#        scripts/test.sh live [bios|uefi|uefi-secureboot|both] [ISO] [--desktop]
#        scripts/test.sh hebrew [bios|uefi] [ISO]    Hebrew typing, RTL UI, assistant conversation (mock provider)
#        scripts/test.sh install [bios|uefi|uefi-secureboot] [ISO] install to a blank qcow2, boot it, log in, restart
#        scripts/test.sh update [bios|uefi]          update the system installed by `install`
#        scripts/test.sh all                         everything; summary in out/test-results/SUMMARY.txt
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

# run NAME COMMAND...: run one suite, keep its log, record pass/fail.
run() {
    _name=$1; shift
    echo "### $_name: $*"
    _s=0
    "$@" > "$RES/$_name.log" 2>&1 || _s=$?
    grep -E '^\[(PASS|FAIL)\]|^== |tests passed|compiler warnings|cppcheck:|catalogue' "$RES/$_name.log" || tail -5 "$RES/$_name.log"
    if [ $_s -eq 0 ]; then echo "PASS  $_name" >> "$RES/SUMMARY.txt"; else echo "FAIL  $_name (exit $_s)" >> "$RES/SUMMARY.txt"; rc=1; fi
}

case "$MODE" in
    unit)
        tests/unit.sh || rc=$? ;;
    live)
        FW="${1:-both}"; ISO="${2:-$(default_iso)}"; [ $# -ge 2 ] && shift 2 || shift $#
        python3 tests/vm/test_live_boot.py "$ISO" --firmware "$FW" --out "$RES" "$@" || rc=$? ;;
    hebrew)
        FW="${1:-bios}"; ISO="${2:-$(default_iso)}"
        python3 tests/vm/test_hebrew.py "$ISO" --firmware "$FW" --out "$RES" || rc=$? ;;
    install)
        FW="${1:-uefi}"; ISO="${2:-$(default_iso)}"
        python3 tests/vm/test_install.py "$ISO" --firmware "$FW" --out "$RES" --keep-disk || rc=$? ;;
    update)
        FW="${1:-uefi}"
        python3 tests/vm/test_update.py "/src/work/install-$FW.qcow2" --firmware "$FW" \
            --vars "$RES/install-$FW/OVMF_VARS.fd" --out "$RES" || rc=$? ;;
    all)
        ISO=$(default_iso)
        { echo "ZionOS test run $(date -u +%Y-%m-%dT%H:%M:%SZ)"; echo "ISO: $(basename "$ISO")  sha256=$(sha256sum "$ISO" | cut -d' ' -f1)";
          echo "git: $(git -c safe.directory=/src -C /src rev-parse --short HEAD 2>/dev/null || echo n/a)  kvm: $([ -w /dev/kvm ] && echo yes || echo no)";
          echo "qemu: $(qemu-system-x86_64 --version | head -1)"; echo; } > "$RES/SUMMARY.txt"
        run unit tests/unit.sh
        run live-bios    python3 tests/vm/test_live_boot.py "$ISO" --firmware bios --desktop --out "$RES"
        run live-uefi    python3 tests/vm/test_live_boot.py "$ISO" --firmware uefi --desktop --out "$RES"
        run hebrew-bios  python3 tests/vm/test_hebrew.py "$ISO" --firmware bios --out "$RES"
        run live-uefi-secureboot python3 tests/vm/test_live_boot.py "$ISO" --firmware uefi-secureboot --out "$RES"
        # Installation under UEFI with Secure Boot enforced (Microsoft keys,
        # Debian-signed shim/GRUB/kernel), then updates on that system.
        run install-uefi-secureboot python3 tests/vm/test_install.py "$ISO" --firmware uefi-secureboot --out "$RES" --keep-disk
        run update-uefi-secureboot  python3 tests/vm/test_update.py /src/work/install-uefi-secureboot.qcow2 \
                             --firmware uefi-secureboot --vars "$RES/install-uefi-secureboot/OVMF_VARS.fd" --out "$RES"
        run install-bios python3 tests/vm/test_install.py "$ISO" --firmware bios --out "$RES"
        echo; cat "$RES/SUMMARY.txt" ;;
    *) die "unknown mode $MODE" ;;
esac
chown -R "$HOST_UID:$HOST_GID" /src/out
exit $rc
