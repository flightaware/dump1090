#!/bin/sh
# Make an existing dump1090-fa package install (PiAware, or the FlightAware
# dump1090-fa .deb) run the DEMOD_2400X build from this directory.
#
#   make                                  build ./dump1090 first
#   sudo sh install-2400x-dev.sh          install, or install a newer build
#   sudo sh uninstall-2400x-dev.sh        back to the packaged /usr/bin/dump1090-fa
#
# The packaged files are left untouched. The build is copied to
# /usr/local/bin/dump1090-2400x, and a systemd override makes the dump1090-fa
# service start it through a copy of the packaged start script, so
# /etc/default/dump1090-fa, the PiAware location, the service user and the JSON
# directory all stay as they were.
set -e

BIN=/usr/local/bin/dump1090-2400x
START=/usr/local/bin/start-dump1090-2400x
PKG_START=/usr/share/dump1090-fa/start-dump1090-fa
OVERRIDE_DIR=/etc/systemd/system/dump1090-fa.service.d
OVERRIDE=$OVERRIDE_DIR/2400x.conf

[ "$(id -u)" = 0 ] || { echo "run with sudo" >&2; exit 1; }

NEW="$(cd "$(dirname "$0")" && pwd)/dump1090"
[ -x "$NEW" ] || { echo "no build at $NEW, run make first" >&2; exit 1; }
"$NEW" --help 2>&1 | grep -q -- --no-demod2400x \
    || { echo "$NEW is not a DEMOD_2400X build" >&2; exit 1; }
[ -f "$PKG_START" ] && systemctl cat dump1090-fa >/dev/null 2>&1 \
    || { echo "the dump1090-fa package is not installed (needs $PKG_START and the service)" >&2; exit 1; }

# Copy of the packaged start script, running the new build
sed "s#exec /usr/bin/dump1090-fa#exec $BIN#" "$PKG_START" > "$START.new"
grep -q "exec $BIN" "$START.new" \
    || { rm -f "$START.new"; echo "could not patch $PKG_START, nothing changed" >&2; exit 1; }

systemctl stop dump1090-fa
install -m 755 "$NEW" "$BIN"
chmod 755 "$START.new"
mv "$START.new" "$START"

# Service override: same unit, user and JSON directory, new start script
mkdir -p "$OVERRIDE_DIR"
printf "[Service]\nExecStart=\nExecStart=%s --write-json %%t/dump1090-fa\n" "$START" > "$OVERRIDE"
systemctl daemon-reload
systemctl start dump1090-fa

sleep 5
pgrep -a dump1090 | cut -c1-60 || true
grep -o "\"version\" : \"[^\"]*\"" /run/dump1090-fa/receiver.json || true
echo "(the DEMOD_2400X build reports a version ending in x, e.g. \"11.2x\")"
