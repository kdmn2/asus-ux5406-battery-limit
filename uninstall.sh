#!/usr/bin/env bash
set -euo pipefail

PKG_NAME="asus-ux5406-battery"
MOD_NAME="asus_ux5406_battery"

if [[ -e /sys/class/power_supply/BAT0/charge_control_end_threshold ]]; then
    echo "Restoring 100% charge limit before removal..."
    echo 100 | sudo tee /sys/class/power_supply/BAT0/charge_control_end_threshold >/dev/null || true
fi
sudo modprobe -r "$MOD_NAME" 2>/dev/null || true
for ver in $(dkms status -m "$PKG_NAME" 2>/dev/null | sed -n "s|^${PKG_NAME}/\([^,:]*\)[,:].*|\1|p" | sort -u); do
    sudo dkms remove -m "$PKG_NAME" -v "$ver" --all || true
done
sudo rm -rf /usr/src/"${PKG_NAME}"-*
sudo rm -f "/etc/modules-load.d/${MOD_NAME}.conf" "/etc/modprobe.d/${MOD_NAME}.conf"

echo "Removed."
