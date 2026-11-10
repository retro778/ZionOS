#!/usr/bin/env python3
"""Boot the ISO through its real boot loader (BIOS and/or UEFI) and verify
the live system from inside via the QEMU guest agent.

Usage: test_live_boot.py ISO [--firmware bios|uefi|both] [--desktop] [--out DIR]
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from zvm import VM, Report, ocr, wait_for  # noqa: E402


def run(iso, firmware, desktop, outdir):
    rep = Report("live boot (%s)" % firmware)
    wd = os.path.join(outdir, "live-" + firmware)
    with VM(name="zionos-live-" + firmware, iso=iso, firmware=firmware,
            workdir=wd, audio=True) as vm:
        # Boot menu: capture it for the branding check, then let the default
        # entry start (Enter skips the timeout).
        time.sleep(6 if firmware == "bios" else 12)
        shot = vm.screenshot(os.path.join(wd, "bootmenu.png"))
        text = ocr(shot)
        rep.check("boot menu shows ZionOS", "ZionOS" in text, text.replace("\n", " ")[:160])
        vm.key("ret")
        try:
            vm.wait_agent(timeout=420)
            rep.check("live system reached guest agent", True)
        except Exception as e:  # noqa: BLE001
            vm.screenshot(os.path.join(wd, "boot-failure.png"))
            rep.check("live system reached guest agent", False, str(e))
            return rep.finish()

        rc, out, _ = vm.guest(". /etc/os-release; echo $ID/$ID_LIKE/$VERSION_CODENAME/$PRETTY_NAME")
        rep.check("os-release identifies ZionOS on Debian trixie",
                  out.startswith("zionos/debian/trixie/ZionOS"), out)
        rc, out, _ = vm.guest("[ -d /sys/firmware/efi ] && echo uefi || echo bios")
        rep.check("firmware mode is %s" % firmware, out.strip() == firmware.split("-")[0], out)
        if firmware == "uefi-secureboot":
            rc, out, _ = vm.guest("od -An -t u1 /sys/firmware/efi/efivars/SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c "
                                  "| awk '{print $NF}'; mokutil --sb-state 2>&1 | head -1; "
                                  "dmesg | grep -i -m2 'secure boot\\|lockdown'")
            rep.check("Secure Boot is enforced and the Debian-signed boot chain is accepted",
                      out.split()[:1] == ["1"], out.replace("\n", " | "))
        rc, out, _ = vm.guest("uname -r; dpkg --print-architecture")
        rep.check("kernel is Debian amd64", "amd64" in out, out.replace("\n", " "))
        rc, out, _ = vm.guest("grep -c boot=live /proc/cmdline; findmnt -no FSTYPE /")
        rep.check("running as live system (overlay root)", "overlay" in out, out.replace("\n", " "))
        wait_for(lambda: vm.guest("systemctl is-system-running")[1].strip()
                 in ("running", "degraded"), 180, what="systemd startup")
        rc, out, _ = vm.guest("systemctl is-system-running; systemctl --failed --no-legend")
        rep.check("no failed systemd units", out.strip() == "running", out.replace("\n", " | "))
        rc, out, _ = vm.guest("for i in 1 2 3 4 5 6; do getent hosts deb.debian.org && break; sleep 5; done")
        rep.check("networking: DNS resolves deb.debian.org", rc == 0, out)
        rc, out, err = vm.guest("curl -sSI --max-time 20 https://deb.debian.org/debian/dists/trixie/Release | head -1")
        rep.check("networking: HTTPS to Debian mirror", " 200" in out, out + err)
        rc, out, _ = vm.guest("apt-cache policy | grep -E 'o=|l=' | sort -u")
        rep.check("APT sources are Debian trixie only",
                  "n=trixie" in out and "Ubuntu" not in out, out.replace("\n", " | "))
        rc, out, _ = vm.guest("id zion; sudo -l -U zion | tail -1")
        rep.check("live user 'zion' exists", "uid=1000(zion)" in out, out.replace("\n", " | "))
        if desktop:
            import desktop_checks
            desktop_checks.run(vm, rep, wd)
        vm.screenshot(os.path.join(wd, "final.png"))
        # Clean shutdown through ACPI.
        try:
            vm.guest("systemctl poweroff", timeout=20)
        except Exception:  # noqa: BLE001 - the agent channel closes as the guest goes down
            pass
        rep.check("clean shutdown", vm.wait_exit(120))
    return rep.finish()


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("iso")
    ap.add_argument("--firmware", default="both", help="bios | uefi | uefi-secureboot | both")
    ap.add_argument("--desktop", action="store_true")
    ap.add_argument("--out", default="/src/out/test-results")
    a = ap.parse_args()
    rc = 0
    for fw in (["bios", "uefi"] if a.firmware == "both" else [a.firmware]):
        rc |= run(a.iso, fw, a.desktop, a.out)
    sys.exit(rc)
