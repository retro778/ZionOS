# Software compatibility

ZionOS provides compatibility **by being Debian 13 "trixie" on amd64**. What
runs on Debian 13 amd64 is expected to run on ZionOS; nothing beyond that is
promised.

## What works

* **Debian packages.** Any package from Debian trixie (`main`, `contrib`,
  `non-free-firmware`; other components if you enable them) via APT or
  Discover.
* **Third-party `.deb` files** built for Debian 13 amd64, if their
  dependencies are satisfiable from Debian 13. Install with
  `sudo apt install ./file.deb` so dependencies are resolved.
* **Building from source.** `build-essential`, `gdb` and `git` are installed;
  portable POSIX/C/C++ software builds as on Debian
  (`sudo apt build-dep <pkg>` or the usual `./configure && make`, CMake, Meson).
* **Flatpak** (optional). The tooling is installed, but no remote is
  configured. *ZionOS Settings → Software* can add Flathub for the current
  user: `flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo`.
  Flathub is a third party; its applications are not reviewed by Debian or
  ZionOS.

## What is not promised

* **Not every Linux binary runs.** Binaries need the amd64 architecture, a
  kernel interface no newer than the running kernel provides, and library
  versions (glibc 2.41, libstdc++ from GCC 14, …) no newer than Debian 13
  ships. Programs built for newer distributions can fail with
  `GLIBC_x.y not found`; 32-bit programs need `dpkg --add-architecture i386`
  and matching libraries.
* **Not every `.deb` installs.** Packages built for Ubuntu, other Debian
  releases or other architectures often have unsatisfiable or wrong
  dependencies. Forcing them can break the system.
* **Never mix repositories.** Do not add Ubuntu, Debian testing/unstable or
  other releases' APT sources (no "FrankenDebian"). ZionOS itself ships only
  Debian trixie sources. `VERSION_CODENAME=trixie` in `/etc/os-release` so
  that vendor instructions using `$(lsb_release -cs)` select the right suite;
  instructions that test `ID=debian` may need `ID_LIKE=debian` handling or
  manual adjustment.
* **No BSD, macOS or other UNIX binaries.** Different kernels and ABIs. Their
  *source code* can often be built if it is portable POSIX code.
* **Windows software** is out of scope (Wine is available from Debian but is
  not installed, configured or tested).
* **Kernel modules** from third parties (proprietary GPU drivers,
  VirtualBox host modules, …) must match the Debian kernel and, with Secure
  Boot, be signed by a key you enrol. Not tested.
* **Licensing.** `non-free-firmware` blobs are redistributable but not free
  software. Software you add from `non-free`, Flathub or vendors carries its
  own licence terms.

## Verified on the built image

Run by `tests/vm/desktop_checks.py` in the live session and on the installed
system (see `docs/READINESS.md` for results): Firefox ESR, LibreOffice
(headless document conversion with a Hebrew file name), Kate, core terminal
utilities, compiling and running a C and a C++20 program, and installing a
Debian package with APT on the installed system.
