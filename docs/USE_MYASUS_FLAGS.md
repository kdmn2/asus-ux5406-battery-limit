# The `use_myasus_flags` parameter

## Short version

The firmware's charge-limit method takes a second, hidden argument. The
Windows MyASUS app appears to use special "magic" numbers that set it. This
module can send those numbers when `use_myasus_flags=1`. It is **off by
default** and **unverified on hardware**, so leave it off.

## What the firmware does with the number

The write goes through `\_SB.PC00.LPCB.EC0.SRSC` in the BIOS (`DSDT.dsl`,
around line 71088). It takes one value and decides what two bytes to send to
the EC:

| Value sent by the module | Percentage byte sent to EC | Flag byte sent to EC | Notes |
|--------------------------|----------------------------|----------------------|-------|
| `80` (normal mode)       | `0x50` (80)                | `0x00`               | Used by default |
| `100` (normal mode)      | `0x64` (100)               | `0x00`               | Used by default |
| `0x80` (flag mode, 80%)  | `0x50` (80)                | `0x01`               | Only with `use_myasus_flags=1` |
| `0x100` (flag mode, 100%)| `0x64` (100)               | `0x80`               | Only with `use_myasus_flags=1` |
| `0x180`                  | `0x64` (100)               | `0x81`               | Not used by this module |
| any other value          | the value itself           | `0x00`               | Plain percentage |

So flag mode does not change the percentage. It only adds a second byte whose
meaning is not documented anywhere the author could find.

## What is known and what is not

Known from the BIOS source:
- The second byte is sent to the EC at the same time as the percentage.
- The BIOS's own readback code checks the first byte only.

Not known:
- What the EC does with the flag byte. The earlier session's notes say the
  `0x01` bit was ignored on this model. That claim has **not** been tested.
- Whether MyASUS's flag values are needed for anything beyond the percentage.

## Why it is off by default

- Plain values (80, 100) are the path already confirmed on hardware.
- The flag path only changes the second byte, but nothing has checked what
  that byte does, so it is an untested write to the EC.
- Because the parameter is writable at runtime, the write mode can be changed
  without reloading the module.

## Options (decision pending)

| Option | Pros | Cons |
|--------|------|------|
| Keep as is (0644) | No code change; you can flip it without rebuilding. | Runtime-writable, so the write mode can change silently; untested path stays reachable. |
| Remove it | Smallest code; no untested write path; nothing to misconfigure. | If a future firmware needs the flags, the code must be re-added. |
| Read-only (0444) | Stops runtime changes; still visible in `/sys/module/asus_ux5406_battery/parameters/`, useful for diagnosis. | Still settable at load time via `/etc/modprobe.d`, so it does not close the path. |

Recommendation: remove it, unless you have a specific reason to test the flags.

## If you want to test it anyway

Only on a machine you can recover, and only after the normal tests pass:
1. Set `use_myasus_flags=1` in `/etc/modprobe.d/asus_ux5406_battery.conf` and reload.
2. Write 80. Check the EC readback (`ec_rsoc_raw`) and the battery status.
3. Set the limit back to 80 in normal mode (`use_myasus_flags=0`) before finishing.
4. Watch `journalctl -k` for `DEVS(...) returned` warnings.
