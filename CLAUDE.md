# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ZionOS is a Debian 13 "trixie" derivative (KDE Plasma, English/Hebrew, optional AI assistant). The repo produces: `.deb` packages for ZionOS-specific components, a live/install ISO built with `live-build`, and a signed APT repository. It is a **development preview**; `docs/READINESS.md` and `docs/KNOWN-ISSUES.md` are the source of truth for what is and is not verified, and must stay honest (never claim something is tested, signed or production-ready unless it was actually run).

## Everything runs in the builder container

The host is never modified and has no passwordless sudo. All building and testing happens in the Docker image `zionos-builder:trixie` (`build/docker/Dockerfile`). The `scripts/*.sh` entry points re-exec themselves inside the container (`in_container` in `scripts/common.sh`; the repo is mounted at `/src`). Use `scripts/in-builder.sh <cmd>` for ad-hoc commands.

- `work/` (live-build trees, caches, test disks, build dirs) and `out/` (packages, ISO, test results) are untracked. Files under `work/` are root-owned: delete them with `scripts/in-builder.sh rm -rf work/...`.
- Docker options passed to `in_container` must come before a literal `--`.
- Don't use `pkill -f '<pattern>'` with a pattern that appears in your own command line (it kills the invoking shell); the tests use the `'[z]ionos-…'` bracket trick for this.

## Commands

```sh
scripts/env-prepare.sh                      # build the builder image
scripts/build-packages.sh [zionos-system|zionos-apps]   # -> out/packages/*.deb (runs unit tests via dh_auto_test)
scripts/build-iso.sh [desktop|minimal]      # -> out/zionos-<ver>[-minimal]-amd64.iso  (~20 min for desktop)
scripts/test.sh unit                        # C++ tests: -Werror, ASan/UBSan, catalogue freshness, cppcheck
scripts/test.sh live [bios|uefi|uefi-secureboot|both] [ISO] [--desktop]
scripts/test.sh hebrew [bios|uefi] [ISO]
scripts/test.sh install [bios|uefi|uefi-secureboot] [ISO]   # keeps work/install-<fw>.qcow2
scripts/test.sh update [bios|uefi|uefi-secureboot]          # needs the disk from `install`
scripts/test.sh all                         # everything (~70 min); summary in out/test-results/SUMMARY.txt
scripts/update-translations.sh              # lupdate + fill Hebrew strings
scripts/publish.sh [--iso] [--dry-run]      # sign locally, rsync to the update server
scripts/release-metadata.sh                 # checksums + buildinfo
```

Fast iteration on the C++ code without touching a running suite's build dir:

```sh
scripts/in-builder.sh env QUICK=1 ZIONOS_BUILD_DIR=/tmp/b2 tests/unit.sh     # QUICK=1 skips cppcheck; ZIONOS_GUI=OFF builds core only
scripts/in-builder.sh sh -c 'cd /src/work/build-unit && ctest -R tst_providers --output-on-failure'   # one suite
scripts/in-builder.sh sh -c 'cd /src/work/build-unit && QT_QPA_PLATFORM=offscreen tests/tst_core markdownDoesNotInterpretHtml'   # one test function
```

Interactive VM for debugging (headless; drive it with screenshots): `scripts/vm.sh start --iso /src/out/….iso [--disk …] [--firmware uefi]`, then `scripts/vm.sh shot /src/work/x.png | key alt f2 | type "text" | click X Y | exec [--user U] 'cmd' | wait-agent | stop`. Only one client can hold the QMP/guest-agent socket at a time, so don't run two `vm.sh` commands concurrently.

An ISO rebuild is required to test package changes in a VM; any change under `packages/` therefore means `build-packages.sh` → `build-iso.sh` → VM tests. Don't rebuild the ISO while a VM test is reading it.

## Architecture

**Two Debian source packages** under `packages/`:

- `zionos-system` (arch `all`): `zionos-base` (diverts `/usr/lib/os-release`; `VERSION_CODENAME` deliberately stays `trixie`), `zionos-artwork` (SVG sources rendered at build time), `zionos-desktop-settings` (kxkbrc us+il, locales, wallpaper, SDDM/GRUB defaults), `zionos-calamares-settings` (branding only; install logic comes from Debian's `calamares-settings-debian`, patched by `zionos-calamares-apply`), `zionos-archive-keyring` (public key, `Signed-By` APT source, pin restricting the ZionOS repo to `zionos-*`).
- `zionos-apps` (C++20, Qt 6 Widgets, CMake): `core/` is a static library with all logic; `ui/` shared widgets; `welcome/`, `settings/`, `assistant/` are thin executables; `tests/` are Qt Test suites.

**Image build** (`distro/`): `auto/config` holds the `lb config` flags; `profiles/<profile>/*.list.chroot` are the package lists; `config/hooks/normal/*.hook.chroot` run in the chroot (they restore the os-release symlink that live-build overwrites, apply Calamares branding, purge `plasma-welcome`); `config/bootloaders/grub-pc/` overrides the GRUB menu for both BIOS and UEFI. `build-iso.sh` copies all non-dbgsym `.deb`s from `out/packages` into `config/packages.chroot/`.

**Assistant design** (`packages/zionos-apps/core/ai/`) — invariants that tests enforce and changes must preserve:

- `ChatProvider`/`ChatStream` (`provider.h`) is the common interface; `AnthropicProvider` (Messages API, `anthropic-version: 2026-10-21`, no sampling params) and `OpenAiProvider` (Responses API, `store: false` by default) only translate formats. `HttpChatStream` owns transport: SSE parsing, idle timeout, error mapping, redirects refused, TLS verification.
- One `send` = exactly one HTTP request. **No automatic retries** (billable).
- `ChatController` holds conversation state independent of widgets; `previewRequest()` must equal what `send()` transmits (the "review what will be sent" dialog relies on it).
- `CredentialStore` uses QtKeychain with insecure fallback disabled; when the keyring is unavailable the only alternative is session-only (in-memory) storage. Never add a plaintext path.
- Model output is untrusted: rendered with `MarkdownNoHTML` in `SafeTextBrowser` (loads no resources, links need confirmation); shell blocks become copy-only cards, never executed.
- Provider base URLs are fixed; the only override is `ZIONOS_TEST_ANTHROPIC_URL` / `ZIONOS_TEST_OPENAI_URL`, honoured for loopback addresses only.
- Before changing a provider adapter, re-check the provider's current official API docs (`docs/AI-PROVIDERS.md` records what was verified and when); don't write endpoints, fields or model IDs from memory.

**Localisation**: every UI string goes through `tr()`; `translations/zionos_he.ts` is compiled by lrelease and `translations/fill_he.py` holds the Hebrew table. After adding or changing a string, add its Hebrew to `fill_he.py` and run `scripts/update-translations.sh` — `tests/unit.sh` fails if the catalogue is stale, incomplete, or a Hebrew string would get a left-to-right base direction (strings starting with Latin text or `%1` get a leading U+200F automatically). Wrap paths, hosts, model IDs and versions with `I18n::ltrIsolate()`.

**VM tests** (`tests/vm/`): `zvm.py` is the QEMU harness (QMP for screenshots/keys/mouse, QEMU guest agent for commands, tesseract OCR with `wait_text`/`click_text`). Tests drive the real GUI with key events and assert from inside the guest. `mock_provider.py` is copied into the guest as a stand-in Anthropic endpoint. The guest agent logs the command lines it runs into the guest journal — don't put a literal you later grep for into a guest command. GUI checks are OCR-based; when one fails, look at the screenshots in `out/test-results/<suite>/` before assuming a product bug.

**Release/update channel**: packages and indexes are signed on the release manager's machine (`scripts/publish.sh`, key fingerprint in `packages/zionos-system/keyring/fingerprint`) and rsynced to a static server; the server holds no key. Public URLs are in `release.conf`; the SSH login lives in the untracked `release.local.conf`. Publishing and pushing are outward-facing: do them only when asked.

## Toolchain quirks (Qt 6.8.2 in trixie)

- `lupdate` hangs on `translate()`/`tr()` calls whose plural argument is a nested call expression — compute the count into a variable first. Use `scripts/update-translations.sh`, not the CMake `update_translations` target.
- `moc` silently skips a file containing raw string literals that end in `")"`; build such test JSON with `QJsonDocument` instead.
- Product code compiles with `QT_NO_CAST_FROM_ASCII` and `-Wconversion -Werror`; test code relaxes both (`tests/CMakeLists.txt`).

## Conventions

- Version lives in `VERSION` and both `debian/changelog` files.
- Commits use the GitHub noreply identity configured in this repo; keep it that way (the public history intentionally contains no personal email).
- When a test fails intermittently or a claim can't be verified, record it in `docs/KNOWN-ISSUES.md` / `docs/READINESS.md` rather than dropping it.
