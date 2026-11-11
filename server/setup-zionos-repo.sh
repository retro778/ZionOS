#!/bin/sh
# One-time server setup for hosting the ZionOS APT repository and ISO files.
# Run ON THE SERVER as a user with sudo:   sudo sh setup-zionos-repo.sh
#
# What it does (and nothing else):
#   1. creates /srv/zionos/{repo,iso}, owned by the upload user, so later
#      uploads need no root access;
#   2. writes /etc/nginx/snippets/zionos-repo.conf (two static locations);
#   3. adds one "include" line to the HTTPS server block of the existing
#      nginx site, after saving a backup of that file;
#   4. runs "nginx -t"; on any error the backup is restored and nginx is NOT
#      reloaded, so running services are not interrupted.
# Existing locations (the chess server), i2pd and firewall rules are untouched.
# To undo: remove the include line and the snippet, then "nginx -s reload".
set -eu
UPLOAD_USER="${UPLOAD_USER:-${SUDO_USER:?run with sudo, or set UPLOAD_USER}}"
SITE="${SITE:-/etc/nginx/sites-available/beginnerchess}"
[ -f "$SITE" ] || SITE=$(readlink -f /etc/nginx/sites-enabled/beginnerchess)
SNIPPET=/etc/nginx/snippets/zionos-repo.conf
[ "$(id -u)" = 0 ] || { echo "run with sudo" >&2; exit 1; }
[ -f "$SITE" ] || { echo "nginx site file not found: $SITE" >&2; exit 1; }

install -d -o "$UPLOAD_USER" -g "$UPLOAD_USER" -m 755 /srv/zionos /srv/zionos/repo /srv/zionos/iso

cat > "$SNIPPET" <<'CONF'
# ZionOS package repository and ISO downloads (static files only).
location /zionos/ {
    alias /srv/zionos/repo/;
    autoindex on;
}
location /iso/ {
    alias /srv/zionos/iso/;
    autoindex on;
    limit_rate 4m;          # per connection, so downloads cannot crowd out other services
}
CONF

if grep -q "snippets/zionos-repo.conf" "$SITE"; then
    echo "include line already present in $SITE"
else
    BACKUP="$SITE.before-zionos.$(date +%Y%m%d%H%M%S)"
    cp -p "$SITE" "$BACKUP"
    # Insert the include just before the catch-all "location / {" of the
    # server block that listens on 443.
    awk '
        /listen[[:space:]]+443/ { in443 = 1 }
        in443 && !done && /^[[:space:]]*location[[:space:]]+\/[[:space:]]*\{/ {
            print "    include snippets/zionos-repo.conf;"
            print ""
            done = 1
        }
        { print }
        END { if (!done) exit 3 }
    ' "$BACKUP" > "$SITE.new" || { rm -f "$SITE.new"; echo "could not find where to insert the include; nothing changed" >&2; exit 1; }
    cat "$SITE.new" > "$SITE" && rm -f "$SITE.new"
    if ! nginx -t; then
        echo "nginx -t failed: restoring $BACKUP, nginx not reloaded" >&2
        cat "$BACKUP" > "$SITE"
        exit 1
    fi
    echo "backup of the previous site file: $BACKUP"
fi
nginx -t
systemctl reload nginx
echo "OK: https://<this host>/zionos/ and /iso/ now serve /srv/zionos/{repo,iso}"
