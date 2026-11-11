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

### Current state

A signed ZionOS repository exists (set up 2026-11-10):

| | |
|---|---|
| Repository | `https://freedomnet.tech/zionos`, suite `aleph`, component `main`, `amd64` |
| ISO downloads | `https://freedomnet.tech/iso/` with `SHA256SUMS` + `SHA256SUMS.asc` |
| Archive key | "ZionOS Archive Signing Key", ed25519, fingerprint `46BB C312 B767 C1DE 8B98 65C3 85E6 DE9A 87D9 9AC3`, expires 2029-11-09 |
| Client side | package `zionos-archive-keyring`: public key in `/usr/share/keyrings/zionos-archive-keyring.asc`, source in `/etc/apt/sources.list.d/zionos.sources` (`Signed-By`), pin in `/etc/apt/preferences.d/zionos.pref` |

Properties:

* **The server holds no key.** Packages and indexes are built and signed on
  the release manager's machine (`scripts/publish.sh`) and uploaded with
  rsync over SSH. A compromised server can withhold or roll back to older
  signed files, but cannot forge packages.
* **Scoped trust.** The key is referenced only by `Signed-By` for this one
  source; it is not a globally trusted APT key.
* **Scoped content.** The pin gives everything from `o=ZionOS` priority -1
  except `zionos-*` packages (500), so the repository can never replace a
  Debian package even if its index listed one.
* The server (nginx, static files) also runs unrelated services of the
  maintainer; the repository is two static `location` blocks
  (`server/setup-zionos-repo.sh`).

Remaining weaknesses, stated plainly:

* The signing key is a software key in the maintainer's GnuPG keyring
  (passphrase-protected), **not on a hardware token and without an offline
  primary key**. Moving to a hardware token is recommended.
* A single person holds the key and the server account.
* No `Valid-Until` is set on the index, so a compromised or stale server
  could serve an old (validly signed) index indefinitely.
* One server, no mirror or CDN.
* Key backup is the maintainer's responsibility; if the key is lost, a new
  key cannot be delivered to installed systems automatically.

### Creating / rotating the archive key

    gpg --batch --quick-gen-key "ZionOS Archive Signing Key" ed25519 sign 3y
    gpg --armor --export <FPR> > packages/zionos-system/keyring/zionos-archive-keyring.asc
    echo <FPR> > packages/zionos-system/keyring/fingerprint

Back up the private key and the revocation certificate offline. To rotate:
generate the new key, ship **both** public keys in
`zionos-archive-keyring.asc` in a release signed by the old key, switch
signing to the new key in the following release, and drop the old key one
release later. Extend the expiry (`gpg --quick-set-expire`) and republish
the keyring package well before 2029-11-09.

### Publishing

    scripts/build-packages.sh
    scripts/publish.sh            # packages; asks for the key passphrase
    scripts/publish.sh --iso      # also ISO + signed checksums

`tests/vm/test_update.py` additionally proves the client-side behaviour with
a throw-away key: upgrade from a signed repository; tampered, wrongly-signed
and unsigned repositories rejected.

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

`scripts/publish.sh --iso` performs steps 5–6 for the ISO, checksums and
packages. Without a key, `scripts/release-metadata.sh` writes unsigned
checksums and marks the build `"checksums_signed": false`.

## Release criteria

See `docs/READINESS.md`. A release may be called production-ready only when
every criterion there is met and verified.
