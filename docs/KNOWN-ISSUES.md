# Known issues and limitations (0.1.0 "Aleph")

## Release blockers (why this is not a production release)

1. **Update channel is new and single-operator.** A signed repository now
   exists (`docs/RELEASE.md`), but the signing key is a software key held by
   one person, there is one server and no mirror, and no update has yet
   been delivered through it to a real user's system.
2. **Release signing is not hardware-backed** (same key; see above).
3. **Real AI provider calls are unverified.** The OpenAI and Anthropic
   adapters are tested against scripted servers that follow the published
   schemas; no request has been sent to either real API (no keys were
   available to the development session, by design).
4. **Only QEMU/KVM has been tested.** VirtualBox, VMware and physical
   hardware are untested. Secure Boot is untested.
5. **No independent security review; no native-speaker review of Hebrew.**
6. **Recovery procedures are documented but mostly unexercised.**

## Intermittent issues seen in testing (cause unknown)

* **Live desktop slow or missing once.** In one of roughly ten live boots
  under QEMU the Plasma desktop of the live user had not started 240 s
  after boot (UEFI + Secure Boot). The immediate re-run on the same ISO
  passed. No diagnostics were captured for the failing boot.
* **Restart not detected once.** In one development run the installed
  system did not answer the guest agent within 300 s after a reboot that
  followed an upgrade; the same disk then rebooted normally three times.
  It is not known whether the guest or the test harness was at fault.

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
* **CI:** the `unit` and `minimal-iso` jobs pass on GitHub Actions; the full
  desktop install job is manual and has not been run on GitHub.
* Files under `work/` are owned by root (created inside the container).

## Third-party behaviour worth knowing

* The QEMU guest agent is installed and enabled; it only functions when the
  hypervisor provides its virtio channel.
* Flatpak is installed but no remote is configured until the user adds
  Flathub in ZionOS Settings.
