#!/usr/bin/env bash
# Resume test for asus_ux5406_battery.
#
#   ./test-resume.sh before   # sets the limit to 60% (or TARGET=80) and records the time
#   systemctl suspend         # suspend and wake the laptop yourself
#   ./test-resume.sh after    # shows the readback and the module's log lines
#
# Only the write needs sudo; the script asks for the password itself.
set -euo pipefail

TARGET="${TARGET:-60}"
BAT=/sys/class/power_supply/BAT0
LIMIT="$BAT/charge_control_end_threshold"
RAW="$BAT/ec_rsoc_raw"
STATE="${XDG_RUNTIME_DIR:-/tmp}/ux5406-resume-test.since"

case "${1:-}" in
    before)
        echo "$TARGET" | sudo tee "$LIMIT" >/dev/null
        sleep 1
        printf '%s\n%s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$TARGET" > "$STATE"
        echo "Limit set to $(cat "$LIMIT")%, EC raw: $(cat "$RAW")"
        echo "Now suspend (systemctl suspend), wake the laptop, then run: $0 after"
        ;;
    after)
        if [[ ! -f "$STATE" ]]; then
            echo "No 'before' run recorded. Run: $0 before" >&2
            exit 1
        fi
        since="$(sed -n 1p "$STATE")"
        TARGET="$(sed -n 2p "$STATE")"
        raw="$(cat "$RAW")"
        echo "Limit now: $(cat "$LIMIT")%"
        echo "EC raw:    $raw"
        echo
        echo "Kernel log since $since (module and suspend lines):"
        journalctl -k --no-pager --since "$since" \
            | grep -E 'asus_ux5406|PM: suspend|PM: hibernation' || echo "(no matching lines)"
        echo
        if [[ "${raw%% *}" == "$(printf '%02x' "$TARGET")" ]]; then
            echo "RESULT: PASS (EC still holds $TARGET%)"
        else
            echo "RESULT: FAIL (expected $(printf '%02x' "$TARGET") in byte 0, got ${raw%% *})"
            if systemctl is-active --quiet asusd 2>/dev/null; then
                echo "Note: asusd is running and re-applies its own limit on wake."
                echo "      Check it with 'asusctl battery info'; TARGET must match it."
            fi
        fi
        ;;
    *)
        echo "Usage: $0 before | after" >&2
        exit 1
        ;;
esac
