# Building and testing ZionOS

## Build host

Inspected host (2026-11-10): Debian 13.7, x86_64, 8 CPUs, 15 GiB RAM,
Docker 29.9, `/dev/kvm` accessible, no passwordless root.

The build never needs root on the host and never touches host disks, the
host boot loader or host packages. Everything runs in a Docker container:

* `build/docker/Dockerfile` — `debian:trixie` plus live-build, the Qt 6
  toolchain, QEMU/OVMF and test tools. Same release as the target, so custom
  packages are compiled against the libraries they will run with.
* `scripts/build-iso.sh` runs the container with `--privileged` because
  live-build must `mount` and `chroot`. It writes only to `work/` and `out/`
  in this repository. (Files there are owned by root; remove them with
  `scripts/in-builder.sh rm -rf work/lb-desktop`.)
* VM tests pass `/dev/kvm` into the container. Virtual disks are qcow2
  *files* under `work/`.

Any other host with Docker and KVM should work; it has only been run on the
host above.

## Commands

| Step | Command | Result |
|---|---|---|
| 1. Environment | `scripts/env-prepare.sh` | image `zionos-builder:trixie`, `out/builder-tool-versions.txt` |
| 2. Packages | `scripts/build-packages.sh [zionos-system\|zionos-apps]` | `out/packages/*.deb`, flat APT repo `out/repo/` |
| 3. ISO | `scripts/build-iso.sh [desktop\|minimal]` | `out/zionos-<ver>[-minimal]-amd64.iso` + `.packages` manifest |
| 4. Run | `scripts/run-qemu.sh [--uefi] [--disk work/x.qcow2] [--no-iso]` | QEMU window (host QEMU) or VNC `127.0.0.1:5901` (container) |
| 5. Test | `scripts/test.sh unit\|live\|install\|update\|all` | results + screenshots in `out/test-results/` |
| 6. Release data | `scripts/release-metadata.sh` | `SHA256SUMS`, `SHA512SUMS`, `*.buildinfo.json` |

Useful variables: `ZIONOS_CLEAN=1` (discard the cached chroot),
`ZIONOS_MIRROR=http://…/debian/` (build-time mirror), `ZIONOS_SIGN_KEY`
(sign the APT repo / checksums), `DEB_BUILD_OPTIONS=nocheck`.

Create a disk for a manual installation test:

```sh
scripts/in-builder.sh qemu-img create -f qcow2 work/disk.qcow2 32G
scripts/run-qemu.sh --uefi --disk work/disk.qcow2
```

## What the ISO build does

`live-build` (`distro/auto/config`) bootstraps Debian trixie from
`deb.debian.org` + `security.debian.org`, installs the package lists of the
chosen profile (`distro/profiles/<profile>/*.list.chroot`), installs the
ZionOS packages from `config/packages.chroot/`, runs the hooks in
`distro/config/hooks/normal/`, and produces an `iso-hybrid` image with GRUB
for BIOS and UEFI (`distro/config/bootloaders/grub-pc`).

## Tests

* **Unit tests** (`tests/unit.sh`, also run by `dh_auto_test` during the
  package build): Qt Test suites in `packages/zionos-apps/tests`, built with
  `-Werror`, AddressSanitizer and UBSan, followed by cppcheck. Provider tests
  use a scripted local HTTP server; no network, no keys.
* **Live boot** (`tests/vm/test_live_boot.py`): boots the ISO through its own
  boot loader under SeaBIOS and OVMF, checks from inside the guest via the
  QEMU guest agent.
* **Install** (`tests/vm/test_install.py`): drives the graphical installer
  with keyboard/mouse events and OCR, installs to a blank qcow2, boots the
  result, logs in through SDDM, restarts, shuts down.
* **Update** (`tests/vm/test_update.py`): Debian `dist-upgrade`, then a
  ZionOS update from a signed test repository, plus rejection of tampered,
  wrongly signed and unsigned repositories.
* **Real provider APIs** (optional, billable, never in CI):
  `ZIONOS_LIVE_API_TESTS=1 ANTHROPIC_API_KEY=… OPENAI_API_KEY=… ctest -L live`
  inside a build tree. Export the keys in your own shell; never commit them.

## Repeatability and provenance

Recorded per build: the exact package manifest (`*.packages`), git commit,
builder image id and tool versions (`*.buildinfo.json`,
`out/builder-tool-versions.txt`). `SOURCE_DATE_EPOCH` is set from the last
commit.

Not pinned: the Debian mirror state. Two builds on different days can
contain different package versions (normally newer security fixes). Pinning
would require building against `snapshot.debian.org` (set `ZIONOS_MIRROR`
to a snapshot URL); this has not been exercised. The build is therefore a
**repeatable process, not a proven bit-for-bit reproducible build**.

## Continuous integration

`.github/workflows/ci.yml` builds the container, runs the unit tests and
static analysis, builds the packages and the minimal ISO and boots it with
QEMU (TCG if KVM is unavailable). The full desktop install test needs KVM
and ~40 minutes; it is defined as a manually triggered job (`workflow_dispatch` with
`full: true`). The `unit` and `minimal-iso` jobs run on every push and
passed on GitHub Actions on 2026-11-11; the manually triggered
`desktop-full` job has not been run there yet.
