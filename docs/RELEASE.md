# Updates and release procedure

## How an installed system is updated

1. **Debian packages** (almost everything): from Debian's mirrors and
   security archive, authenticated by the Debian archive keys that ship in
   `debian-archive-keyring`. ZionOS adds nothing to this path.
2. **ZionOS packages** (`zionos-base`, `zionos-artwork`,
   `zionos-desktop-settings`, `zionos-apps-common`, `zionos-welcome`,
   `zionos-settings`, `zionos-assistant`): from a ZionOS APT repository,
   authenticated with a dedicated ZionOS archive key referenced by
   `Signed-By` (never added to the global trusted keys).

### Current state — incomplete

**There is no hosted ZionOS repository and no production signing key.**
Consequently release 0.1.0 images contain no ZionOS APT source: Debian
updates work, but ZionOS's own packages do not update automatically. This
release requirement is **open**.

What exists and is tested:

* `scripts/build-packages.sh` produces `out/repo/`, a flat APT repository
  (development use, unsigned unless `ZIONOS_SIGN_KEY` is set).
* `tests/vm/test_update.py` proves the mechanism end to end with a
  throw-away key: signed repo → `Signed-By` source → `apt upgrade` to a newer
  ZionOS package; tampered, wrongly-signed and unsigned repositories are
  rejected by APT.

### Design for the production repository

* Repository layout: `deb [signed-by=/usr/share/keyrings/zionos-archive-keyring.gpg] https://<host>/zionos aleph main`.
* A `zionos-archive-keyring` package ships the public key and the deb822
  source file; it is added to the image only once a real repository exists.
* Key handling: an offline primary key; a signing subkey on a hardware token
  used only by the release manager; yearly expiry with rollover through the
  keyring package (old and new keys overlap for one release).
* APT pinning (`Pin: release o=ZionOS`, priority 500, restricted to
  `zionos-*` package names) so the ZionOS repository can never replace a
  Debian package.

## Release procedure

1. `git status` clean; bump `VERSION` and both `debian/changelog` files.
2. `scripts/env-prepare.sh && scripts/build-packages.sh && ZIONOS_CLEAN=1 scripts/build-iso.sh`
3. `scripts/test.sh all` — every check must pass; attach
   `out/test-results/` to the release.
4. Manual checks from `docs/READINESS.md` that automation does not cover
   (VirtualBox, VMware, real hardware, Hebrew visual review).
5. `ZIONOS_SIGN_KEY=<key id> scripts/release-metadata.sh` → `SHA256SUMS`,
   `SHA256SUMS.asc`, `*.buildinfo.json`.
6. Publish ISO + checksums + signature + package manifest; publish signed
   packages to the repository; tag the commit (`git tag -s v<version>`).
7. Users verify: `gpg --verify SHA256SUMS.asc SHA256SUMS && sha256sum -c SHA256SUMS`.

Steps 5–6 cannot be completed today (no signing key, no hosting);
`scripts/release-metadata.sh` then writes unsigned checksums and marks the
build `"checksums_signed": false`.

## Release criteria

See `docs/READINESS.md`. A release may be called production-ready only when
every criterion there is met and verified.
