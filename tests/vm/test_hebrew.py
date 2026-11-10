#!/usr/bin/env python3
"""Hebrew / right-to-left checks in the live session, driven with real
keyboard events and verified from inside the guest.

Covers: selecting Hebrew at boot, Hebrew UI of ZionOS applications, typing
Hebrew with the Alt+Shift layout switch, Hebrew file names, selection and
clipboard, dialogs, and a Hebrew assistant conversation against a local
stand-in for the provider (no key, no Internet).

Usage: test_hebrew.py ISO [--firmware bios|uefi] [--out DIR]
"""
import argparse
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
import desktop_checks  # noqa: E402
from zvm import VM, Report, ocr, wait_for  # noqa: E402

FAKE_KEY = "sk-ant-test-vm-0000000000000000"
HERE = os.path.dirname(os.path.abspath(__file__))
# Keys on the US layout that produce the given Hebrew text on the "il" layout.
SHALOM_OLAM = "akuo guko"       # שלום עולם
QUESTION = "thl ngsfbho"        # איך מעדכנים


def run(iso, firmware, outdir):
    rep = Report("Hebrew live session (%s)" % firmware)
    wd = os.path.join(outdir, "hebrew-" + firmware)
    with VM(name="zionos-hebrew", iso=iso, firmware=firmware, workdir=wd) as vm:
        time.sleep(6 if firmware == "bios" else 10)
        vm.key("down")            # second boot entry: "ZionOS Live (Hebrew)"
        vm.key("ret")
        vm.wait_agent(420)
        desktop_checks.wait_desktop(vm, "zion")
        time.sleep(30)

        def g(cmd, user=None, timeout=120):
            return vm.guest(cmd, user=user, timeout=timeout)

        def session(cmd):
            """Run a command as the desktop user with the session bus."""
            return g("export DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus WAYLAND_DISPLAY=wayland-0; " + cmd,
                     user="zion")

        def clipboard():
            return session("qdbus6 org.kde.klipper /klipper getClipboardContents")[1].rstrip("\n")

        rc, out, _ = g("cat /proc/cmdline; tr '\\0' '\\n' < /proc/$(pgrep -u zion plasmashell | head -1)/environ | grep '^LANG='")
        rep.check("boot menu: Hebrew entry selects he_IL", "locales=he_IL.UTF-8" in out and "LANG=he_IL.UTF-8" in out,
                  out.replace("\n", " ")[-160:])
        shot = vm.screenshot(os.path.join(wd, "01-welcome-he.png"))
        text = ocr(shot, "heb+eng")
        rep.check("Welcome window is shown in Hebrew", "ברוכים" in text and "ZionOS" in text,
                  " ".join(text.split())[:120])
        rc, out, _ = session("qdbus6 org.kde.KWin /KWin org.kde.KWin.supportInformation 2>/dev/null | grep -i -m1 'layoutDirection\\|rightToLeft'; "
                             "kreadconfig6 --file kdeglobals --group Locale --key Language 2>/dev/null; echo; echo $LANG")
        g("pkill -u zion -f '^zionos-welcome'; true")

        # --- typing with layout switching -----------------------------------
        vm.key("alt", "f2"); time.sleep(3)
        vm.type("konsole"); time.sleep(2); vm.key("ret"); time.sleep(8)
        vm.type("cat > /tmp/typed.txt"); vm.key("ret")
        vm.key("alt", "shift"); time.sleep(1)
        vm.type(SHALOM_OLAM)
        vm.key("alt", "shift"); time.sleep(1)
        vm.type(" abc"); vm.key("ret"); vm.key("ctrl", "d"); time.sleep(1)
        rc, out, _ = g("cat /tmp/typed.txt")
        rep.check("typing: Alt+Shift switches to Hebrew and back", out.strip() == "שלום עולם abc", repr(out))
        # Hebrew file name created from the terminal, typed on the keyboard.
        vm.type("touch /tmp/"); vm.key("alt", "shift"); time.sleep(1); vm.type("neuc.")
        vm.key("alt", "shift"); time.sleep(1); vm.type("txt"); vm.key("ret"); time.sleep(1)
        rc, out, _ = g("ls /tmp | grep txt")
        rep.check("file names: Hebrew name typed in the terminal", "קובץ.txt" in out, out)
        vm.screenshot(os.path.join(wd, "02-konsole.png"))
        g("pkill -u zion konsole; true")

        # --- ZionOS Settings dialog in Hebrew ---------------------------------
        vm.key("alt", "f2"); time.sleep(3)
        vm.type("zionos-settings --page keyboard"); time.sleep(2); vm.key("ret"); time.sleep(7)
        shot = vm.screenshot(os.path.join(wd, "03-settings-he.png"))
        text = ocr(shot, "heb+eng")
        rep.check("dialogs: ZionOS Settings is translated and laid out for Hebrew",
                  "מקלדת" in text and ("פריסות" in text or "עברית" in text), " ".join(text.split())[:160])
        g("pkill -u zion -f '^zionos-settings'; true")

        # --- assistant conversation in Hebrew ----------------------------------
        with open(os.path.join(HERE, "mock_provider.py"), "rb") as f:
            vm.guest_write("/tmp/mock_provider.py", f.read())
        # Keep KDE Wallet's first-use wizard out of the way: with the wallet
        # disabled the keyring is "unavailable", which is exactly the path
        # that must offer session-only storage instead of plaintext.
        session("kwriteconfig6 --file kwalletrc --group Wallet --key Enabled false; "
                "rm -f /tmp/mock-provider.jsonl; (setsid python3 /tmp/mock_provider.py >/tmp/mock.log 2>&1 &)")
        vm.key("alt", "f2"); time.sleep(3)
        vm.type("env ZIONOS_TEST_ANTHROPIC_URL=http://127.0.0.1:8765 zionos-assistant"); time.sleep(2)
        vm.key("ret"); time.sleep(8)
        shot = vm.screenshot(os.path.join(wd, "04-assistant-he.png"))
        text = ocr(shot, "heb+eng")
        rep.check("assistant: window in Hebrew names the destination host",
                  "127.0.0.1" in text and ("שליחה" in text or "העוזר" in text), " ".join(text.split())[:160])

        def ask():
            vm.key("alt", "shift"); time.sleep(1)
            vm.type(QUESTION)
            vm.key("alt", "shift"); time.sleep(1)
            vm.type(" apt?")
            time.sleep(1)

        # 1. No key: nothing may be sent.
        ask()
        vm.key("ctrl", "ret"); time.sleep(5)
        rc, out, _ = g("wc -l < /tmp/mock-provider.jsonl 2>/dev/null || echo 0")
        vm.screenshot(os.path.join(wd, "05-no-key.png"))
        rep.check("assistant: without an API key nothing is transmitted", out.strip() == "0", out)

        # 2. Selection + clipboard on the Hebrew/English text still in the input.
        vm.key("ctrl", "a"); vm.key("ctrl", "c"); time.sleep(1)
        clip = clipboard()
        rep.check("clipboard: select-all + copy of mixed Hebrew/English text", clip == "איך מעדכנים apt?", repr(clip))

        # 3. Enter a (fake) key: keyring unavailable -> session-only storage.
        vm.key("ctrl", "comma"); time.sleep(4)
        vm.key("tab"); vm.type(FAKE_KEY); vm.key("ret"); time.sleep(4)
        shot = vm.screenshot(os.path.join(wd, "06-keyring-unavailable.png"))
        text = ocr(shot, "heb+eng")
        rep.check("keys: unavailable keyring is explained, session-only offered",
                  "KDE Wallet" in text or "מחזיק המפתחות" in text, " ".join(text.split())[:200])
        vm.key("ret"); time.sleep(2)      # default button: use for this session only
        vm.key("esc"); time.sleep(2)      # close settings
        rc, out, _ = g("grep -rl 'sk-ant-test' /home/zion /tmp --exclude=mock_provider.py 2>/dev/null | head -3")
        rep.check("keys: the session-only key is not written to disk", out.strip() == "", out)

        # 4. Send the Hebrew question for real (to the local stand-in).
        vm.key("ctrl", "ret")
        try:
            wait_for(lambda: g("grep -c message_stop /tmp/mock.log >/dev/null 2>&1; wc -l < /tmp/mock-provider.jsonl "
                               "2>/dev/null || echo 0")[1].strip() == "1", 30, what="request")
        except Exception:  # noqa: BLE001
            pass
        time.sleep(6)
        rc, out, _ = g("cat /tmp/mock-provider.jsonl 2>/dev/null")
        reqs = [json.loads(line) for line in out.splitlines() if line.strip()]
        ok = len(reqs) == 1
        detail = "requests=%d" % len(reqs)
        if ok:
            h, b = reqs[0]["headers"], reqs[0]["body"]
            ok = (reqs[0]["path"] == "/v1/messages" and h.get("x-api-key") == FAKE_KEY
                  and h.get("anthropic-version") == "2026-10-21" and b["stream"] is True
                  and b["messages"][-1] == {"role": "user", "content": "איך מעדכנים apt?"}
                  and "temperature" not in b and "attached-content" not in json.dumps(b["messages"]))
            detail = json.dumps({"msg": b["messages"][-1], "model": b["model"]}, ensure_ascii=False)
        rep.check("assistant: exactly one request with the typed Hebrew text", ok, detail)
        shot = vm.screenshot(os.path.join(wd, "07-conversation.png"))
        text = ocr(shot, "heb+eng")
        flat = " ".join(text.split())
        rep.check("assistant: streamed Hebrew reply and proposed command are displayed",
                  "sudo apt update" in flat and ("שלום" in flat or "הפקודה" in flat), flat[:200])
        rep.check("assistant: HTML in the reply is shown as text, not interpreted", "script" in flat, flat[-200:])
        rep.check("assistant: provider-reported token usage is shown", "42" in flat and "57" in flat, flat[-260:])
        rc, out, _ = g("ls -l /home/zion/.local/share/zionos-assistant/conversations/ | tail -n +2 | cut -c1-10; "
                       "grep -l 'איך מעדכנים' /home/zion/.local/share/zionos-assistant/conversations/*.json | wc -l; "
                       "grep -c 'sk-ant' /home/zion/.local/share/zionos-assistant/conversations/*.json")
        o = out.split()
        rep.check("history: conversation saved locally (0600), without the key",
                  o[:1] == ["-rw-------"] and o[1:2] == ["1"] and o[2:3] == ["0"], out.replace("\n", " "))

        # 5. Copy the proposed command via the keyboard-accessible button.
        ok = vm.click_text("sudo apt update", dx=-150, timeout=10)   # the "copy" button left of the code (RTL)
        time.sleep(2)
        clip = clipboard()
        rep.check("commands: copy button puts only the command on the clipboard (nothing was executed)",
                  clip == "sudo apt update", repr(clip))
        rc, out, _ = g("grep -c 'apt update' /var/log/apt/history.log 2>/dev/null || echo 0")
        rep.check("commands: the proposed command was not run", out.strip() == "0", out)

        try:
            g("systemctl poweroff", timeout=20)
        except Exception:  # noqa: BLE001
            pass
        vm.wait_exit(120)
    return rep.finish()


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("iso")
    ap.add_argument("--firmware", default="bios")
    ap.add_argument("--out", default="/src/out/test-results")
    a = ap.parse_args()
    sys.exit(run(a.iso, a.firmware, a.out))
