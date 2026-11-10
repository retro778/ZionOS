"""Checks of the live or installed ZionOS desktop, run through the guest agent."""
import os
import time

from zvm import ocr, wait_for


def user_env(user):
    return ("export XDG_RUNTIME_DIR=/run/user/$(id -u %s) WAYLAND_DISPLAY=wayland-0 "
            "DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/$(id -u %s)/bus; " % (user, user))


def wait_desktop(vm, user, timeout=240):
    return wait_for(lambda: vm.guest("pgrep -u %s plasmashell" % user)[0] == 0, timeout,
                    what="plasmashell of %s" % user)


def run(vm, rep, outdir, user="zion", live=True):
    try:
        wait_desktop(vm, user)
        rep.check("desktop: Plasma session running for %s" % user, True)
    except Exception as e:  # noqa: BLE001
        vm.screenshot(os.path.join(outdir, "no-desktop.png"))
        rep.check("desktop: Plasma session running for %s" % user, False, str(e))
        return
    time.sleep(25)
    shot = vm.screenshot(os.path.join(outdir, "desktop.png"))
    text = ocr(shot)
    rep.check("desktop: ZionOS Welcome shown at first login", "Welcome to ZionOS" in text,
              text.replace("\n", " ")[:120])

    def g(cmd, **kw):
        return vm.guest(cmd, **kw)

    rc, out, _ = g("systemctl is-active sddm NetworkManager; loginctl show-user %s -p State" % user)
    rep.check("desktop: sddm and NetworkManager active", out.split()[:2] == ["active", "active"], out)
    rc, out, _ = g("for p in plasmashell kwin_wayland pipewire wireplumber; do pgrep -u %s -x $p >/dev/null "
                   "&& echo $p; done" % user)
    rep.check("desktop: panel/compositor/audio services running",
              set(out.split()) >= {"plasmashell", "kwin_wayland", "pipewire", "wireplumber"}, out)
    rc, out, err = g(user_env(user) + "wpctl status | sed -n '/Sinks:/,/Sources:/p' | head -5", user=user)
    rep.check("audio: PipeWire has an output sink", "Sinks" in out and ("Audio" in out or "Speaker" in out
              or "Output" in out or "alsa" in out.lower() or "*" in out), (out + err).replace("\n", " "))
    rc, out, _ = g("for b in dolphin konsole systemsettings plasma-discover firefox-esr libreoffice kate "
                   "kscreen-doctor nmcli flatpak gcc g++ make zionos-welcome zionos-settings zionos-assistant; "
                   "do command -v $b >/dev/null || echo MISSING:$b; done")
    rep.check("desktop: expected applications installed", out.strip() == "", out)
    rc, out, _ = g("ls /usr/share/applications | grep -c -E 'zionos-(welcome|settings|assistant|about)'")
    rep.check("desktop: ZionOS launcher entries", out.strip() == "4", out)
    rc, out, _ = g("locale -a | grep -i -E '^(he_IL|en_US)\\.utf-?8$' | sort | tr '\\n' ' '; "
                   "fc-list :lang=he family | sort -u | head -40 | tr '\\n' ';'")
    rep.check("i18n: en_US and he_IL locales generated", "en_US" in out and "he_IL" in out, out[:80])
    rep.check("i18n: Hebrew fonts available", "Noto Sans Hebrew" in out or "Culmus" in out or "David" in out
              or "Frank" in out, out[-300:])
    rc, out, _ = g("grep -E 'LayoutList|Options' /etc/xdg/kxkbrc | tr '\\n' ' '")
    rep.check("i18n: us+il layouts with Alt+Shift by default",
              "LayoutList=us,il" in out and "grp:alt_shift_toggle" in out, out)

    # Representative applications actually start (as the desktop user).
    rc, out, err = g(user_env(user) + "libreoffice --version; firefox-esr --version; kate --version", user=user,
                     timeout=180)
    rep.check("apps: LibreOffice, Firefox ESR and Kate report versions",
              "LibreOffice" in out and "Firefox" in out and "kate" in out.lower(), (out + err).replace("\n", " ")[:200])
    rc, out, err = g(user_env(user) + "cd $(mktemp -d) && printf 'שלום\\n' > 'קובץ עברי.txt' && "
                     "libreoffice --headless --convert-to pdf 'קובץ עברי.txt' >/dev/null 2>&1; ls", user=user,
                     timeout=240)
    rep.check("apps: LibreOffice converts a Hebrew-named document", "קובץ עברי.pdf" in out, out + err)
    prog = ('#include <iostream>\\n#include <string>\\nint main(){std::string s=\\"ZionOS C++ שלום\\";'
            'std::cout<<s<<std::endl;return 0;}')
    rc, out, err = g("cd $(mktemp -d) && printf '%s\\n' > t.cpp && g++ -std=c++20 -Wall -O2 t.cpp -o t && ./t && "
                     "printf '#include <stdio.h>\\nint main(void){puts(\"ZionOS C ok\");return 0;}\\n' > c.c && "
                     "gcc -Wall c.c -o c && ./c" % prog, user=user, timeout=180)
    rep.check("dev: C and C++ programs compile and run", "ZionOS C++ שלום" in out and "ZionOS C ok" in out,
              (out + err)[:200])
    rc, out, _ = g("printf 'a b\\n' | awk '{print $2}'; echo x | sed s/x/y/; tar --version | head -1; "
                   "git --version; curl --version | head -1; ssh -V 2>&1 | head -1")
    rep.check("cli: core terminal utilities work", out.startswith("b\ny\ntar") and "git version" in out, out[:120])

    # ZionOS applications start and stay up (catches missing libraries,
    # translation or platform plugin problems).
    for app, title in (("zionos-settings --page about", "About"), ("zionos-assistant", "Assistant")):
        g(user_env(user) + "(setsid %s >/tmp/%s.log 2>&1 &) ; sleep 6" % (app, app.split()[0]), user=user)
        rc, out, _ = g("pgrep -u %s -f '^%s' >/dev/null && echo up; cat /tmp/%s.log | head -5"
                       % (user, app.split()[0], app.split()[0]))
        rep.check("apps: %s starts" % app.split()[0], out.startswith("up"), out)
    time.sleep(3)
    vm.screenshot(os.path.join(outdir, "zionos-apps.png"))
    g("pkill -u %s -f zionos-settings; pkill -u %s -f zionos-assistant; true" % (user, user))

    rc, out, _ = g("zionos-assistant --version; dpkg -s zionos-assistant | grep -E '^(Status|Version)'")
    rep.check("packaging: zionos-assistant installed from .deb", "install ok installed" in out, out.replace("\n", " "))
    # The system must not depend on the assistant or on any key.
    rc, out, _ = g("apt-get -s remove zionos-assistant 2>/dev/null | grep '^Remv' | tr '\\n' ';'")
    rep.check("optional AI: removing the assistant removes nothing else",
              out.count("Remv") == 1 and "zionos-assistant" in out, out)
    if live:
        rc, out, _ = g("command -v zionos-install calamares >/dev/null && grep -c '^branding: zionos' /etc/calamares/settings.conf")
        rep.check("installer: Calamares present with ZionOS branding", out.strip() == "1", out)
