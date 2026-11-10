# Shared settings for the ZionOS QEMU scripts (sourced, not executed).
# Everything lives in this folder: the ISO, the virtual disk (a qcow2 FILE)
# and the UEFI variable store. No real disk or partition is ever touched.
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ISO=$(ls -t "$HERE"/zionos-*-amd64.iso 2>/dev/null | head -1 || true)
DISK="${ZIONOS_DISK:-$HERE/zionos-disk.qcow2}"
DISK_SIZE="${ZIONOS_DISK_SIZE:-40G}"
MEM="${ZIONOS_MEM:-4096}"          # MiB
CPUS="${ZIONOS_CPUS:-4}"
VARS="$HERE/zionos-uefi-vars.fd"
OVMF_CODE=/usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS=/usr/share/OVMF/OVMF_VARS_4M.fd

die() { echo "error: $*" >&2; exit 1; }

command -v qemu-system-x86_64 >/dev/null || die "qemu-system-x86_64 is not installed (sudo apt install qemu-system-x86 qemu-utils ovmf)"

# Firmware: UEFI when OVMF is installed, otherwise legacy BIOS.
# The choice is remembered (the UEFI variable file exists or not), because a
# system installed in one mode does not boot in the other.
firmware_args() {
    if [ -f "$VARS" ]; then
        echo "-drive if=pflash,format=raw,readonly=on,file=$OVMF_CODE -drive if=pflash,format=raw,file=$VARS"
    fi
}

base_args() {
    ACCEL="kvm"; CPU="host"
    if [ ! -w /dev/kvm ]; then
        echo "warning: /dev/kvm is not usable; running without acceleration (very slow)" >&2
        ACCEL="tcg"; CPU="max"
    fi
    echo "-name ZionOS -machine q35,accel=$ACCEL -cpu $CPU -m $MEM -smp $CPUS \
-device virtio-vga -device qemu-xhci -device usb-tablet \
-netdev user,id=n0 -device virtio-net-pci,netdev=n0 \
-device ich9-intel-hda -device hda-duplex -device virtio-rng-pci \
${ZIONOS_QEMU_DISPLAY:--display gtk,zoom-to-fit=off}"
}
