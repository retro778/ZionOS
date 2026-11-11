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
| 7 | ZionOS packages delivered as `.deb`; authenticated update mechanism | **met, with caveats** | signed repository at `https://freedomnet.tech/zionos`, verified from live and installed systems; software key held by one person, single server (`docs/RELEASE.md`) |
| 8 | Signed release artefacts | **met, with caveats** | `SHA256SUMS.asc` signed with the archive key; key is not hardware-backed |
| 9 | English and Hebrew: selectable at setup and later; typing, RTL, mixed text | **met for ZionOS components (QEMU)** | `hebrew-bios`, unit tests; upstream gaps in `docs/LOCALIZATION.md`; no native-speaker review |
| 10 | AI assistant: both providers, streaming, cancel, errors, no retries, key handling | **met against mocks; not verified against the real APIs** | `tst_providers`, `tst_chat`, `hebrew-bios`, keyring check in install suites |
| 11 | OS fully usable without AI, keys or the assistant package | **met** | all suites run without keys; "removing the assistant removes nothing else" |
| 12 | Security review | **not met** | self-review only (`docs/SECURITY.md`); no independent review |
| 13 | Licensing and attribution documented | **met** | `THIRD-PARTY.md`; original artwork only |
| 14 | Recovery procedures | **documented, mostly unexercised** | `docs/RECOVERY.md` |
| 15 | VM compatibility: QEMU, VirtualBox, VMware | **QEMU verified; VirtualBox and VMware untested** | `docs/VM.md` |
| 16 | Physical hardware | **untested** | — |
| 17 | Reproducible build | **repeatable, not bit-for-bit reproducible** | `docs/BUILD.md` |
| 18 | CI | **partly met** | unit + minimal-ISO boot jobs pass on GitHub Actions; full desktop install job is manual and not yet run there |

Blocking for a production label: 10 (real APIs), 12, 14, 15, 16; 7 and 8 need a hardware-backed key and a second maintainer to be considered robust.

## Automated test results

<!-- RESULTS-BEGIN -->
Final run on the published image, 2026-11-11 (QEMU 10.0.13, KVM):

```
ZionOS test run 2026-11-11T00:02:45Z
ISO: zionos-0.1.0-amd64.iso  sha256=085a1369662310aafc5743c07cab74637d010e7a23aff3403a67c59e4143d8e1
git: 8544a73  kvm: yes
qemu: QEMU emulator version 10.0.13 (Debian 1:10.0.13+ds-0+deb13u1)
```

| Suite | Checks passed | Failed | Result |
|---|---|---|---|
| unit (5 Qt Test suites, ASan/UBSan, -Werror, cppcheck, catalogue) | 5 of 5 suites | 0 | pass |
| live-bios | 37 | 0 | pass (re-run, see note) |
| live-uefi | 37 | 0 | pass |
| live-uefi-secureboot | 13 | 0 | pass |
| hebrew-bios | 18 | 0 | pass |
| install-uefi-secureboot | 50 | 0 | pass |
| update-uefi-secureboot | 13 | 0 | pass |
| install-bios | 49 | 0 | pass |

**Note on the re-run.** `live-bios` first reported one failure caused by the test itself (it misread `apt-cache policy` output for the new update-channel check). The check was corrected and the suite re-run on the same ISO: 37/37. The later suites in the same run already used the corrected check.

**Intermittent failures seen in earlier runs** (not reproduced in this run): once the live desktop had not started 240 s after boot under UEFI Secure Boot; once the guest agent did not answer within 300 s after a post-upgrade reboot. Causes unknown; see `docs/KNOWN-ISSUES.md`.

The live and installed systems in these runs fetched and verified the real ZionOS repository at `https://freedomnet.tech/zionos`.

Raw logs and screenshots: `out/test-results/` (not committed); condensed copies: `docs/evidence/`.
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

1. Move the archive key to a hardware token with an offline primary key;
   add `Valid-Until` and a second mirror; deliver a first real update (0.1.1).
2. Run `tst_live_api` with real keys for both providers.
3. Test on VirtualBox, VMware and at least two physical machines.
4. Native-speaker review of the Hebrew catalogue; Hebrew installation run.
5. Independent security review of `zionos-assistant`.
6. Exercise each recovery procedure and automate the important ones.
