#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PKG_NAME="asus-ux5406-battery"
PKG_VER="$(sed -n 's/^PACKAGE_VERSION="\(.*\)"/\1/p' "$SCRIPT_DIR/dkms.conf")"
MOD_NAME="asus_ux5406_battery"
SRC_DIR="/usr/src/${PKG_NAME}-${PKG_VER}"
THRESHOLD="${THRESHOLD:-80}"

case "$THRESHOLD" in
    60|80|100) ;;
    *) echo "THRESHOLD must be 60, 80 or 100 (got: $THRESHOLD)" >&2; exit 1 ;;
esac

if ! command -v dkms >/dev/null 2>&1; then
    echo "dkms is not installed. Install it first (e.g. 'sudo pacman -S dkms')." >&2
    exit 1
fi

echo "[1/5] Removing any previously registered versions of ${PKG_NAME}..."
for ver in $(dkms status -m "$PKG_NAME" 2>/dev/null | sed -n "s|^${PKG_NAME}/\([^,:]*\)[,:].*|\1|p" | sort -u); do
    echo "      removing ${PKG_NAME}/${ver}"
    sudo dkms remove -m "$PKG_NAME" -v "$ver" --all || true
done
sudo rm -rf /usr/src/"${PKG_NAME}"-*

echo "[2/5] Registering ${PKG_NAME}/${PKG_VER} with DKMS..."
sudo mkdir -p "$SRC_DIR"
sudo cp "$SCRIPT_DIR/${MOD_NAME}.c" "$SCRIPT_DIR/Makefile" "$SCRIPT_DIR/dkms.conf" "$SRC_DIR/"
sudo dkms add -m "$PKG_NAME" -v "$PKG_VER"

echo "[3/5] Building and installing for every installed kernel with headers..."
failed=()
for build in /usr/lib/modules/*/build; do
    kver="$(basename "$(dirname "$build")")"
    echo "      kernel ${kver}"
    if ! sudo dkms install -m "$PKG_NAME" -v "$PKG_VER" -k "$kver" --force; then
        failed+=("$kver")
    fi
done
if ((${#failed[@]})); then
    echo "WARNING: build failed for: ${failed[*]}" >&2
fi

echo "[4/5] Configuring auto-load on boot (threshold=${THRESHOLD})..."
echo "$MOD_NAME" | sudo tee "/etc/modules-load.d/${MOD_NAME}.conf" >/dev/null
echo "options $MOD_NAME default_threshold=${THRESHOLD}" | sudo tee "/etc/modprobe.d/${MOD_NAME}.conf" >/dev/null

echo "[5/5] Reloading module..."
sudo modprobe -r "$MOD_NAME" 2>/dev/null || true
sudo modprobe "$MOD_NAME"

loaded="$(cat "/sys/module/${MOD_NAME}/srcversion")"
installed="$(modinfo -F srcversion "$MOD_NAME")"
echo
echo "Loaded module version:    $(cat "/sys/module/${MOD_NAME}/version" 2>/dev/null || echo unknown)"
echo "Loaded srcversion:        $loaded"
echo "Installed srcversion:     $installed"
if [[ "$loaded" != "$installed" ]]; then
    echo "ERROR: the loaded module does not match the installed one." >&2
    exit 1
fi

echo
echo "Charge limit: $(cat /sys/class/power_supply/BAT0/charge_control_end_threshold)%"
echo "Battery:      $(cat /sys/class/power_supply/BAT0/capacity)% ($(cat /sys/class/power_supply/BAT0/status))"
