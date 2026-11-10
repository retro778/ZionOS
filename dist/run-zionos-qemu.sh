#!/bin/sh
# Start ZionOS in a virtual machine (QEMU/KVM).
#
#   ./run-zionos-qemu.sh          boot the system installed by
#                                 ./install-zionos-qemu.sh
#   ./run-zionos-qemu.sh --live   boot the live ISO only (nothing is saved)
#
# Options via environment: ZIONOS_MEM=4096 ZIONOS_CPUS=4
set -eu
. "$(dirname "$0")/qemu-common.sh"
LIVE=0
for a in "$@"; do
    case "$a" in
        --live) LIVE=1 ;;
        -h|--help) sed -n '2,9p' "$0"; exit 0 ;;
        *) die "unknown option: $a" ;;
    esac
done

if [ $LIVE = 1 ]; then
    [ -n "$ISO" ] && [ -f "$ISO" ] || die "no zionos-*-amd64.iso found in $HERE"
    echo "Booting the live ISO (user \"zion\", password \"live\"; nothing is saved)"
    # shellcheck disable=SC2046
    exec qemu-system-x86_64 $(base_args) -cdrom "$ISO"
fi

[ -f "$DISK" ] || die "no installed system yet ($DISK is missing).
  Install first:        ./install-zionos-qemu.sh
  Or try without installing: ./run-zionos-qemu.sh --live"
echo "Starting ZionOS from $(basename "$DISK") ($([ -f "$VARS" ] && echo UEFI || echo BIOS))"
# shellcheck disable=SC2046
exec qemu-system-x86_64 $(base_args) $(firmware_args) \
    -drive "file=$DISK,if=none,id=hd0,format=qcow2,discard=unmap" \
    -device virtio-blk-pci,drive=hd0,bootindex=1
