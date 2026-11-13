# Testing

## Offline (no hardware)

```sh
make -C /lib/modules/$(uname -r)/build M=$PWD W=1 modules
modinfo ./asus_ux5406_battery.ko | grep -E 'version|srcversion'
```

## Read-only checks

```sh
cat /sys/class/power_supply/BAT0/charge_control_end_threshold
cat /sys/class/power_supply/BAT0/ec_rsoc_raw       # expect e.g. "50 2c 05" at 80
cat /sys/module/asus_ux5406_battery/version
journalctl -k -b | grep -E 'asus_ux5406|DEVS'
```

## Hardware tests (change the charge limit)

Run these only on a machine you can recover, and only one at a time.

1. **Set and read back**: write 60, check the readback, then 80, then 100.
2. **Reject invalid values**: `echo 90 > ...` must fail with `Invalid argument`.
3. **Retention**: set 60, reboot, check the value. Then suspend and check.
4. **Resume restore**: set 60, suspend and resume, check the readback is 60.
5. **Failed-write path** (1.1.0): not reproducible on demand. Look for
   `returned` warnings in the journal after normal use.

Record each result in the table below.

| Test | Date | Kernel | Result |
|------|------|--------|--------|
| 1 | | | |
| 2 | | | |
| 3 | | | |
| 4 | | | |
