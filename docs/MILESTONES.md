# Milestones

Status legend: **done** = implemented and verified by the listed command;
**partial** = implemented, verification incomplete; **open** = not done.

## M1 — Host inspection and architecture decisions — done (2026-11-10)

* Host: Debian 13.7 (Liquorix kernel), 8 CPUs, 15 GiB RAM, Docker 29.9,
  `/dev/kvm` usable, no passwordless sudo. Nothing is installed on the host;
  all work happens in the `zionos-builder:trixie` container.
* Debian stable verified from the archive `Release` file: 13.7 "trixie".
* Decisions: `docs/ARCHITECTURE.md`.

## M2 — Minimal branded ISO boots in QEMU — done (2026-11-10)

Commands:

    scripts/env-prepare.sh
    scripts/build-packages.sh zionos-system
    scripts/build-iso.sh minimal
    scripts/test.sh live both out/zionos-0.1.0-minimal-amd64.iso

Result: 12/12 checks pass under SeaBIOS and 12/12 under OVMF (UEFI): boot
menu branding, guest reaches multi-user, `ID=zionos`/`ID_LIKE=debian`,
overlay root, no failed units, DNS + HTTPS, Debian-only APT sources, live
user, clean ACPI shutdown. Evidence: `docs/evidence/m2-minimal-live-boot.txt`.

Problems found and fixed on the way:

* live-build replaces `/etc/os-release` with a copy of Debian's file, hiding
  the `zionos-base` diversion → chroot hook `9000-zionos-identity` and a
  guard in the package's postinst restore the symlink.

## M3 — Desktop, live session, installer, installed-system boot — open
## M4 — Welcome / Settings applications packaged — open
## M5 — AI assistant with mocked provider tests — open
## M6 — Hebrew localisation and RTL checks — open
## M7 — Packaging, updates, recovery, release procedure — open
## M8 — Release checks and readiness report — open
