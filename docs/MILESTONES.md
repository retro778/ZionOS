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

## M3 — Desktop, live session, installer, networking, installed boot — done (2026-11-10)

KDE Plasma 6.3 (Wayland) from Debian, SDDM, Discover, Firefox ESR,
LibreOffice; Calamares with ZionOS branding and an extra confirmation
before writing; `zionos-desktop-settings` (us+il layouts, locales,
wallpaper, login background, GRUB menu). Verified by `scripts/test.sh live`
and `scripts/test.sh install` on BIOS, UEFI and UEFI with Secure Boot.

Problems found and fixed: wrong package names in the first list
(`plasma-workspace-wayland`, `kwallet-pam`, `spectacle`); debug-symbol
packages leaking into the image; KDE's own welcome centre replaced by
ZionOS Welcome; live session lacked the Alt+Shift option for the console.

## M4 — Welcome / Settings applications, packaged — done (2026-11-10)

`zionos-welcome`, `zionos-settings` (language, keyboard layouts and switch
shortcut, shortcuts into KDE System Settings, software/Flatpak, assistant,
About) as `.deb` packages from `packages/zionos-apps`. C++20, Qt 6, CMake,
`-Werror`, tests run in the package build. Keyboard-shortcut change verified
live through the GUI.

## M5 — AI assistant with mocked provider tests — done; live verification open

Anthropic Messages API and OpenAI Responses API adapters behind
`ChatProvider`, written against the official references consulted on
2026-11-10 (`docs/AI-PROVIDERS.md`). Streaming, cancel, idle timeout, error
mapping, no retries, usage display, local history with delete/disable,
sanitised Markdown, command cards with copy only, explicit attachments with
review, KDE Wallet storage with session-only fallback. Verified with mocks
(`tst_providers`, `tst_chat`), end to end in a VM against a local stand-in
provider, and with the real KDE Wallet on an installed system.

**Open:** no request has been sent to the real provider APIs (no keys
available to the development session).

## M6 — Hebrew localisation and RTL — done for ZionOS components

195-string Hebrew catalogue, RTL layouts, isolates and direction marks for
mixed text, per-paragraph direction in replies. Verified by unit tests and
`scripts/test.sh hebrew` (real key events). Found and fixed: Hebrew
sentences starting with a Latin name were laid out left-to-right.
**Open:** native-speaker review; upstream translation gaps are documented,
not fixed.

## M7 — Packaging, updates, recovery, release procedure — partly done

Done: all ZionOS components are `.deb` packages; local APT repository;
update path tested with a signed test repository including rejection of
tampered/unsigned repositories; release metadata script; recovery and
release procedures documented; key-holding processes hardened against core
dumps. **Open:** production signing key and hosted repository (release
requirement marked incomplete in `docs/RELEASE.md`); recovery procedures
largely unexercised.

## M8 — Release checks and readiness report — done

`scripts/test.sh all` and `docs/READINESS.md`. Verdict: development
preview, not production-ready; blocking items are listed there.
