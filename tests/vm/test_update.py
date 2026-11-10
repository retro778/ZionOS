#!/usr/bin/env python3
"""Update an installed ZionOS system.

1. Debian updates: `apt-get update && apt-get dist-upgrade` from the Debian
   archive and security archive.
2. ZionOS updates: a newer zionos-system build is published in a signed APT
   repository (throw-away test key generated here), the installed system is
   pointed at it with a Signed-By source, and upgrades.
3. Authentication: a tampered repository and an unsigned repository are
   both rejected by APT.

Usage: test_update.py DISK.qcow2 [--firmware bios|uefi] [--vars OVMF_VARS.fd] [--out DIR]
The disk must contain an installed ZionOS (see test_install.py --keep-disk).
"""
import argparse
import functools
import http.server
import os
import shutil
import subprocess
import sys
import tempfile
import threading

sys.path.insert(0, os.path.dirname(__file__))
from zvm import VM, Report, wait_for  # noqa: E402

PORT = 8099
NEWVER = "0.1.1~test1"


def sh(cmd, **kw):
    return subprocess.run(cmd, shell=True, check=True, capture_output=True, text=True, **kw)


def build_repo(work):
    """Builds zionos-system at a higher version and a signed flat APT repo."""
    gnupg = os.path.join(work, "gnupg")
    os.makedirs(gnupg, mode=0o700)
    env = dict(os.environ, GNUPGHOME=gnupg)
    sh("gpg --batch --passphrase '' --quick-gen-key 'ZionOS TEST archive key (not for release)' ed25519 sign 1d", env=env)
    src = os.path.join(work, "src", "zionos-system")
    shutil.copytree("/src/packages/zionos-system", src, ignore=shutil.ignore_patterns("gen"))
    with open(os.path.join(src, "debian/changelog")) as f:
        old = f.read()
    with open(os.path.join(src, "debian/changelog"), "w") as f:
        f.write("zionos-system (%s) trixie; urgency=medium\n\n  * Test update.\n\n"
                " -- ZionOS Developers <zionos-devel@localhost>  Tue, 10 Nov 2026 12:00:00 +0000\n\n%s" % (NEWVER, old))
    sh("dpkg-buildpackage -us -uc -b", cwd=src)
    repo = os.path.join(work, "repo")
    os.makedirs(os.path.join(repo, "pool"))
    sh("cp %s/src/*.deb %s/pool/" % (work, repo))
    sign_repo(repo, env)
    key = os.path.join(work, "zionos-test-archive-keyring.gpg")
    sh("gpg --export > %s" % key, env=env)
    return repo, key, env


def sign_repo(repo, env):
    sh("apt-ftparchive packages pool > Packages && gzip -9nkf Packages && "
       "apt-ftparchive -o APT::FTPArchive::Release::Origin=ZionOS -o APT::FTPArchive::Release::Label=ZionOS "
       "-o APT::FTPArchive::Release::Suite=zionos-test release . > Release && "
       "rm -f InRelease Release.gpg && gpg --batch --yes --clearsign -o InRelease Release", cwd=repo, env=env)


def serve(repo):
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=repo)
    handler.log_message = lambda *a, **k: None
    srv = http.server.ThreadingHTTPServer(("0.0.0.0", PORT), handler)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("disk")
    ap.add_argument("--firmware", default="uefi")
    ap.add_argument("--vars")
    ap.add_argument("--out", default="/src/out/test-results")
    a = ap.parse_args()
    wd = os.path.join(a.out, "update-" + a.firmware)
    os.makedirs(wd, exist_ok=True)
    rep = Report("update installed system (%s)" % a.firmware)
    work = tempfile.mkdtemp(prefix="zupd-")
    repo, key, env = build_repo(work)
    rep.check("update: signed test repository built with zionos-base %s" % NEWVER,
              os.path.exists(os.path.join(repo, "InRelease")))
    srv = serve(repo)
    apt = "DEBIAN_FRONTEND=noninteractive apt-get -o Dpkg::Options::=--force-confold "
    with VM(name="zionos-update", disk=a.disk, firmware=a.firmware, workdir=wd, vars_file=a.vars) as vm:
        vm.wait_agent(300)
        wait_for(lambda: vm.guest("systemctl is-system-running")[1].strip() in ("running", "degraded"), 180,
                 what="systemd startup")

        def g(cmd, timeout=900):
            rc, out, err = vm.guest(cmd + " 2>&1", timeout=timeout)
            return rc, out + err

        rc, before = g("dpkg-query -W -f '${Version}' zionos-base")
        rep.check("update: installed zionos-base is older than the test update", before.strip() != NEWVER, before)

        # 1. Debian archive + security updates.
        rc, out = g(apt + "update")
        rep.check("update: apt-get update against Debian archives", rc == 0 and "Err:" not in out, out[-300:])
        rc, out = g(apt + "-y dist-upgrade", timeout=2400)
        rep.check("update: Debian dist-upgrade completes", rc == 0, out[-400:])
        rc, out = g("dpkg --audit; dpkg -l | grep -c -E '^(iU|iF|iH)'")
        rep.check("update: no half-configured packages afterwards", out.strip().endswith("0"), out[-200:])

        # 2. ZionOS repository with Signed-By.
        with open(key, "rb") as f:
            vm.guest_write("/usr/share/keyrings/zionos-test-archive-keyring.gpg", f.read())
        vm.guest_write("/etc/apt/sources.list.d/zionos-test.sources",
                       "Types: deb\nURIs: http://10.0.2.2:%d/\nSuites: ./\n"
                       "Signed-By: /usr/share/keyrings/zionos-test-archive-keyring.gpg\n" % PORT)
        rc, out = g(apt + "update")
        rep.check("update: signed ZionOS repository is accepted", rc == 0 and "NO_PUBKEY" not in out
                  and "not signed" not in out, out[-300:])
        rc, out = g("apt-cache policy zionos-base | sed -n 2,3p")
        rep.check("update: newer ZionOS package is the upgrade candidate", NEWVER in out, out)
        rc, out = g(apt + "-y upgrade", timeout=1200)
        rc2, after = g("dpkg-query -W -f '${Version} ${Status}' zionos-base zionos-artwork")
        rep.check("update: ZionOS packages upgraded through APT", rc == 0 and after.count(NEWVER) == 2
                  and "half" not in after, after + " | " + out[-200:])
        rc, out = g(". /etc/os-release; echo $ID $VERSION_ID; readlink /etc/os-release; "
                    "dpkg-divert --list | grep -c os-release; systemctl --failed --no-legend | wc -l")
        rep.check("update: identity and diversion intact after upgrade",
                  out.split()[:2] == ["zionos", "0.1"] and out.split()[-2:] == ["1", "0"], out.replace("\n", " "))

        # 3a. Tampered repository (index changed after signing).
        with open(os.path.join(repo, "Packages"), "a") as f:
            f.write("\n")
        sh("gzip -9nkf Packages", cwd=repo)
        rc, out = g("rm -rf /var/lib/apt/lists/10.0.2.2*; " + apt + "update")
        rep.check("update: tampered repository index is rejected",
                  "Hash Sum mismatch" in out or "File has unexpected size" in out or rc != 0, out[-300:])
        # 3b. Repository signed by an unknown key.
        other = os.path.join(work, "gnupg2")
        os.makedirs(other, mode=0o700)
        env2 = dict(os.environ, GNUPGHOME=other)
        sh("gpg --batch --passphrase '' --quick-gen-key 'Untrusted key' ed25519 sign 1d", env=env2)
        sign_repo(repo, env2)
        rc, out = g("rm -rf /var/lib/apt/lists/10.0.2.2*; " + apt + "update")
        rep.check("update: repository signed by an untrusted key is rejected",
                  "NO_PUBKEY" in out or "not signed" in out or "public key is not available" in out, out[-300:])
        # 3c. Unsigned repository.
        os.unlink(os.path.join(repo, "InRelease"))
        rc, out = g("rm -rf /var/lib/apt/lists/10.0.2.2*; " + apt + "update")
        rep.check("update: unsigned repository is rejected", "not signed" in out or "no longer signed" in out
                  or "does not have a Release file" in out, out[-300:])
        g("rm -f /etc/apt/sources.list.d/zionos-test.sources /usr/share/keyrings/zionos-test-archive-keyring.gpg; "
          + apt + "update")

        # The updated system still restarts.
        try:
            vm.guest("systemctl reboot", timeout=20)
        except Exception:  # noqa: BLE001
            pass
        import time
        time.sleep(20)
        try:
            vm.wait_agent(300)
            rc, out = g("systemctl is-system-running --wait; uname -r")
            rep.check("update: system restarts after the upgrade", "running" in out or "degraded" in out, out)
        except Exception as e:  # noqa: BLE001
            rep.check("update: system restarts after the upgrade", False, str(e))
        try:
            vm.guest("systemctl poweroff", timeout=20)
        except Exception:  # noqa: BLE001
            pass
        vm.wait_exit(120)
    srv.shutdown()
    shutil.rmtree(work, ignore_errors=True)
    sys.exit(rep.finish())


if __name__ == "__main__":
    main()
