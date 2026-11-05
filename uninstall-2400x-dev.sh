#!/bin/sh
# Undo install-2400x-dev.sh: the dump1090-fa service goes back to the packaged
# /usr/bin/dump1090-fa (as installed by apt-get), with its settings unchanged.
#
#   sudo sh uninstall-2400x-dev.sh
set -e

BIN=/usr/local/bin/dump1090-2400x
START=/usr/local/bin/start-dump1090-2400x
OVERRIDE_DIR=/etc/systemd/system/dump1090-fa.service.d
OVERRIDE=$OVERRIDE_DIR/2400x.conf

[ "$(id -u)" = 0 ] || { echo "run with sudo" >&2; exit 1; }
[ -x /usr/bin/dump1090-fa ] \
    || { echo "packaged /usr/bin/dump1090-fa missing, reinstall it with: apt-get install --reinstall dump1090-fa" >&2; exit 1; }

rm -f "$OVERRIDE" "$START" "$BIN"
rmdir "$OVERRIDE_DIR" 2>/dev/null || true    # only if nothing else is in it
systemctl daemon-reload
systemctl restart dump1090-fa

sleep 5
pgrep -a dump1090 | cut -c1-60 || true
grep -o "\"version\" : \"[^\"]*\"" /run/dump1090-fa/receiver.json || true
echo "(the packaged dump1090-fa reports a version without the trailing x)"
