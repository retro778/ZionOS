#!/bin/sh
# Refresh packages/zionos-apps/translations/zionos_he.ts from the sources and
# fill known Hebrew strings (translations/fill_he.py). New strings must be
# added to that table (or translated with Qt Linguist) before tests pass.
set -eu
. "$(dirname "$0")/common.sh"
if ! inside_container; then in_container -- scripts/update-translations.sh; exit $?; fi
cd /src/packages/zionos-apps
timeout 120 /usr/lib/qt6/bin/lupdate -no-obsolete -locations none -I . \
    core ui welcome settings assistant -ts translations/zionos_he.ts
python3 translations/fill_he.py translations/zionos_he.ts
chown "$HOST_UID:$HOST_GID" translations/*
