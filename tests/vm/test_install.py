#!/usr/bin/env python3
"""Install ZionOS from the ISO to a blank qcow2 disk with the graphical
installer, then boot the installed system and verify it.

The installer is driven like a user would: the Welcome window's "Install
ZionOS" button, keyboard and mouse input sent through QMP, and OCR of
screenshots to confirm each page. The virtual disk is a file created by this
test; no host device is touched.

Usage: test_install.py ISO [--firmware bios|uefi] [--out DIR] [--keep-disk]
"""
import argparse
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
import desktop_checks  # noqa: E402
from zvm import VM, Report, wait_for  # noqa: E402

USER, PASSWORD, HOST = "tester", "ziontest2026", "zionvm"


def install(iso, disk, firmware, wd, rep):
    with VM(name="zionos-install-" + firmware, iso=iso, disk=disk, firmware=firmware, workdir=wd) as vm:
        time.sleep(6 if firmware == "bios" else 10)
        vm.key("ret")
        vm.wait_agent(420)
        desktop_checks.wait_desktop(vm, "zion")
        rep.check("install: live desktop is up", True)
        rc, out, _ = vm.guest("lsblk -dno NAME,SIZE,TYPE | grep disk")
        rep.check("install: exactly one blank target disk (vda)", out.split()[:1] == ["vda"], out)

        ok = vm.wait_text("Welcome to ZionOS", 120) and vm.click_text("Install ZionOS", timeout=30)
        if not ok:  # fall back to the launcher
            vm.key("alt", "f2"); time.sleep(3); vm.type("zionos-install"); time.sleep(2); vm.key("ret")
        rep.check("install: installer starts from the Welcome window", ok)
        ok = vm.wait_text("installer for ZionOS", 120)
        vm.screenshot(os.path.join(wd, "01-welcome.png"))
        if not rep.check("install: Calamares welcome page with ZionOS branding", ok):
            return False
        vm.key("alt", "n")
        rep.check("install: location page", vm.wait_text("Region", 60) or vm.wait_text("Zone", 10))
        vm.key("alt", "n")
        rep.check("install: keyboard page", vm.wait_text("Keyboard model", 60))
        vm.key("alt", "n")
        ok = vm.wait_text("Erase disk", 90)
        text = vm.screen_text()
        rep.check("install: partition page names the target disk", ok and "/dev/vda" in text,
                  text.replace("\n", " ")[:160])
        vm.click_text("Erase disk", dx=-70)
        time.sleep(4)
        vm.screenshot(os.path.join(wd, "02-partition.png"))
        vm.key("alt", "n")
        if not rep.check("install: users page", vm.wait_text("What is your name", 60)):
            return False
        vm.click_text("What is your name", dy=27)
        vm.type("Tester")
        vm.key("tab"); vm.key("tab")
        vm.key("ctrl", "a"); vm.type(HOST)
        vm.key("tab"); vm.type(PASSWORD)
        vm.key("tab"); vm.type(PASSWORD)
        time.sleep(2)
        vm.screenshot(os.path.join(wd, "03-users.png"))
        vm.key("alt", "n")
        ok = vm.wait_text("overview of what will happen", 60)
        text = vm.screen_text()
        vm.screenshot(os.path.join(wd, "04-summary.png"))
        flat = " ".join(text.split())
        if not rep.check("install: summary identifies the disk to be erased",
                         ok and "Erase disk /dev/vda" in flat, flat[:200]):
            return False
        vm.key("alt", "i")
        if vm.wait_text("Install now", 20):
            vm.screenshot(os.path.join(wd, "05-confirm.png"))
            rep.check("install: explicit confirmation before writing", True)
            vm.click_text("Install now") or vm.key("alt", "i")
        else:
            rep.check("install: explicit confirmation before writing", False, "no confirmation dialog seen")
        t0 = time.time()
        done = vm.wait_text("All done", 2400, interval=20)
        vm.screenshot(os.path.join(wd, "06-finished.png"))
        if not rep.check("install: installer reports completion", done,
                         "after %ds: %s" % (time.time() - t0, " ".join(vm.screen_text().split())[:200])):
            vm.guest("cp /root/.cache/calamares/session.log /tmp/ 2>/dev/null; true")
            rc, out, _ = vm.guest("tail -40 /root/.cache/calamares/session.log")
            print(out)
            return False
        print("installation took %d s" % (time.time() - t0), flush=True)
        rc, out, _ = vm.guest("lsblk -no NAME,FSTYPE,SIZE /dev/vda | tr '\\n' ';'")
        rep.check("install: target disk partitioned", "ext4" in out, out)
        try:
            vm.guest("systemctl poweroff", timeout=20)
        except Exception:  # noqa: BLE001
            pass
        vm.wait_exit(120)
    return True


FAKE_KEY = "sk-ant-test-vm-0000000000000000"


def keyring_check(vm, rep, wd):
    """Save a (fake) API key through the assistant's settings dialog and
    confirm it is stored in KDE Wallet - unlocked by the login password via
    PAM - and nowhere in plain text."""
    vm.guest("pkill -u %s -f '[z]ionos-(welcome|settings|assistant)'; true" % USER)
    vm.key("alt", "f2"); time.sleep(3)
    vm.type("zionos-assistant"); time.sleep(2); vm.key("ret"); time.sleep(9)
    vm.key("ctrl", "comma"); time.sleep(6)       # Settings and API keys
    vm.type(FAKE_KEY); vm.key("ret"); time.sleep(6)
    vm.screenshot(os.path.join(wd, "11-key-saved.png"))
    vm.key("esc"); time.sleep(1)
    q = ("export DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus QT_QPA_PLATFORM=offscreen; "
         "kwallet-query -r anthropic-api-key -f zionos-assistant kdewallet 2>&1 | head -1")
    rc, out, _ = vm.guest(q, user=USER)
    rep.check("keyring: API key saved from the UI is stored in KDE Wallet", out.strip() == FAKE_KEY, out[:120])
    rc, out, _ = vm.guest("grep -rl '%s' /home/%s /tmp /var/log 2>/dev/null | head -3; "
                          "pgrep -u %s -a kwalletd6 | grep -c pam-login" % (FAKE_KEY, USER, USER))
    rep.check("keyring: no plaintext copy of the key on disk; wallet unlocked by login",
              out.strip() == "1", out.replace("\n", " | "))
    vm.guest("pkill -u %s -f '[z]ionos-assistant'; true" % USER)


def verify(disk, firmware, wd, rep, vars_file):
    with VM(name="zionos-installed-" + firmware, disk=disk, firmware=firmware, workdir=wd,
            vars_file=vars_file, audio=True) as vm:
        try:
            vm.wait_agent(300)
        except Exception as e:  # noqa: BLE001
            vm.screenshot(os.path.join(wd, "installed-boot-failure.png"))
            rep.check("installed: boots from disk without the ISO", False, str(e))
            return
        rep.check("installed: boots from disk without the ISO", True)

        def g(cmd, **kw):
            return vm.guest(cmd, **kw)

        rc, out, _ = g(". /etc/os-release; echo $ID/$ID_LIKE/$VERSION_CODENAME; findmnt -no SOURCE,FSTYPE /; "
                       "grep -c boot=live /proc/cmdline; [ -d /sys/firmware/efi ] && echo uefi || echo bios")
        o = out.split()
        rep.check("installed: ZionOS on its own ext4 root, not live",
                  o[0] == "zionos/debian/trixie" and "/dev/vda" in o[1] and o[2] == "ext4" and o[3] == "0"
                  and o[4] == firmware, out.replace("\n", " "))
        wait_for(lambda: g("systemctl is-system-running")[1].strip() in ("running", "degraded"), 180,
                 what="systemd startup")
        rc, out, _ = g("systemctl is-system-running; systemctl --failed --no-legend")
        rep.check("installed: no failed systemd units", out.strip() == "running", out.replace("\n", " | "))
        rc, out, _ = g("dpkg -l live-boot live-config calamares calamares-settings-debian zionos-calamares-settings "
                       "2>/dev/null | grep -c '^ii'; ls /usr/share/applications | grep -c -i -E 'calamares|zionos-install'")
        rep.check("installed: live and installer packages removed", out.split() == ["0", "0"], out)
        rc, out, _ = g("getent passwd %s | cut -d: -f3; id -nG %s; getent passwd zion user live | wc -l; "
                       "passwd -S root | cut -d' ' -f2; hostname" % (USER, USER))
        o = out.split("\n")
        rep.check("installed: only the user created in the installer; root locked; hostname set",
                  o[0] == "1000" and "sudo" in o[1].split() and o[2] == "0" and o[3] == "L" and o[4] == HOST,
                  out.replace("\n", " | "))
        rc, out, _ = g("grep -rs -i autologin /etc/sddm.conf /etc/sddm.conf.d/ | grep -v '^#' | grep -i 'User=.' ; "
                       "sudo -l -U %s | grep -c NOPASSWD; ls /etc/sudoers.d/" % USER)
        rep.check("installed: no autologin, no passwordless sudo", "User=" not in out and out.split()[0] == "0", out)
        rc, out, _ = g("grep -rhs '^deb ' /etc/apt/sources.list /etc/apt/sources.list.d/ | sort -u; "
                       "grep -rhs -E '^(URIs|Suites):' /etc/apt/sources.list.d/ | sort -u")
        rep.check("installed: APT uses Debian trixie + security, no CD-ROM",
                  "trixie" in out and "security" in out and "cdrom" not in out and "ubuntu" not in out.lower(),
                  out.replace("\n", " | ")[:300])
        rc, out, err = g("apt-get update 2>&1 | tail -3; apt-get -s -y dist-upgrade | grep -E '^[0-9]+ upgraded'",
                         timeout=300)
        rep.check("updates: apt-get update succeeds and an upgrade can be computed",
                  "upgraded" in out and "Err" not in out, (out + err).replace("\n", " | ")[:300])
        rc, out, err = g("DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends sl 2>&1 | tail -2; "
                         "dpkg -s sl | grep Status", timeout=300)
        rep.check("packages: installing a Debian package with APT works", "install ok installed" in out, out + err)
        rc, out, _ = g("grep -c 'ZionOS' /boot/grub/grub.cfg; ls /boot/efi/EFI 2>/dev/null | tr '\\n' ' '")
        rep.check("installed: GRUB menu entries are branded ZionOS", int(out.split()[0]) > 0, out)

        # Log in through the SDDM greeter with the password chosen during installation.
        time.sleep(15)
        vm.screenshot(os.path.join(wd, "10-login-screen.png"))
        vm.type(PASSWORD)
        vm.key("ret")
        try:
            desktop_checks.wait_desktop(vm, USER, 180)
            logged_in = True
        except Exception:  # noqa: BLE001
            logged_in = False
            vm.screenshot(os.path.join(wd, "login-failure.png"))
        if rep.check("installed: graphical login with the installed user's password", logged_in):
            desktop_checks.run(vm, rep, wd, user=USER, live=False)
            keyring_check(vm, rep, wd)

        # Restart, then a clean shutdown.
        try:
            g("systemctl reboot", timeout=20)
        except Exception:  # noqa: BLE001
            pass
        time.sleep(20)
        try:
            vm.wait_agent(300)
            rc, out, _ = g("uptime -s; findmnt -no SOURCE /")
            rep.check("installed: restart returns to the installed system", "/dev/vda" in out, out.replace("\n", " "))
        except Exception as e:  # noqa: BLE001
            rep.check("installed: restart returns to the installed system", False, str(e))
        try:
            g("systemctl poweroff", timeout=20)
        except Exception:  # noqa: BLE001
            pass
        rep.check("installed: clean shutdown", vm.wait_exit(180))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("iso")
    ap.add_argument("--firmware", default="uefi")
    ap.add_argument("--out", default="/src/out/test-results")
    ap.add_argument("--keep-disk", action="store_true")
    ap.add_argument("--verify-only", help="skip installation and verify this existing disk image")
    a = ap.parse_args()
    wd = os.path.join(a.out, "install-" + a.firmware)
    os.makedirs(wd, exist_ok=True)
    rep = Report("install + first boot (%s)" % a.firmware)
    vars_file = os.path.join(wd, "OVMF_VARS.fd")
    disk = a.verify_only or os.path.join("/src/work", "install-%s.qcow2" % a.firmware)
    ok = True
    if not a.verify_only:
        for f in (disk, vars_file):
            if os.path.exists(f):
                os.unlink(f)
        subprocess.run(["qemu-img", "create", "-f", "qcow2", disk, "24G"], check=True, stdout=subprocess.DEVNULL)
        ok = install(a.iso, disk, a.firmware, wd, rep)
    if ok:
        verify(disk, a.firmware, wd, rep, vars_file)
    rc = rep.finish()
    if rc == 0 and not a.keep_disk and not a.verify_only:
        os.unlink(disk)
    sys.exit(rc)


if __name__ == "__main__":
    main()
