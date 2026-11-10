ZionOS 0.1.0 "Aleph" - try it in QEMU
======================================

Files in this folder
  zionos-0.1.0-amd64.iso     live + installation image (BIOS and UEFI)
  install-zionos-qemu.sh     create a virtual disk and start the installer
  run-zionos-qemu.sh         start the installed system (or --live)
  SHA256SUMS                 checksum of the ISO (unsigned development build)

Install
  ./install-zionos-qemu.sh
    1. In the boot menu choose "ZionOS Live (English)" or "(Hebrew)".
    2. In the Welcome window press "Install ZionOS" and follow the steps.
       The VM sees only the virtual disk file zionos-disk.qcow2 ("vda").
    3. When it says "All done", shut the VM down.

Run
  ./run-zionos-qemu.sh          the installed system
  ./run-zionos-qemu.sh --live   the live session without installing

Details
  Virtual disk   zionos-disk.qcow2, 40 GB maximum, grows on demand
                 (a fresh install uses about 7.5 GB).
                 Other size:  ZIONOS_DISK_SIZE=80G ./install-zionos-qemu.sh
  Memory / CPUs  4 GB / 4;  ZIONOS_MEM=8192 ZIONOS_CPUS=6 ./run-zionos-qemu.sh
  Firmware       UEFI (OVMF) by default; add --bios when installing for legacy BIOS
  Start over     ./install-zionos-qemu.sh --fresh   (deletes the virtual disk)
  Live login     user "zion", password "live" (live session only)
  Keyboard       English + Hebrew, switch with Alt+Shift
  Mouse          click inside the window; Ctrl+Alt+G releases a grabbed pointer

This is a development preview, not a production release. The project
sources, documentation and test results are in ~/ZionOS (see
docs/READINESS.md and docs/KNOWN-ISSUES.md).
