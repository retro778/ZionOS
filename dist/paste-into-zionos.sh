#!/bin/sh
# Type text from the host into the running ZionOS virtual machine, as if it
# were typed on the VM's keyboard. Use it for things you cannot paste, such
# as an API key: click into the field inside ZionOS first, then run this.
#
#   ./paste-into-zionos.sh            type the host clipboard contents
#   ./paste-into-zionos.sh --ask      ask for the text (hidden input) instead
#   ./paste-into-zionos.sh --enter    press Enter afterwards
#
# The text is sent straight to QEMU over a local socket in this folder. It is
# not written to any file and not shown on screen.
# Requirements: the VM was started with ./run-zionos-qemu.sh or
# ./install-zionos-qemu.sh (current versions), and the keyboard layout inside
# ZionOS is English (the "us" indicator in the panel) while typing.
# Only ASCII text can be typed this way.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOCK="${ZIONOS_QMP_SOCKET:-$HERE/.zionos-qmp.sock}"
ASK=0; ENTER=0
for a in "$@"; do
    case "$a" in
        --ask) ASK=1 ;;
        --enter) ENTER=1 ;;
        -h|--help) sed -n '2,16p' "$0"; exit 0 ;;
        *) echo "unknown option: $a" >&2; exit 1 ;;
    esac
done
[ -S "$SOCK" ] || { echo "error: no running ZionOS VM found ($SOCK).
Start it with ./run-zionos-qemu.sh (restart the VM if it was started with an older script)." >&2; exit 1; }

if [ $ASK = 0 ]; then
    if command -v wl-paste >/dev/null 2>&1 && [ -n "${WAYLAND_DISPLAY:-}" ]; then TEXT=$(wl-paste --no-newline)
    elif command -v xclip >/dev/null 2>&1; then TEXT=$(xclip -o -selection clipboard)
    elif command -v xsel >/dev/null 2>&1; then TEXT=$(xsel --clipboard --output)
    else ASK=1; fi
fi
if [ $ASK = 1 ]; then
    printf 'Text to type into ZionOS (input is hidden): ' >&2
    stty -echo 2>/dev/null || true
    IFS= read -r TEXT
    stty echo 2>/dev/null || true
    echo >&2
fi
[ -n "$TEXT" ] || { echo "error: nothing to type (clipboard is empty)" >&2; exit 1; }

# The text goes to Python through the environment of this one process, not
# through its command line (which other users could see in the process list).
ZIONOS_TYPE_TEXT="$TEXT" ZIONOS_TYPE_ENTER="$ENTER" exec python3 - "$SOCK" <<'PY'
import json, os, socket, sys, time

text = os.environ.pop("ZIONOS_TYPE_TEXT")
enter = os.environ.pop("ZIONOS_TYPE_ENTER") == "1"
text = text.rstrip("\r\n")

PLAIN = {" ": "spc", "-": "minus", "=": "equal", "[": "bracket_left", "]": "bracket_right",
         "\\": "backslash", ";": "semicolon", "'": "apostrophe", "`": "grave_accent",
         ",": "comma", ".": "dot", "/": "slash", "\t": "tab", "\n": "ret"}
SHIFT = {"_": "minus", "+": "equal", "{": "bracket_left", "}": "bracket_right", "|": "backslash",
         ":": "semicolon", '"': "apostrophe", "~": "grave_accent", "<": "comma", ">": "dot",
         "?": "slash", "!": "1", "@": "2", "#": "3", "$": "4", "%": "5", "^": "6", "&": "7",
         "*": "8", "(": "9", ")": "0"}

def keys_for(ch):
    if ch in PLAIN:
        return [PLAIN[ch]]
    if ch in SHIFT:
        return ["shift", SHIFT[ch]]
    if "a" <= ch <= "z" or "0" <= ch <= "9":
        return [ch]
    if "A" <= ch <= "Z":
        return ["shift", ch.lower()]
    return None

bad = sorted({c for c in text if keys_for(c) is None})
if bad:
    sys.exit("error: the text contains %d character(s) that cannot be typed on a US keyboard "
             "(non-ASCII); nothing was sent." % len(bad))

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(sys.argv[1])
f = s.makefile("rwb", buffering=0)

def cmd(obj):
    f.write(json.dumps(obj).encode() + b"\n")
    while True:
        r = json.loads(f.readline())
        if "return" in r:
            return
        if "error" in r:
            sys.exit("error from QEMU: %s" % r["error"].get("desc", "unknown"))

f.readline()                                   # greeting
cmd({"execute": "qmp_capabilities"})
HOLD = 60                                      # ms each key is held down
for ch in text + ("\n" if enter else ""):
    k = keys_for(ch)
    cmd({"execute": "send-key", "arguments": {
        "keys": [{"type": "qcode", "data": x} for x in k], "hold-time": HOLD}})
    time.sleep(HOLD / 1000.0 + 0.09)           # let the release happen before the next key
# QEMU hands the keys to the guest at its own pace; give it time to drain
# before reporting, so nothing is pressed in the VM while text still arrives.
time.sleep(min(8.0, 0.5 + len(text) * 0.03))
print("Typed %d characters into the ZionOS VM." % len(text))
PY
