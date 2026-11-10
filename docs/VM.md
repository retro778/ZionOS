# Virtual machines

The same ISO is intended for QEMU/KVM, VirtualBox and VMware. Only QEMU/KVM
has actually been run; the status table at the end is explicit about that.

## QEMU / KVM (primary, automated)

Tested configuration (see `tests/vm/zvm.py`): QEMU 10 from Debian trixie,
`-machine q35,accel=kvm -cpu host`, 4 vCPU, 4 GiB RAM, `virtio-vga`,
`virtio-net` (user networking), `virtio-blk` qcow2 disk, IDE CD-ROM, USB
tablet + keyboard, Intel HDA audio, firmware SeaBIOS or OVMF
(`OVMF_CODE_4M.fd`, Secure Boot not enabled).

```sh
scripts/run-qemu.sh                      # live session, BIOS
scripts/run-qemu.sh --uefi               # live session, UEFI
scripts/in-builder.sh qemu-img create -f qcow2 work/disk.qcow2 32G
scripts/run-qemu.sh --uefi --disk work/disk.qcow2            # install
scripts/run-qemu.sh --uefi --disk work/disk.qcow2 --no-iso   # boot the installed system
```

virt-manager / libvirt: create a "Debian 13" guest, firmware UEFI or BIOS,
video *Virtio*, add a *Channel* `org.qemu.guest_agent.0` and, for clipboard
sharing and automatic display resizing, a *SPICE* display with the
`spicevmc` channel (`spice-vdagent` is preinstalled in ZionOS).

Guest integration packages in the image: `qemu-guest-agent`,
`spice-vdagent`; virtio drivers are part of the Debian kernel.

## VirtualBox (untested)

Suggested settings: Type *Linux*, Version *Debian (64-bit)*, ≥ 4 GiB RAM,
2+ CPUs, ≥ 24 GiB VDI disk, Graphics controller **VMSVGA** with 128 MB video
memory (enable 3D acceleration if the host supports it), *Enable EFI*
optional, network NAT, audio controller ICH AC97 or Intel HD Audio.

Guest integration: the Debian kernel contains the `vboxguest`, `vboxsf` and
`vboxvideo` drivers, so graphics, mouse integration and basic shared-folder
mounting are expected to work. **Debian 13 does not package the VirtualBox
Guest Additions user-space tools** (VirtualBox is not in Debian stable), so
shared clipboard, drag-and-drop and automatic display resizing require
installing the Guest Additions from the VirtualBox "Insert Guest Additions
CD image" menu inside the installed system
(`sudo apt install build-essential dkms linux-headers-amd64`, then run
`VBoxLinuxAdditions.run`). Oracle's licence allows that, but ZionOS cannot
redistribute the result preinstalled.

## VMware Workstation / Player / Fusion (x86) / ESXi (untested)

Suggested settings: Guest OS *Debian 12.x 64-bit* or newer, ≥ 4 GiB RAM,
2+ CPUs, ≥ 24 GiB disk (SCSI or NVMe), network NAT, firmware UEFI or BIOS,
*Accelerate 3D graphics* optional.

Guest integration: `open-vm-tools` and `open-vm-tools-desktop` are
preinstalled (clipboard, display resizing, time sync, shared folders via
`vmhgfs-fuse`: `vmhgfs-fuse .host:/ /mnt/hgfs -o allow_other` after enabling
a shared folder). The Debian kernel includes `vmwgfx`, `vmxnet3` and
`vmw_pvscsi`.

## Compatibility matrix

Legend: **verified** = executed in this project's automated tests with the
result recorded in `docs/READINESS.md`; **partial** = some aspects executed;
**untested** = never run; **failing** = run and failed.

<!-- MATRIX-BEGIN -->
(filled in by the release readiness pass)
<!-- MATRIX-END -->
