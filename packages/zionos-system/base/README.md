# ZionOS

ZionOS is a desktop operating system based on Debian GNU/Linux.

* The Linux kernel, GNU C library, APT/dpkg and almost all software come
  unmodified from the Debian archive and receive Debian security updates.
* ZionOS adds branding, default settings, English/Hebrew localisation
  defaults and a small set of applications (Welcome, Settings, Assistant).

`VERSION_CODENAME` in `/etc/os-release` intentionally names the Debian base
suite so that tools which use it to choose an APT suite keep working. The
original Debian os-release file is preserved as `/usr/lib/os-release.debian`.

ZionOS is not affiliated with or endorsed by the Debian Project.
Debian is a registered trademark of Software in the Public Interest, Inc.
