#!/usr/bin/env python3
"""Interactive control of a long-running test VM (development aid).

  vmctl.py serve [--iso ISO] [--disk DISK] [--firmware bios|uefi] [--boot disk]
  vmctl.py shot FILE.png | key K1 [K2..] | type TEXT | click X Y [--double]
  vmctl.py exec [--user U] 'shell command' | wait-agent | stop | ocr FILE.png [lang]
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from zvm import VM, _JsonSock, ocr  # noqa: E402

WD = os.environ.get("ZVM_DIR", "/src/work/vm")


def attach():
    vm = VM(workdir=WD)
    vm.proc = None
    vm.qmp = _JsonSock(os.path.join(WD, "qmp.sock"))
    vm.qmp.recv()
    vm.qmp_cmd("qmp_capabilities")
    vm.alive = lambda: True
    return vm


def main(a):
    cmd = a[0]
    if cmd == "serve":
        import argparse
        ap = argparse.ArgumentParser()
        ap.add_argument("--iso"); ap.add_argument("--disk"); ap.add_argument("--firmware", default="bios")
        ap.add_argument("--boot"); ap.add_argument("--mem", type=int, default=4096)
        o = ap.parse_args(a[1:])
        os.makedirs(WD, exist_ok=True)
        vm = VM(iso=o.iso, disk=o.disk, firmware=o.firmware, boot=o.boot, workdir=WD, mem=o.mem, audio=True)
        import subprocess
        p = subprocess.Popen(vm.args)
        sys.exit(p.wait())
    if cmd == "ocr":
        print(ocr(a[1], a[2] if len(a) > 2 else "eng"))
        return
    # attach() rebuilds socket paths; VM.__init__ unlinks existing sockets, so
    # bypass it for attach mode.
    vm = VM.__new__(VM)
    vm.workdir = WD
    vm.qga = None
    vm.proc = None
    vm.qga_path = os.path.join(WD, "qga.sock")
    vm.qmp = _JsonSock(os.path.join(WD, "qmp.sock"))
    vm.qmp.recv()
    vm.qmp_cmd("qmp_capabilities")
    vm.alive = lambda: True
    if cmd == "shot":
        vm.screenshot(a[1]); os.chmod(a[1], 0o644)
    elif cmd == "key":
        vm.key(*a[1:])
    elif cmd == "type":
        vm.type(a[1])
    elif cmd == "click":
        vm.click(int(a[1]), int(a[2]), double="--double" in a)
    elif cmd == "wait-agent":
        vm.wait_agent(int(a[1]) if len(a) > 1 else 420); print("agent ready")
    elif cmd == "exec":
        user = None
        if a[1] == "--user":
            user = a[2]; a = a[2:]
        vm.wait_agent(60)
        rc, out, err = vm.guest(a[1], timeout=int(os.environ.get("ZVM_TIMEOUT", "300")), user=user)
        sys.stdout.write(out); sys.stderr.write(err); sys.exit(rc)
    elif cmd == "stop":
        vm.qmp_cmd("quit")
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main(sys.argv[1:])
