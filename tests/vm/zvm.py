"""Minimal QEMU harness for ZionOS integration tests.

Drives a VM through QMP (screenshots, keyboard, pointer, power) and through
the QEMU guest agent (running commands inside the guest). Standard library
only; runs inside the builder container.
"""
import base64
import json
import os
import shutil
import socket
import subprocess
import tempfile
import time

OVMF_CODE = "/usr/share/OVMF/OVMF_CODE_4M.fd"
OVMF_VARS = "/usr/share/OVMF/OVMF_VARS_4M.fd"


class VMError(RuntimeError):
    pass


class _JsonSock:
    def __init__(self, path, timeout=30):
        deadline = time.time() + timeout
        while True:
            try:
                self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                self.s.connect(path)
                break
            except OSError:
                if time.time() > deadline:
                    raise
                time.sleep(0.2)
        self.buf = b""

    def close(self):
        try:
            self.s.close()
        except OSError:
            pass

    def send(self, obj):
        self.s.sendall(json.dumps(obj).encode() + b"\n")

    def recv(self, timeout=30):
        self.s.settimeout(timeout)
        while b"\n" not in self.buf:
            chunk = self.s.recv(65536)
            if not chunk:
                raise VMError("socket closed")
            self.buf += chunk
        line, self.buf = self.buf.split(b"\n", 1)
        line = line.lstrip(b"\xff")
        return json.loads(line)


class VM:
    def __init__(self, name="zionos-test", iso=None, disk=None, firmware="bios",
                 mem=4096, cpus=4, workdir=None, boot=None, extra=None,
                 vars_file=None, audio=False):
        self.workdir = workdir or tempfile.mkdtemp(prefix="zvm-")
        os.makedirs(self.workdir, exist_ok=True)
        self.qmp_path = os.path.join(self.workdir, "qmp.sock")
        self.qga_path = os.path.join(self.workdir, "qga.sock")
        self.serial_log = os.path.join(self.workdir, "serial.log")
        for p in (self.qmp_path, self.qga_path):
            if os.path.exists(p):
                os.unlink(p)
        kvm = os.access("/dev/kvm", os.R_OK | os.W_OK)
        self.kvm = kvm
        a = ["qemu-system-x86_64", "-name", name,
             "-machine", "q35,accel=%s" % ("kvm" if kvm else "tcg"),
             "-cpu", "host" if kvm else "max",
             "-m", str(mem), "-smp", str(cpus),
             "-display", "none", "-vga", "none", "-device", "virtio-vga",
             "-device", "qemu-xhci", "-device", "usb-tablet", "-device", "usb-kbd",
             "-netdev", "user,id=n0", "-device", "virtio-net-pci,netdev=n0",
             "-device", "virtio-rng-pci",
             "-qmp", "unix:%s,server=on,wait=off" % self.qmp_path,
             "-chardev", "socket,path=%s,server=on,wait=off,id=qga0" % self.qga_path,
             "-device", "virtio-serial",
             "-device", "virtserialport,chardev=qga0,name=org.qemu.guest_agent.0",
             "-serial", "file:%s" % self.serial_log,
             "-nodefaults"]
        if audio:
            a += ["-audiodev", "none,id=a0", "-device", "ich9-intel-hda",
                  "-device", "hda-duplex,audiodev=a0"]
        if firmware == "uefi":
            self.vars = vars_file or os.path.join(self.workdir, "OVMF_VARS.fd")
            if not os.path.exists(self.vars):
                shutil.copy(OVMF_VARS, self.vars)
            a += ["-drive", "if=pflash,format=raw,readonly=on,file=%s" % OVMF_CODE,
                  "-drive", "if=pflash,format=raw,file=%s" % self.vars]
        if disk:
            a += ["-drive", "file=%s,if=none,id=hd0,format=qcow2,discard=unmap" % disk,
                  "-device", "virtio-blk-pci,drive=hd0,bootindex=%d" % (2 if (iso and boot != "disk") else 1)]
        if iso:
            a += ["-drive", "file=%s,if=none,id=cd0,media=cdrom,readonly=on,format=raw" % iso,
                  "-device", "ide-cd,drive=cd0,bootindex=%d" % (1 if boot != "disk" else 2)]
        if extra:
            a += extra
        self.args = a
        self.proc = None
        self.qmp = None
        self.qga = None

    # ---- lifecycle -------------------------------------------------------
    def start(self):
        self.log = open(os.path.join(self.workdir, "qemu.log"), "wb")
        self.proc = subprocess.Popen(self.args, stdout=self.log, stderr=self.log)
        self.qmp = _JsonSock(self.qmp_path)
        self.qmp.recv()  # greeting
        self.qmp_cmd("qmp_capabilities")
        return self

    def alive(self):
        return self.proc is not None and self.proc.poll() is None

    def stop(self):
        if self.alive():
            try:
                self.qmp_cmd("quit")
            except Exception:
                self.proc.kill()
            try:
                self.proc.wait(10)
            except subprocess.TimeoutExpired:
                self.proc.kill()

    def wait_exit(self, timeout=120):
        try:
            self.proc.wait(timeout)
            return True
        except subprocess.TimeoutExpired:
            return False

    def __enter__(self):
        return self.start()

    def __exit__(self, *exc):
        self.stop()

    # ---- QMP -------------------------------------------------------------
    def qmp_cmd(self, cmd, **args):
        msg = {"execute": cmd}
        if args:
            msg["arguments"] = args
        self.qmp.send(msg)
        while True:
            r = self.qmp.recv(60)
            if "return" in r:
                return r["return"]
            if "error" in r:
                raise VMError("%s: %s" % (cmd, r["error"]))
            # otherwise: asynchronous event, ignore

    def screenshot(self, path):
        self.qmp_cmd("screendump", filename=path, format="png")
        return path

    def key(self, *keys, hold=80):
        """Press a key combination, e.g. key('ctrl', 'alt', 'f2')."""
        self.qmp_cmd("send-key", keys=[{"type": "qcode", "data": k} for k in keys],
                     **{"hold-time": hold})
        time.sleep(0.12 + hold / 1000.0)

    _KEYMAP = {" ": "spc", "\n": "ret", "-": "minus", ".": "dot", "/": "slash",
               ",": "comma", "=": "equal", ";": "semicolon", "'": "apostrophe",
               "\\": "backslash", "[": "bracket_left", "]": "bracket_right",
               "`": "grave_accent", "\t": "tab"}
    _SHIFT = {"_": "minus", ":": "semicolon", "?": "slash", "!": "1", "@": "2",
              "#": "3", "$": "4", "%": "5", "^": "6", "&": "7", "*": "8",
              "(": "9", ")": "0", "+": "equal", "\"": "apostrophe", "|": "backslash",
              "<": "comma", ">": "dot", "{": "bracket_left", "}": "bracket_right",
              "~": "grave_accent"}

    def type(self, text):
        """Type ASCII text on the (US layout) guest keyboard."""
        for ch in text:
            if ch in self._KEYMAP:
                self.key(self._KEYMAP[ch])
            elif ch in self._SHIFT:
                self.key("shift", self._SHIFT[ch])
            elif ch.isupper():
                self.key("shift", ch.lower())
            else:
                self.key(ch)

    def _screen_size(self):
        p = os.path.join(self.workdir, "_size.png")
        self.screenshot(p)
        from PIL import Image
        with Image.open(p) as im:
            return im.size

    def mouse_move(self, x, y):
        w, h = self._screen_size()
        ax, ay = int(x * 32767 / max(w - 1, 1)), int(y * 32767 / max(h - 1, 1))
        self.qmp_cmd("input-send-event", events=[
            {"type": "abs", "data": {"axis": "x", "value": ax}},
            {"type": "abs", "data": {"axis": "y", "value": ay}}])
        time.sleep(0.1)

    def click(self, x, y, button="left", double=False):
        self.mouse_move(x, y)
        for _ in range(2 if double else 1):
            for down in (True, False):
                self.qmp_cmd("input-send-event", events=[
                    {"type": "btn", "data": {"down": down, "button": button}}])
                time.sleep(0.06)
        time.sleep(0.2)

    def screen_text(self, lang="eng"):
        p = os.path.join(self.workdir, "_ocr.png")
        self.screenshot(p)
        return ocr(p, lang)

    def wait_text(self, phrase, timeout=120, interval=4, lang="eng"):
        """Wait until the phrase is visible on screen (OCR, whitespace-insensitive)."""
        want = "".join(phrase.lower().split())
        deadline = time.time() + timeout
        while time.time() < deadline:
            if want in "".join(self.screen_text(lang).lower().split()):
                return True
            time.sleep(interval)
        return False

    def click_text(self, phrase, dx=0, dy=0, timeout=60, lang="eng", double=False):
        """Click on a visible label. Returns False if it never appears."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            p = os.path.join(self.workdir, "_find.png")
            self.screenshot(p)
            pos = find_text(p, phrase, lang)
            if pos:
                self.click(pos[0] + dx, pos[1] + dy, double=double)
                return True
            time.sleep(3)
        return False

    def powerdown(self):
        self.qmp_cmd("system_powerdown")

    # ---- guest agent -----------------------------------------------------
    def wait_agent(self, timeout=300):
        """Wait until the guest agent inside the VM answers."""
        # QEMU serves one client at a time on the agent socket: drop any
        # previous connection (e.g. from before a guest reboot) first.
        if self.qga is not None:
            self.qga.close()
            self.qga = None
        deadline = time.time() + timeout
        while time.time() < deadline:
            if not self.alive():
                raise VMError("qemu exited while waiting for guest agent")
            q = None
            try:
                q = _JsonSock(self.qga_path, timeout=5)
                token = int(time.time() * 1000) % 1000000
                q.send({"execute": "guest-sync", "arguments": {"id": token}})
                while True:
                    r = q.recv(5)
                    if r.get("return") == token:
                        self.qga = q
                        return True
            except (OSError, VMError, ValueError):
                if q is not None:
                    q.close()
                time.sleep(3)
        raise VMError("guest agent did not respond within %ss" % timeout)

    def _qga(self, cmd, timeout=60, **args):
        msg = {"execute": cmd}
        if args:
            msg["arguments"] = args
        self.qga.send(msg)
        r = self.qga.recv(timeout)
        if "error" in r:
            raise VMError("%s: %s" % (cmd, r["error"]))
        return r["return"]

    def guest(self, script, timeout=120, check=False, user=None):
        """Run a shell snippet in the guest. Returns (exit_code, stdout, stderr)."""
        if user:
            arg = ["-u", user, "--", "env", "XDG_RUNTIME_DIR=/run/user/1000",
                   "sh", "-c", script]
            path = "/usr/sbin/runuser"
        else:
            arg, path = ["-c", script], "/bin/sh"
        pid = self._qga("guest-exec", path=path, arg=arg,
                        **{"capture-output": True})["pid"]
        deadline = time.time() + timeout
        while True:
            st = self._qga("guest-exec-status", pid=pid)
            if st["exited"]:
                break
            if time.time() > deadline:
                raise VMError("guest command timed out: %s" % script)
            time.sleep(0.5)
        out = base64.b64decode(st.get("out-data", "")).decode("utf-8", "replace")
        err = base64.b64decode(st.get("err-data", "")).decode("utf-8", "replace")
        rc = st.get("exitcode", -1)
        if check and rc != 0:
            raise VMError("guest command failed (%s): %s\n%s\n%s" % (rc, script, out, err))
        return rc, out, err

    def guest_write(self, path, data):
        if isinstance(data, str):
            data = data.encode()
        h = self._qga("guest-file-open", path=path, mode="w")
        self._qga("guest-file-write", handle=h,
                  **{"buf-b64": base64.b64encode(data).decode()})
        self._qga("guest-file-close", handle=h)


def ocr(image, lang="eng"):
    """OCR a screenshot with tesseract (upscaled for small UI fonts)."""
    from PIL import Image
    with Image.open(image) as im:
        im = im.convert("L").resize((im.width * 2, im.height * 2))
        tmp = image + ".ocr.png"
        im.save(tmp)
    r = subprocess.run(["tesseract", tmp, "-", "-l", lang, "--psm", "11"],
                       capture_output=True, text=True)
    os.unlink(tmp)
    return r.stdout


def find_text(image, phrase, lang="eng"):
    """Locate a phrase on a screenshot. Returns the (x, y) centre of the first
    word of the best match in screen pixels, or None."""
    from PIL import Image
    scale = 2
    with Image.open(image) as im:
        im = im.convert("L").resize((im.width * scale, im.height * scale))
        tmp = image + ".find.png"
        im.save(tmp)
    r = subprocess.run(["tesseract", tmp, "-", "-l", lang, "--psm", "11", "tsv"],
                       capture_output=True, text=True)
    os.unlink(tmp)
    words = []
    for line in r.stdout.splitlines()[1:]:
        c = line.split("\t")
        if len(c) >= 12 and c[11].strip():
            words.append((c[11].strip(), int(c[6]), int(c[7]), int(c[8]), int(c[9])))
    want = phrase.split()
    norm = lambda w: "".join(ch for ch in w.lower() if ch.isalnum())  # noqa: E731
    for i in range(len(words) - len(want) + 1):
        if all(norm(words[i + j][0]) == norm(want[j]) for j in range(len(want))):
            first, last = words[i], words[i + len(want) - 1]
            x0, y0 = first[1], first[2]
            x1, y1 = last[1] + last[3], last[2] + last[4]
            return ((x0 + x1) // 2 // scale, (y0 + y1) // 2 // scale)
    return None


def wait_for(fn, timeout, interval=3, what="condition"):
    deadline = time.time() + timeout
    last = None
    while time.time() < deadline:
        last = fn()
        if last:
            return last
        time.sleep(interval)
    raise VMError("timed out waiting for %s" % what)


class Report:
    """Collects check results and prints a summary; exit code reflects failures."""
    def __init__(self, title):
        self.title, self.rows = title, []

    def check(self, name, ok, detail=""):
        self.rows.append((name, bool(ok), detail.strip()))
        print("[%s] %s%s" % ("PASS" if ok else "FAIL", name,
                             (" -- " + detail.strip().splitlines()[0][:200]) if detail.strip() else ""),
              flush=True)
        return bool(ok)

    def finish(self):
        failed = [r for r in self.rows if not r[1]]
        print("\n== %s: %d passed, %d failed ==" % (self.title, len(self.rows) - len(failed), len(failed)))
        return 1 if failed else 0
