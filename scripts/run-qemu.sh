#!/bin/sh
# Step 4: launch ZionOS in QEMU/KVM.
# Usage: scripts/run-qemu.sh [--uefi] [--disk FILE.qcow2] [--no-iso] [--iso FILE] [--mem MB]
#
# Uses the host's qemu-system-x86_64 with a window when it is installed;
# otherwise runs QEMU from the builder container and serves the display over
# VNC on 127.0.0.1:5901 (connect with any VNC viewer).
# The virtual disk is a qcow2 FILE; no host disk or partition is ever used.
set -eu
. "$(dirname "$0")/common.sh"
ISO=$(ls -t "$ROOT"/out/zionos-*-amd64.iso 2>/dev/null | head -1 || true)
DISK=""; UEFI=0; MEM=4096; USE_ISO=1
while [ $# -gt 0 ]; do
    case "$1" in
        --uefi) UEFI=1 ;;
        --disk) DISK=$2; shift ;;
        --iso) ISO=$2; shift ;;
        --no-iso) USE_ISO=0 ;;
        --mem) MEM=$2; shift ;;
        *) die "unknown option $1" ;;
    esac
    shift
done
rel() { realpath --relative-to="$ROOT" "$1"; }

set -- -machine q35,accel=kvm -cpu host -m "$MEM" -smp 4 \
    -device virtio-vga -device qemu-xhci -device usb-tablet \
    -netdev user,id=n0 -device virtio-net-pci,netdev=n0 \
    -device ich9-intel-hda -device hda-duplex
if [ -n "$DISK" ]; then
    [ -f "$DISK" ] || die "disk image $DISK not found (create: scripts/in-builder.sh qemu-img create -f qcow2 $DISK 32G)"
    case "$DISK" in *.qcow2) ;; *) die "refusing non-qcow2 disk: $DISK" ;; esac
fi

if command -v qemu-system-x86_64 >/dev/null 2>&1; then
    [ $UEFI = 1 ] && set -- "$@" -bios /usr/share/ovmf/OVMF.fd
    [ $USE_ISO = 1 ] && [ -n "$ISO" ] && set -- "$@" -cdrom "$ISO"
    [ -n "$DISK" ] && set -- "$@" -drive "file=$DISK,if=virtio,format=qcow2"
    exec qemu-system-x86_64 "$@" -display gtk
else
    [ $UEFI = 1 ] && set -- "$@" -bios /usr/share/ovmf/OVMF.fd
    [ $USE_ISO = 1 ] && [ -n "$ISO" ] && set -- "$@" -cdrom "/src/$(rel "$ISO")"
    [ -n "$DISK" ] && set -- "$@" -drive "file=/src/$(rel "$DISK"),if=virtio,format=qcow2"
    echo "QEMU display: vnc://127.0.0.1:5901"
    in_container --device /dev/kvm -p 127.0.0.1:5901:5901 -- \
        qemu-system-x86_64 "$@" -display none -vnc 0.0.0.0:1
fi
