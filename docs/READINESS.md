# Release readiness report — ZionOS 0.1.0 "Aleph"

**Verdict: development preview. Not production-ready.**

The image builds, boots, installs, updates and runs its desktop and
applications in QEMU/KVM, and the automated suite passes (results below).
Several release criteria are nevertheless unmet or unverified, so this
release must not be labelled production-ready. Building and booting an ISO
is not, by itself, evidence of production quality.

## Release criteria

| # | Criterion | Status | Evidence / gap |
|---|---|---|---|
| 1 | ISO builds from the repository with documented commands in an isolated environment | **met** | `scripts/build-iso.sh`; `docs/BUILD.md` |
| 2 | Live boot on BIOS and UEFI | **met (QEMU)** | `live-bios`, `live-uefi` |
| 3 | Boot with UEFI Secure Boot enforced | **met (QEMU/OVMF, Microsoft keys)** | `live-uefi-secureboot`, `install-uefi-secureboot` |
| 4 | Installation to a blank disk; target disk identified and confirmed before writing; no default credentials | **met (QEMU)** | `install-bios`, `install-uefi-secureboot` |
| 5 | Installed system boots, graphical login, restart, shutdown | **met (QEMU)** | same suites |
| 6 | Debian security updates preserved and working | **met** | `update-uefi-secureboot` (dist-upgrade), APT source checks |
| 7 | ZionOS packages delivered as `.deb`; authenticated update mechanism | **partly met** | mechanism verified with a test key; **no production repository or key** |
| 8 | Signed release artefacts | **not met** | no signing infrastructure; checksums are generated unsigned |
| 9 | English and Hebrew: selectable at setup and later; typing, RTL, mixed text | **met for ZionOS components (QEMU)** | `hebrew-bios`, unit tests; upstream gaps in `docs/LOCALIZATION.md`; no native-speaker review |
| 10 | AI assistant: both providers, streaming, cancel, errors, no retries, key handling | **met against mocks; not verified against the real APIs** | `tst_providers`, `tst_chat`, `hebrew-bios`, keyring check in install suites |
| 11 | OS fully usable without AI, keys or the assistant package | **met** | all suites run without keys; "removing the assistant removes nothing else" |
| 12 | Security review | **not met** | self-review only (`docs/SECURITY.md`); no independent review |
| 13 | Licensing and attribution documented | **met** | `THIRD-PARTY.md`; original artwork only |
| 14 | Recovery procedures | **documented, mostly unexercised** | `docs/RECOVERY.md` |
| 15 | VM compatibility: QEMU, VirtualBox, VMware | **QEMU verified; VirtualBox and VMware untested** | `docs/VM.md` |
| 16 | Physical hardware | **untested** | — |
| 17 | Reproducible build | **repeatable, not bit-for-bit reproducible** | `docs/BUILD.md` |
| 18 | CI | **defined, never executed on a CI service** | `.github/workflows/ci.yml` |

Blocking for a production label: 7, 8, 10 (real APIs), 12, 14, 15, 16.

## Automated test results

<!-- RESULTS-BEGIN -->
(filled in from out/test-results/SUMMARY.txt by the final run)
<!-- RESULTS-END -->

What the suites cover:

* **unit** — 5 Qt Test suites (SSE parser; redaction, credentials, history,
  Markdown sanitising, attachments, os-release, desktop config; both provider
  adapters against a scripted HTTP server incl. every error class, cancel,
  timeout, no-retry, redirect refusal, TLS failure; conversation controller;
  translations and bidi), built with `-Werror` + ASan/UBSan, zero compiler
  warnings; Hebrew catalogue freshness; cppcheck clean.
* **live-bios / live-uefi** — boot through the real boot loader; identity;
  no failed units; DNS + HTTPS; Debian-only APT sources; Plasma session;
  audio sink and playback; resolution change; Firefox, LibreOffice, Kate;
  compiling C and C++; ZionOS apps start; keyboard-shortcut setting changed
  through the GUI and effective; clean shutdown.
* **live-uefi-secureboot** — the same boot with Secure Boot enforced.
* **hebrew-bios** — Hebrew boot entry, Hebrew UI, typing with Alt+Shift,
  Hebrew file name, clipboard, session-only key path, Hebrew conversation
  with a streamed mixed-direction reply against a local stand-in provider,
  HTML shown inert, usage display, history permissions, copy-command button,
  installer in Hebrew.
* **install-uefi-secureboot / install-bios** — graphical installation driven
  by keyboard/mouse events; disk identification and confirmation; boot from
  disk; no live/installer packages left; only the created user, root locked,
  no autologin; APT works; SDDM login; desktop checks; API key stored in KDE
  Wallet (unlocked by login) and nowhere in plain text; restart; shutdown.
* **update-uefi-secureboot** — Debian dist-upgrade; ZionOS upgrade from a
  signed repository; tampered, wrongly signed and unsigned repositories
  rejected; restart after upgrade.

## Verified manually during development (not automated)

* Adding the Flathub remote from *ZionOS Settings → Software* (user-level
  remote created) — QEMU live session, 2026-11-10.
* KDE Wallet first-use wizard in the live session, cancel → explanation →
  "use for this session only" — QEMU Hebrew live session, 2026-11-10.
* `dist/install-zionos-qemu.sh` and `dist/run-zionos-qemu.sh` start QEMU on
  the development host (headless smoke test; UEFI, BIOS, live).

## Not verified

* Requests to the real OpenAI and Anthropic APIs (no keys in the development
  environment). `tst_live_api` is provided for a key holder.
* VirtualBox, VMware, physical hardware, Wi-Fi, Bluetooth, printing,
  suspend/resume, multi-monitor, HiDPI.
* SPICE/agent features: host clipboard sharing, automatic display resize,
  shared folders.
* Installation in Hebrew, manual partitioning, disk encryption, dual boot.
* Recovery procedures other than "boots from the live medium" and "restarts
  after an upgrade".
* Upgrades across ZionOS releases (there is only one release).
* Accessibility (screen reader, high contrast) of the ZionOS applications.
* Long-running stability and performance.

## Recommended next steps

1. Create the archive signing key and a hosted repository; ship
   `zionos-archive-keyring` and sign ISO checksums.
2. Run `tst_live_api` with real keys for both providers.
3. Test on VirtualBox, VMware and at least two physical machines.
4. Native-speaker review of the Hebrew catalogue; Hebrew installation run.
5. Independent security review of `zionos-assistant`.
6. Exercise each recovery procedure and automate the important ones.
