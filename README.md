# asus-ux5406-battery

Out-of-tree Linux kernel module that lets you set the battery charge limit
(`charge_control_end_threshold`) on the **ASUS Zenbook S14 UX5406AA**.

On this model the mainline `asus-wmi` driver does not expose the threshold,
because its BIOS returns a buffer where the driver expects an integer. This
module calls the firmware directly, with the same EC command the BIOS uses.

- Standard sysfs attribute: `/sys/class/power_supply/BAT0/charge_control_end_threshold`
- Allowed values: **60, 80, 100**
- Default on load: **80**
- Reads the real limit back from the embedded controller (EC)
- Re-applies the limit after suspend, hibernate and restore
- Rebuilt automatically by DKMS when the kernel is updated

## Status

| Version | Built | Tested on hardware |
|---------|-------|--------------------|
| 1.0.0   | yes   | yes: 80% set, EC reads back `50 2c 05`, `status: Not charging` at 88% |
| 1.1.0   | yes, offline (7.3.3 and 6.18 LTS) | **not yet** |

1.1.0 changes behaviour (see [CHANGELOG](CHANGELOG.md)). Treat it as unverified
until you have run the tests in [docs/TESTING.md](docs/TESTING.md).

## Requirements

- ASUS Zenbook S14 UX5406AA (confirm with `cat /sys/class/dmi/id/board_name`)
- CachyOS or Arch Linux
- `dkms`
- Kernel headers for every kernel you boot, e.g. `linux-cachyos-headers`
  (and `linux-cachyos-lts-headers` if you use the LTS kernel)
- For clang-built kernels (the CachyOS default), `clang` and `lld` must be
  installed. The Makefile detects this automatically.
- `sudo` access

## Install

```sh
git clone https://github.com/kdmn2/asus-ux5406-battery-limit.git
cd asus-ux5406-battery-limit
./install.sh                 # default limit: 80
THRESHOLD=60 ./install.sh    # or 60 / 100
```

`install.sh` will:
1. remove any older DKMS registrations of this module,
2. copy the source into `/usr/src/asus-ux5406-battery-1.1.0`,
3. build and install it for every kernel that has headers,
4. write `/etc/modules-load.d/asus_ux5406_battery.conf` and
   `/etc/modprobe.d/asus_ux5406_battery.conf` (boot-time load and threshold),
5. reload the module and confirm the loaded version matches the installed one.

Check the result:

```sh
cat /sys/class/power_supply/BAT0/charge_control_end_threshold
cat /sys/module/asus_ux5406_battery/version
dkms status asus-ux5406-battery
```

## Change the limit

```sh
echo 60 | sudo tee /sys/class/power_supply/BAT0/charge_control_end_threshold
```

To make a different value permanent, edit `/etc/modprobe.d/asus_ux5406_battery.conf`
or re-run `THRESHOLD=<value> ./install.sh`.

## Remove

```sh
./uninstall.sh
```

This sets the limit back to 100, unloads the module and removes DKMS, the
source copy and the boot configuration.

## Reinstalling the OS

Everything needed is in this repository. After a fresh install:

1. Install the requirements above (`sudo pacman -S dkms clang lld linux-cachyos-headers`).
2. Clone the repository and run `./install.sh`.
3. Reboot and check the limit with the commands above.

If you use MyASUS on Windows, do not change the limit there and in Linux at
the same time. Both write the same EC setting.

## Troubleshooting

- **`install.sh` fails at `dkms install`**: check `/var/lib/dkms/asus-ux5406-battery/1.1.0/build/make.log`.
- **Module loads but writes fail with `-EIO`**: the firmware returned an
  unexpected result. Check `journalctl -k -b | grep asus_ux5406`.
- **Writes fail with `-ENODEV`**: the firmware does not implement the method.
  Your board is probably not a UX5406AA; do not force it.
- **`asus-wmi already supports RSOC, not loading`**: a future kernel's
  `asus-wmi` handles the threshold natively. Use that instead.

## How it works

See [docs/FIRMWARE_NOTES.md](docs/FIRMWARE_NOTES.md) for the ACPI call chain,
the EC mailbox protocol and the source references.

Read [docs/USE_MYASUS_FLAGS.md](docs/USE_MYASUS_FLAGS.md) before enabling
the `use_myasus_flags` parameter. It is off by default and unverified.
