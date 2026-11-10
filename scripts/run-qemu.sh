#!/bin/sh
# Step 4: launch ZionOS in QEMU/KVM.
# Usage: scripts/run-qemu.sh [--uefi] [--disk FILE.qcow2] [--no-iso] [--iso FILE]
#                            [--mem MB] [--spice] [--dry-run]
#
# With qemu-system-x86_64 installed on the host a normal window opens;
# otherwise QEMU runs from the builder container and the display is served
# over VNC on 127.0.0.1:5901.
#   --spice     host QEMU only: SPICE display with the vdagent channel
#               (clipboard sharing, automatic resizing); view with
#               `remote-viewer spice://127.0.0.1:5930`
#   --dry-run   print the QEMU command instead of running it
# The virtual disk must be a qcow2 FILE; no host disk or partition is used.
set -eu
. "$(dirname "$0")/common.sh"
ISO=$(ls -t "$ROOT"/out/zionos-*-amd64.iso 2>/dev/null | grep -v minimal | head -1 || true)
DISK=""; UEFI=0; MEM=4096; USE_ISO=1; SPICE=0; DRY=0
while [ $# -gt 0 ]; do
    case "$1" in
        --uefi) UEFI=1 ;;
        --disk) DISK=$2; shift ;;
        --iso) ISO=$2; shift ;;
        --no-iso) USE_ISO=0 ;;
        --mem) MEM=$2; shift ;;
        --spice) SPICE=1 ;;
        --dry-run) DRY=1 ;;
        -h|--help) sed -n '2,16p' "$0"; exit 0 ;;
        *) die "unknown option $1" ;;
    esac
    shift
done
if [ -n "$DISK" ]; then
    case "$DISK" in *.qcow2) ;; *) die "refusing non-qcow2 disk: $DISK" ;; esac
    [ -f "$DISK" ] || die "disk image $DISK not found. Create it with:
  scripts/in-builder.sh qemu-img create -f qcow2 $DISK 32G"
    [ -b "$DISK" ] && die "refusing to use a block device"
fi
[ $USE_ISO = 0 ] || [ -f "$ISO" ] || die "no ISO found; run scripts/build-iso.sh"
[ -w /dev/kvm ] || echo "warning: /dev/kvm is not writable; the VM will be very slow" >&2

HOSTQEMU=0; command -v qemu-system-x86_64 >/dev/null 2>&1 && HOSTQEMU=1
if [ $HOSTQEMU = 1 ]; then P=""; else P="/src/"; fi
path() { if [ $HOSTQEMU = 1 ]; then realpath "$1"; else echo "/src/$(realpath --relative-to="$ROOT" "$1")"; fi; }

set -- qemu-system-x86_64 -name ZionOS -machine q35,accel=kvm -cpu host -m "$MEM" -smp 4 \
    -device virtio-vga -device qemu-xhci -device usb-tablet \
    -netdev user,id=n0 -device virtio-net-pci,netdev=n0 \
    -device ich9-intel-hda -device hda-duplex \
    -device virtio-serial -chardev socket,path=/tmp/zionos-qga.$$.sock,server=on,wait=off,id=qga0 \
    -device virtserialport,chardev=qga0,name=org.qemu.guest_agent.0
if [ $UEFI = 1 ]; then
    CODE=""
    for c in /usr/share/OVMF/OVMF_CODE_4M.fd /usr/share/OVMF/OVMF_CODE.fd /usr/share/ovmf/OVMF.fd; do
        if [ $HOSTQEMU = 0 ] || [ -f "$c" ]; then CODE=$c; break; fi
    done
    [ -n "$CODE" ] || die "OVMF firmware not found (install the 'ovmf' package)"
    VARS="$ROOT/work/run-qemu-OVMF_VARS.fd"
    if [ ! -f "$VARS" ] && [ $DRY = 0 ]; then
        mkdir -p "$ROOT/work" 2>/dev/null || true
        if [ $HOSTQEMU = 1 ]; then cp "$(dirname "$CODE")/OVMF_VARS_4M.fd" "$VARS" 2>/dev/null || cp "$(dirname "$CODE")/OVMF_VARS.fd" "$VARS" || VARS=""
        else in_container -- cp /usr/share/OVMF/OVMF_VARS_4M.fd "/src/work/run-qemu-OVMF_VARS.fd"; fi
    fi
    set -- "$@" -drive "if=pflash,format=raw,readonly=on,file=$CODE"
    [ -n "$VARS" ] && set -- "$@" -drive "if=pflash,format=raw,file=$(path "$VARS" 2>/dev/null || echo "$VARS")"
fi
[ -n "$DISK" ] && set -- "$@" -drive "file=$(path "$DISK"),if=virtio,format=qcow2"
[ $USE_ISO = 1 ] && set -- "$@" -cdrom "$(path "$ISO")"
[ -n "$DISK" ] && [ $USE_ISO = 1 ] && set -- "$@" -boot menu=on

if [ $HOSTQEMU = 1 ]; then
    if [ $SPICE = 1 ]; then
        set -- "$@" -spice port=5930,addr=127.0.0.1,disable-ticketing=on \
            -chardev spicevmc,id=vd0,name=vdagent -device virtserialport,chardev=vd0,name=com.redhat.spice.0 -display none
        echo "SPICE display: remote-viewer spice://127.0.0.1:5930"
    else
        set -- "$@" -display gtk,gl=off
    fi
    if [ $DRY = 1 ]; then echo "$@"; exit 0; fi
    exec "$@"
else
    set -- "$@" -display none -vnc 0.0.0.0:1
    if [ $DRY = 1 ]; then echo "docker run ... $BUILDER_IMAGE $*"; exit 0; fi
    echo "QEMU display: vnc://127.0.0.1:5901"
    in_container --device /dev/kvm -p 127.0.0.1:5901:5901 -- "$@"
fi
