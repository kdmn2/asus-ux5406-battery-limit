# Testing

## Offline (no hardware)

```sh
make -C /lib/modules/$(uname -r)/build M=$PWD W=1 modules
modinfo ./asus_ux5406_battery.ko | grep -E 'version|srcversion'
```

## Read-only checks

```sh
cat /sys/class/power_supply/BAT0/charge_control_end_threshold
cat /sys/class/power_supply/BAT0/ec_rsoc_raw       # e.g. "50 28 05" at 80
cat /sys/module/asus_ux5406_battery/version
journalctl -k -b | grep -E 'asus_ux5406|DEVS'
```

## Hardware tests (change the charge limit)

1. **Set and read back**: write 100, 60 and 80; check byte 0 of `ec_rsoc_raw`.
2. **Reject invalid values**: `echo 90 > ...` must fail with `Invalid argument`.
3. **Suspend**: `TARGET=80 ./test-resume.sh before`, `systemctl suspend`,
   `./test-resume.sh after`. If asusd runs, `TARGET` must match
   `asusctl battery info`.
4. **Hibernate**: same as 3 with `systemctl hibernate`.
5. **Charging stops at the limit**: from about 70% on AC, log `capacity`,
   `status` and `power_now` until past the limit.
6. **LTS kernel**: boot the LTS kernel and repeat 1.

## Results (UX5406AA, BIOS 310)

| Test | Date | Kernel | Result |
|------|------|--------|--------|
| 1 | 2026-11-13 | 7.3.3-1-cachyos | PASS: 100 -> `64 08 05`, 60 -> `3c 28 05`, 80 -> `50 28 05` |
| 2 | 2026-11-13 | 7.3.3-1-cachyos | PASS: 90 rejected, limit stayed 80 |
| 3 | 2026-11-13 | 7.3.3-1-cachyos | FAIL with asusd at its default 100: module restored 60 (`3c`), asusd then wrote 100 on wake |
| 3 | 2026-11-13 | 7.3.3-1-cachyos | PASS after `asusctl battery limit 80`: `50 28 05` after s2idle |
| 4 | 2026-11-13 | 7.3.3-1-cachyos | MyASUS at 80%: EC reads `d0 28 05` before module write (not a valid limit) |
| 4b | 2026-11-13 | 7.3.3-1-cachyos | `use_myasus_flags=1`, write 80: EC reads `64 38 05` (100%, no limit). Flag option removed in 1.1.2 |
| 5 | 2026-11-13 | 7.3.3-1-cachyos | PASS: charging from 78% at 15 W; at 80% (20:26:38) power dropped to 1.9 W then 0 W, status "Not charging", EC `50 2c 05`, charger still connected |
| 6 | | | not run |

## Notes from testing

- Byte 1 of `ec_rsoc_raw` is not a fixed mode value. It has read `2c`, `28`
  and `08` and seems to follow the charge state.
- The EC accepts writes right after resume. The limit losses seen after
  resume were caused by asusd writing its own value.
- Not known: whether the EC keeps the limit across suspend by itself. The
  module re-applies it on every wake, so the tests cannot tell.
