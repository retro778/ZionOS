# Recovery

ZionOS uses Debian's standard boot and package tooling, so Debian recovery
procedures apply. The procedures below are documented from the system's
design; except where noted they have **not** been exercised by the automated
tests.

## The system does not boot

1. **Recovery mode.** In the GRUB menu choose *Advanced options for ZionOS* →
   the entry ending in *(recovery mode)*. Because root is locked, Debian asks
   to press Enter for maintenance without a root password only if configured;
   otherwise use the live medium (next item).
2. **Live medium.** Boot the ZionOS ISO, open Konsole:

   ```sh
   lsblk -f                         # find the root partition, e.g. /dev/vda2
   sudo mount /dev/vda2 /mnt
   sudo mount /dev/vda1 /mnt/boot/efi          # UEFI systems only
   for d in dev proc sys run; do sudo mount --rbind /$d /mnt/$d; done
   sudo chroot /mnt
   ```

   Inside the chroot you can repair packages (`dpkg --configure -a`,
   `apt -f install`), reset a password (`passwd <user>`), or reinstall the
   boot loader:

   ```sh
   grub-install /dev/vda            # BIOS;  UEFI: grub-install --bootloader-id=debian
   update-grub
   ```
3. **Graphics problems.** From the ISO, *ZionOS Live (safe graphics)* boots
   with `nomodeset`. On an installed system press `e` in GRUB and append
   `nomodeset` to the `linux` line.

## An update was interrupted

```sh
sudo dpkg --configure -a
sudo apt -f install
sudo apt full-upgrade
```

## Forgotten password

Use the live medium and `chroot` as above, then `passwd <user>`. With
full-disk encryption the disk passphrase is still required; a lost
encryption passphrase cannot be recovered.

## Restoring Debian identification / removing ZionOS branding

```sh
sudo apt remove zionos-base zionos-desktop-settings zionos-artwork
```

`zionos-base` removes its diversion and Debian's `/usr/lib/os-release`
returns. The system is then a plain Debian trixie installation.

## Assistant problems

* "Keyring unavailable": open **KDE Wallet Manager**, enable the wallet
  subsystem and unlock the default wallet, then save the key again.
* Remove all assistant data: delete the key in the assistant settings, then
  `rm -r ~/.local/share/zionos-assistant ~/.config/ZionOS`.

## Verified by tests

* A system upgraded with `apt dist-upgrade` plus a ZionOS package update
  restarts correctly (`tests/vm/test_update.py`).
* The live medium boots on BIOS and UEFI (`tests/vm/test_live_boot.py`),
  which is the entry point for every procedure above.
