# ZionOS

ZionOS is an installable Linux desktop operating system based on **Debian 13
"trixie"**, with **KDE Plasma**, first-class **English and Hebrew** support
and an optional desktop **AI assistant** for user-supplied OpenAI or
Anthropic API keys.

It is a Debian derivative: the kernel, glibc, APT/dpkg and nearly all
software are unmodified Debian packages that keep receiving Debian security
updates. ZionOS adds branding, defaults and three small C++/Qt 6
applications (Welcome, Settings, Assistant), all shipped as `.deb` packages.

> **Status:** development release 0.1.0 "Aleph". It is **not** declared
> production-ready. See [`docs/READINESS.md`](docs/READINESS.md) for what has
> and has not been verified, and [`docs/KNOWN-ISSUES.md`](docs/KNOWN-ISSUES.md).

Website and downloads: <https://retro778.github.io/ZionOS/>

## Quick start

Requirements on the build host: Docker, ~25 GB free disk, and `/dev/kvm`
for the VM tests. Nothing else is installed on, or changed in, the host.

```sh
scripts/env-prepare.sh        # 1. build the isolated builder image (Debian trixie)
scripts/build-packages.sh     # 2. build ZionOS .deb packages -> out/packages, out/repo
scripts/build-iso.sh          # 3. build the live/install ISO  -> out/zionos-<ver>-amd64.iso
scripts/run-qemu.sh           # 4. try it in QEMU (add --uefi, --disk FILE.qcow2)
scripts/test.sh all           # 5. unit tests + QEMU boot/install/update tests
scripts/release-metadata.sh   # 6. checksums + build provenance
```

Details: [`docs/BUILD.md`](docs/BUILD.md).

## Documentation

| Topic | File |
|---|---|
| Architecture and decisions | [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) |
| Building, testing, CI | [`docs/BUILD.md`](docs/BUILD.md) |
| Installing, live session credentials | [`docs/INSTALL.md`](docs/INSTALL.md) |
| Virtual machines and test matrix | [`docs/VM.md`](docs/VM.md) |
| Software compatibility and its limits | [`docs/COMPATIBILITY.md`](docs/COMPATIBILITY.md) |
| English/Hebrew, RTL, untranslated parts | [`docs/LOCALIZATION.md`](docs/LOCALIZATION.md) |
| AI assistant: providers and data flow | [`docs/AI-PROVIDERS.md`](docs/AI-PROVIDERS.md) |
| Security model | [`docs/SECURITY.md`](docs/SECURITY.md) |
| Updates, signing and release procedure | [`docs/RELEASE.md`](docs/RELEASE.md) |
| Recovery | [`docs/RECOVERY.md`](docs/RECOVERY.md) |
| Milestones, readiness, known issues | [`docs/MILESTONES.md`](docs/MILESTONES.md), [`docs/READINESS.md`](docs/READINESS.md), [`docs/KNOWN-ISSUES.md`](docs/KNOWN-ISSUES.md) |
| Licenses and attribution | [`THIRD-PARTY.md`](THIRD-PARTY.md) |

## License

ZionOS-specific code is licensed GPL-3.0-or-later (`LICENSE`); ZionOS
artwork is CC-BY-SA-4.0. Everything else in the image keeps its upstream
license; see [`THIRD-PARTY.md`](THIRD-PARTY.md).

ZionOS is not affiliated with or endorsed by the Debian Project, KDE,
OpenAI or Anthropic.
