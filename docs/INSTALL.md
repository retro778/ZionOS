# Installing ZionOS

## Requirements

x86_64 (amd64) PC or virtual machine, UEFI or legacy BIOS, 4 GiB RAM
(2 GiB may boot but is untested), 20 GiB disk.

## Live session

Boot the ISO and choose **ZionOS Live (English)** or **ZionOS Live
(Hebrew)**. The live desktop logs in automatically.

| Live-only setting | Value |
|---|---|
| User / password | `zion` / `live` |
| sudo | passwordless for `zion` |
| Root account | locked |
| Persistence | none; changes are lost at shutdown |

These settings exist only in the live session and are not copied to an
installed system.

## Installation

1. In the Welcome window press **Install ZionOS** (or start *Install ZionOS*
   from the application launcher).
2. Choose language, time zone and keyboard layout.
3. **Partitions:** the selected disk is shown by device name and size (for
   example `vda - 24.00 GiB (/dev/vda)`). *Erase disk* deletes everything on
   that disk. *Manual partitioning* is available; encryption can be enabled
   on this page.
4. Create your user: name, login, computer name and a password. There is no
   default password and root stays locked.
5. The **Summary** page lists exactly what will happen, including the line
   `Erase disk /dev/… and install ZionOS`. After pressing *Install* a second
   confirmation appears. Nothing is written to the disk before that.
6. When "All done" appears, restart and remove the installation medium.

The installer is Calamares with Debian's installation logic
(`calamares-settings-debian`) and ZionOS branding. After installation the
live-session and installer packages are removed from the installed system.

Defaults chosen by the automatic layout: GPT with a 300 MiB EFI system
partition on UEFI (or an MBR/BIOS layout), one ext4 root partition, no swap
partition. Use manual partitioning for anything else.

The EFI boot entry is created in `EFI/debian`: Debian's signed GRUB expects
that path. The boot menu itself shows "ZionOS".

## After installation

* Software and updates: **Discover**, or `sudo apt update && sudo apt full-upgrade`.
* APT sources are Debian's `trixie`, `trixie-updates`, `trixie-security`
  (and `trixie-backports`, which is never used unless you ask for it).
* Language and keyboard: **ZionOS Settings**. Both English (US) and Hebrew
  layouts are enabled; **Alt+Shift** switches.
* The assistant is optional; see `docs/AI-PROVIDERS.md`.

## Safety notes for testers

Only install to a blank virtual disk or a machine whose disk you intend to
erase. The project's own tests install exclusively to qcow2 image files.
