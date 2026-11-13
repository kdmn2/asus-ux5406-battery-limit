#!/usr/bin/env bash
# Compare the EC state left by Windows/MyASUS with the module's plain write.
#
#   ./test-myasus.sh prepare   # in Linux: stop the module auto-loading for the next boot
#   (reboot to Windows, set the limit in MyASUS, restart back into Linux)
#   ./test-myasus.sh read      # in Linux: read the EC before the module writes, then restore
#
# Only the commands that change system files or load the module use sudo.
set -euo pipefail

MOD=asus_ux5406_battery
CONF=/etc/modules-load.d/${MOD}.conf
OFF=${CONF}.disabled
RAW=/sys/class/power_supply/BAT0/ec_rsoc_raw
LIMIT=/sys/class/power_supply/BAT0/charge_control_end_threshold

case "${1:-}" in
    prepare)
        if [[ -f "$CONF" ]]; then
            sudo mv "$CONF" "$OFF"
        fi
        echo "Auto-load disabled for the next boot."
        echo "Now: reboot to Windows, set MyASUS, then RESTART (not shut down) into Linux"
        echo "and run: $0 read"
        ;;
    read)
        if [[ -d /sys/module/$MOD ]]; then
            echo "The module is already loaded, so it has already written its own value." >&2
            echo "The MyASUS state is lost for this boot. Run '$0 prepare' and try again." >&2
            exit 1
        fi
        # default_threshold=0 is rejected by the module, so it attaches without writing.
        sudo modprobe "$MOD" default_threshold=0
        sleep 1
        myasus="$(cat "$RAW")"
        echo "EC state left by MyASUS:  $myasus"
        journalctl -k -b --no-pager | grep "$MOD" | tail -3

        echo 80 | sudo tee "$LIMIT" >/dev/null
        sleep 1
        plain="$(cat "$RAW")"
        echo "EC after plain 80 write:  $plain"

        if [[ -f "$OFF" ]]; then
            sudo mv "$OFF" "$CONF"
        fi
        echo "Auto-load restored."
        echo
        echo "Send both EC lines above."
        ;;
    *)
        echo "Usage: $0 prepare | read" >&2
        exit 1
        ;;
esac
