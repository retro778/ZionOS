# Known issues and limitations (0.1.0 "Aleph")

## Release blockers (why this is not a production release)

1. **No ZionOS update channel.** There is no hosted, signed ZionOS APT
   repository and no production signing key. Debian packages update
   normally; ZionOS's own packages do not. The mechanism is implemented and
   tested with a throw-away key only (`docs/RELEASE.md`).
2. **ISO and checksums are unsigned** for the same reason.
3. **Real AI provider calls are unverified.** The OpenAI and Anthropic
   adapters are tested against scripted servers that follow the published
   schemas; no request has been sent to either real API (no keys were
   available to the development session, by design).
4. **Only QEMU/KVM has been tested.** VirtualBox, VMware and physical
   hardware are untested. Secure Boot is untested.
5. **No independent security review; no native-speaker review of Hebrew.**
6. **Recovery procedures are documented but mostly unexercised.**

## Functional gaps

* **KDE Wallet in the live session.** The live user has no login password,
  so the wallet is not opened automatically and KDE shows its first-use
  wizard when the assistant first asks for the keyring. Cancelling it leads
  to the "use for this session only" choice. On an installed system the
  wallet is unlocked by the login password (verified).
* **Language change needs a new login** for non-ZionOS applications (Plasma
  reads its language at session start).
* **Hebrew installation not exercised.** Calamares offers Hebrew, but the
  automated installation runs in English.
* **No swap partition** in the installer's automatic layout (inherited
  Debian Calamares default); hibernation is therefore unavailable unless
  the disk is partitioned manually.
* **EFI boot entry is named "Debian"** (`EFI/debian`) to stay compatible
  with Debian's signed GRUB. The boot menu shows "ZionOS".
* **Boot splash and console branding.** The graphical boot splash is
  Debian's default Plymouth theme; only the GRUB menu, login screen,
  wallpaper, installer and system identification carry ZionOS branding.
* **`trixie-backports` is enabled** on installed systems by Debian's
  installer logic. Backports are never installed automatically, but the
  source is present.
* **Automatic display resizing and host clipboard** depend on the
  hypervisor agent (SPICE, Guest Additions, VMware Tools) and are untested.
  Changing the resolution from inside the guest is tested.
* **VirtualBox Guest Additions are not preinstalled** (not available in
  Debian stable); see `docs/VM.md`.
* **Assistant:** text only (no images or files other than UTF-8 text up to
  256 KiB); no tool use; one conversation at a time; live model lists are
  fetched only on request.
* **`claude-fable-5-2`** requires 30-day data retention on the Anthropic
  account (provider policy); organisations with zero data retention get an
  error from the provider.

## Build and tooling

* **Not bit-for-bit reproducible.** Builds use the live Debian mirrors, so
  package versions follow the archive (`docs/BUILD.md`).
* **Qt `lupdate` 6.8.2 hangs** on some valid C++ (a `translate()` call with a
  nested call expression as the plural argument). The sources avoid the
  construct and the test script runs `lupdate` under a timeout. The CMake
  `update_translations` target can hang for the same reason; use
  `scripts/update-translations.sh`.
* **Qt `moc` 6.8.2 mis-parses** some raw string literals ending in `")"`;
  affected test data is built with `QJsonDocument` instead.
* **Maintainer address is a placeholder** (`zionos-devel@localhost`) because
  the project has no domain or mailing list; lintian's
  `bogus-mail-host` tag is suppressed for that reason.
* **GUI tests rely on OCR** of screenshots (tesseract) and fixed keyboard
  sequences; a changed upstream dialog can break a test without a real
  regression.
* **CI workflow has never run** on a hosting service (no remote repository).
* Files under `work/` are owned by root (created inside the container).

## Third-party behaviour worth knowing

* The QEMU guest agent is installed and enabled; it only functions when the
  hypervisor provides its virtio channel.
* Flatpak is installed but no remote is configured until the user adds
  Flathub in ZionOS Settings.
