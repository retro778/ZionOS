#!/bin/sh
# Install ZionOS into a virtual machine (QEMU/KVM).
#
#   ./install-zionos-qemu.sh            create the virtual disk and boot the ISO
#   ./install-zionos-qemu.sh --bios     use legacy BIOS instead of UEFI
#   ./install-zionos-qemu.sh --fresh    delete the existing virtual disk first
#
# In the VM: choose "ZionOS Live", then press "Install ZionOS" in the Welcome
# window and follow the installer. The only disk the VM can see is the file
# zionos-disk.qcow2 in this folder (shown as "vda"), so "Erase disk" is safe.
# When the installer says "All done", shut the VM down and start the
# installed system with ./run-zionos-qemu.sh
#
# Live session login (if asked): user "zion", password "live".
# Options via environment: ZIONOS_DISK_SIZE=40G ZIONOS_MEM=4096 ZIONOS_CPUS=4
set -eu
. "$(dirname "$0")/qemu-common.sh"
BIOS=0; FRESH=0
for a in "$@"; do
    case "$a" in
        --bios) BIOS=1 ;;
        --fresh) FRESH=1 ;;
        -h|--help) sed -n '2,17p' "$0"; exit 0 ;;
        *) die "unknown option: $a" ;;
    esac
done
[ -n "$ISO" ] && [ -f "$ISO" ] || die "no zionos-*-amd64.iso found in $HERE"
command -v qemu-img >/dev/null || die "qemu-img is not installed (sudo apt install qemu-utils)"

if [ -e "$DISK" ]; then
    if [ $FRESH = 1 ]; then
        rm -f "$DISK" "$VARS"
    else
        die "$DISK already exists.
  Start the installed system:   ./run-zionos-qemu.sh
  Reinstall from scratch:       ./install-zionos-qemu.sh --fresh"
    fi
fi
qemu-img create -f qcow2 "$DISK" "$DISK_SIZE" >/dev/null
echo "Created virtual disk $DISK ($DISK_SIZE, grows on demand)"

rm -f "$VARS"
if [ $BIOS = 0 ] && [ -f "$OVMF_CODE" ] && [ -f "$OVMF_VARS" ]; then
    cp "$OVMF_VARS" "$VARS"
    echo "Firmware: UEFI"
else
    echo "Firmware: legacy BIOS"
fi

echo "Booting $(basename "$ISO") ..."
# shellcheck disable=SC2046
exec qemu-system-x86_64 $(base_args) $(firmware_args) \
    -drive "file=$DISK,if=none,id=hd0,format=qcow2,discard=unmap" \
    -device virtio-blk-pci,drive=hd0,bootindex=2 \
    -drive "file=$ISO,if=none,id=cd0,media=cdrom,readonly=on,format=raw" \
    -device ide-cd,drive=cd0,bootindex=1
