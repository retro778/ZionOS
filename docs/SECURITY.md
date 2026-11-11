# Security model

## Base system

* All base packages come from Debian `trixie`, `trixie-updates` and
  `trixie-security` over APT with Debian's archive signatures. ZionOS does
  not rebuild or patch them, so Debian security advisories apply unchanged.
* ZionOS's own packages come from `https://freedomnet.tech/zionos`, verified
  with a dedicated archive key (`Signed-By`) and pinned so that repository
  can only provide `zionos-*` packages. The server holds no signing key
  (`docs/RELEASE.md`).
* `unattended-upgrades` is installed (Debian default configuration: security
  updates).
* UEFI Secure Boot uses Debian's signed shim, GRUB and kernel. ZionOS ships
  no boot binaries of its own. (Secure Boot itself has **not** been tested;
  see `docs/READINESS.md`.)
* `non-free-firmware` is enabled for hardware support; `non-free` is not.

## Accounts

* **Installed system:** no default credentials. The installer requires a
  user name and a non-empty password; the root account is locked and
  administration goes through `sudo` for that user. Autologin is off unless
  the user ticks it. (Verified by the install test.)
* **Live session only:** user `zion`, password `live`, passwordless `sudo`,
  and automatic login. These are provided by Debian's `live-config` and exist
  only while running from the ISO; the installer removes the live packages
  and does not copy the live user.
* The QEMU guest agent is installed for VM integration. It is reachable only
  from the hypervisor through a virtio channel that the VM owner configures;
  on physical hardware the service does not start (no such device).

## ZionOS applications

They are ordinary, unprivileged Qt applications. Nothing ZionOS-specific
runs as root or as a daemon, and there are no setuid binaries or polkit
rules of our own. `zionos-assistant` refuses to start as root.

`zionos-settings` writes only per-user files (`~/.config/kxkbrc`,
`~/.config/plasma-localerc`, `~/.config/ZionOS/zionos.conf`) and runs
`flatpak --user` when asked.

## API keys

* Stored only in the desktop keyring through the freedesktop Secret Service /
  KWallet (QtKeychain, service `zionos-assistant`, entries
  `anthropic-api-key` and `openai-api-key`).
* QtKeychain's plaintext fallback is disabled (`setInsecureFallback(false)`).
  If the keyring is missing or locked, saving fails with an explanation and
  the assistant offers **session-only** storage: the key stays in the
  assistant's process memory and is gone when it closes. Nothing is written
  to disk in that case.
* Keys are never placed in the settings file, history, logs, the ISO or this
  repository. The key input field is cleared as soon as *Save* is pressed.
* `Redactor` removes key-shaped strings, `Authorization`/`x-api-key` values
  and every key seen at runtime from all error text before it is displayed.
* The assistant and the settings tool mark themselves non-dumpable
  (`PR_SET_DUMPABLE=0`, core size 0): no core dump or crash report can
  contain a key, and other processes of the same user cannot attach a
  debugger to read it.
* Keys are sent only as request headers to the provider's HTTPS endpoint.
  Redirects are refused, so a key cannot follow a redirect to another host.
* Tests use obviously fake keys; no real key exists anywhere in the tree.

## Network

* HTTPS with certificate verification against the system CA store
  (`QSslSocket::VerifyPeer`, TLS ≥ 1.2). Certificate errors abort the
  request; there is no "ignore" path in the code.
* Endpoints are fixed in the code. The only override is a test hook
  (`ZIONOS_TEST_ANTHROPIC_URL` / `ZIONOS_TEST_OPENAI_URL`) that is honoured
  **only for loopback addresses**, so it cannot redirect a key off the machine.
* No telemetry, crash upload or background network activity in any ZionOS
  component.

## Untrusted content

Model output, attached files and clipboard text are untrusted.

* Model output is rendered as Markdown with HTML disabled, in a widget whose
  resource loader always returns nothing (no remote images, no `file:` reads).
* Links are never followed automatically; only `http`/`https` links can be
  opened, after a confirmation that shows the full URL.
* The model has no tools. The application cannot execute commands, read
  files, or change settings on the model's behalf, so prompt injection in an
  attachment has nothing to trigger. Proposed commands are displayed with a
  warning and a copy button only; copied text has bidi controls and
  invisible characters removed (guards against "Trojan Source" style
  commands that display differently from what they execute).
* What the user sees in the review dialog is, by construction and by test,
  the text that is transmitted.

## Known limitations

* The assistant holds the key in process memory while running. Debugger
  attachment and core dumps are disabled, but malware already running as the
  same user has other ways to interfere (for example by reading the keyring
  once it is unlocked); that is outside what a desktop application can prevent.
* History is stored unencrypted in the user's home directory (mode 0600).
  Use full-disk encryption (offered by the installer) if that matters, or
  disable history.
* No independent security review has been performed.

## Reporting

There is no public tracker yet. Until one exists, report issues to the
maintainers of the repository you obtained ZionOS from.
