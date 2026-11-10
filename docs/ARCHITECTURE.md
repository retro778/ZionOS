# ZionOS architecture

## Summary

ZionOS is a **Debian derivative**, not a new operating system core.

| Layer | Source | Notes |
|---|---|---|
| Kernel, glibc, systemd, APT/dpkg | Debian 13 "trixie" (stable), unmodified | Security updates come straight from `security.debian.org` |
| Desktop | KDE Plasma 6 from Debian (`main`) | SDDM login, KWin compositor, Dolphin, Konsole, Discover |
| Installer | Calamares from Debian | ZionOS branding + configuration only |
| Image build | Debian `live-build` | `iso-hybrid` image; GRUB for both legacy BIOS and UEFI |
| ZionOS additions | this repository | branding, defaults, En/He localisation defaults, Welcome / Settings / Assistant apps (C++20, Qt 6, CMake), packaged as `.deb` |

We do **not** implement a kernel, package manager, browser, crypto library or
compositor. TLS uses Qt Network over the system OpenSSL; credentials use the
freedesktop Secret Service (KWallet's implementation on Plasma) via QtKeychain.

## Decisions (recorded 2026-11-10)

1. **Base release: Debian 13 "trixie", point release 13.7.**
   Verified from `https://deb.debian.org/debian/dists/stable/Release`
   (`Suite: stable`, `Version: 13.7`, `Codename: trixie`,
   `Date: Sat, 12 Sep 2026`). The image tracks the `trixie`,
   `trixie-updates` and `trixie-security` suites; no Ubuntu or other Debian
   suites are ever mixed in.
2. **Architecture:** `amd64` (x86_64) only for now.
3. **Archive areas:** `main contrib non-free-firmware`. `non-free-firmware`
   is enabled, as on official Debian media, so that common Wi-Fi/GPU hardware
   works; `non-free` is not enabled.
4. **Build isolation:** the development host is itself Debian 13 but has no
   passwordless root and must not be modified. All building and testing runs
   in a Docker container built from `debian:trixie`
   (`build/docker/Dockerfile`). `live-build` needs `mount`/`chroot`, so the
   ISO build container runs with `--privileged`; it only touches the
   repository's `work/` and `out/` directories. QEMU runs inside the same
   image with `/dev/kvm` passed through.
5. **Boot:** GRUB (`grub-pc` + `grub-efi`) for BIOS and UEFI with one shared
   themed menu. Secure Boot uses Debian's signed shim/GRUB/kernel unchanged.
6. **Identity:** `zionos-base` diverts `/usr/lib/os-release`
   (`ID=zionos`, `ID_LIKE=debian`). `VERSION_CODENAME` stays `trixie` so
   third-party tooling that derives an APT suite from it keeps working. All
   Debian packages keep their names, versions and maintainers.
7. **Custom code:** C++20 / Qt 6 Widgets / CMake, one source package
   (`zionos-apps`) with a shared static core library; shell only for build
   glue and maintainer scripts.
8. **Testing:** unit tests (Qt Test, mock HTTP server) in the package build;
   VM integration tests in QEMU/KVM driven through QMP and the QEMU guest
   agent (`tests/vm`).

## Repository layout

```
build/docker/      builder container definition
distro/            live-build configuration (package lists, hooks, overlays, boot menu)
packages/          Debian source packages for ZionOS components
  zionos-system/   base identification, artwork, desktop defaults, installer config
  zionos-apps/     C++/Qt applications (Welcome, Settings, Assistant, About)
scripts/           documented entry points (env, packages, ISO, QEMU, tests, release)
tests/             VM integration tests and helpers
docs/              architecture, build, security, install, recovery, release, status
```

## Trust boundaries

See `docs/SECURITY.md`.
