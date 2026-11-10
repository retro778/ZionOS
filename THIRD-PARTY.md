# Third-party software, licences and attribution

## ZionOS-specific work (this repository)

| Component | Licence |
|---|---|
| Source code under `packages/`, `scripts/`, `tests/`, `distro/` | GPL-3.0-or-later (`LICENSE`) |
| Artwork under `packages/zionos-system/artwork/` (logo, wallpaper, boot and installer images) | CC-BY-SA-4.0, original work created for ZionOS |
| Hebrew translations of ZionOS applications | GPL-3.0-or-later |

No third-party artwork or fonts are stored in this repository. Text in the
generated images is rendered at build time with DejaVu Sans (Bitstream Vera
/ public-domain-style licence).

## Software in the image

The ISO is an aggregate of unmodified Debian packages. Each keeps its own
licence; the authoritative, per-package statement is installed at
`/usr/share/doc/<package>/copyright`, and the exact package set of an image
is listed in the `*.packages` manifest written next to the ISO.

Principal components and licences (summary, not a substitute for the
per-package files):

| Component | Upstream | Licence (main) |
|---|---|---|
| Linux kernel | kernel.org | GPL-2.0 |
| GNU C library, coreutils, GCC runtime | GNU | LGPL-2.1+, GPL-3+ (with runtime exception) |
| systemd | systemd project | LGPL-2.1+ |
| APT, dpkg, live-build, live-boot, live-config | Debian | GPL-2+ / GPL-3+ |
| KDE Plasma, KWin, Dolphin, Konsole, Discover, Kate | KDE | GPL-2+/LGPL-2.1+ and others |
| Qt 6 | The Qt Company / Qt Project | LGPL-3.0 / GPL |
| QtKeychain | Frank Osterfeld et al. | BSD-3-Clause |
| SDDM | SDDM project | GPL-2+ |
| Calamares, calamares-settings-debian | Calamares / Debian | GPL-3+ |
| GRUB 2, shim | GNU / rhboot | GPL-3+ / BSD-2-Clause |
| Firefox ESR | Mozilla | MPL-2.0 |
| LibreOffice | The Document Foundation | MPL-2.0 |
| Flatpak, PackageKit | freedesktop.org | LGPL-2.1+ / GPL-2+ |
| PipeWire, WirePlumber | freedesktop.org | MIT |
| Noto fonts | Google | OFL-1.1 |
| Culmus Hebrew fonts | Culmus project | GPL-2 (with font exception) |
| DejaVu, Liberation fonts | DejaVu / Red Hat | Bitstream Vera-style / OFL-1.1 |
| Firmware (`non-free-firmware`) | hardware vendors | redistributable binary licences, **not free software** |

## Source code availability

Source for every Debian package in the image is available from the Debian
archive (`apt source <package>` with the `deb-src` lines that the installed
system already has), matched to the versions in the image manifest. When
distributing ZionOS images you are responsible for meeting the source-offer
obligations of the GPL for the binaries you distribute (for example by
mirroring the corresponding sources or pointing to `snapshot.debian.org`).

## Trademarks

Debian is a registered trademark of Software in the Public Interest, Inc.
KDE and Plasma are trademarks of KDE e.V. Firefox is a trademark of the
Mozilla Foundation. LibreOffice is a trademark of The Document Foundation.
OpenAI and Anthropic/Claude are trademarks of their respective owners.
ZionOS uses these names only to identify the software it includes or
interoperates with and claims no affiliation or endorsement. Debian
packages are shipped unmodified under their original names; ZionOS branding
is applied only to ZionOS-specific components, the boot menu, the wallpaper,
the installer's presentation and `/etc/os-release`.
