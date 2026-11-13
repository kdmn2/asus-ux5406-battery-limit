# Firmware notes: UX5406AA charge limit

These notes cover how the BIOS exposes the charge limit. They point to the
relevant parts of the DSDT. Source material is not included; the full
dumps are ASUS firmware and are not redistributed here.

Reference: BIOS `UX5406AA.310` (09/01/2026), DSDT length `0x67708`, OEM
revision `0x01072009`.

## Method chain

| Step | ACPI location | Role |
|------|---------------|------|
| WMI entry | `\_SB.ATKD.WMNB` (~line 67397) | Dispatches WMI calls by method ID |
| Read status | `WMNB`, DSTS branch, `IIA0 == 0x00120057` (~67932-67976) | Returns an 8-byte **buffer**, not an integer |
| Write limit | `WMNB`, DEVS branch, `IIA0 == 0x00120057` (~68490) | Calls `EC0.SRSC(IIA1)`, returns its result |
| EC write | `EC0.SRSC` (~71088) | Builds `{percent, flags}` and calls `WEBC(0x06, 2, ...)` |
| EC read | `EC0.REBC(cmd, len)` (~70692) | Reads `len` bytes of the mailbox |
| EC mailbox | `EC0.WEBC` (~70653) | Posts a command, returns 1 when posted |

## The DSTS buffer (why `asus-wmi` misses the feature)

`DSTS(0x00120057)` returns `BCMV`, an 8-byte buffer. The low dword `BCML`
holds flags; bit `0x00010000` is the presence bit that `asus-wmi` tests. Mainline
`asus_wmi_evaluate_method3()` only accepts `ACPI_TYPE_INTEGER`, so it reads
the buffer as 0 and never registers the threshold attribute.

Flags set in `BCML` from `REBC(0x05, 0x03)` byte 1:
- bit `0x2000` is set when byte 0 equals `0x50` (80%).
- bits `0x200`, `0x800`, `0x100000`, `0x1000000` mirror byte 1 bits.

The DSTS buffer does **not** contain the threshold percentage. Only the
`0x2000` bit says "80%". Reading the exact value requires `REBC`.

## The EC mailbox

- Ports: `BRAH` at `0x08A1`, `BRAI`/`BRAD` at `0x08A2`/`0x08A3` (SystemIO).
- A mutex `CMUT` in `EC0` guards the mailbox. `REBC`, `WEBC` and most other
  accessors acquire it. One access in `FBST` (~line 72567) writes `BRAH`
  without `CMUT`; it writes the same constant every other caller uses.
- `WEBC` returns `1` once the command is **posted**. It does not wait for the
  EC to finish, so a read immediately after a write may still show the old
  value. The module does not yet check how long this takes.
- `WEBC` returns `0` if the mailbox stays busy for about 20 ms; nothing is posted.

## Readback bytes (`REBC(0x05, 0x03)`)

Observed on this machine: `50 2c 05` with the limit at 80 and the battery at 88%.

| Byte | Meaning (inferred) |
|------|--------------------|
| 0 | Active limit in percent (`0x50` = 80, `0x64` = 100). Firmware compares it against 80 and 60. |
| 1 | EC status bits. Their meaning is not documented. |
| 2 | Version byte (`0x05`). |

`_BST` (~line 72630) calls `REBC(0x05, 0x02)` and sets the "charge limiting"
state when byte 0 is `0x50` or `0x3C` (60). The Linux battery core then
reports "Not charging". This means "Not charging" is a firmware-derived
status, not a measurement that charging has stopped.

## Values the firmware recognises

- `80` (`0x50`) and `60` (`0x3C`) are checked by the BIOS.
- `100` (`0x64`) is the default.
- Other values are stored as-is; the module only accepts 60, 80 and 100.

## Read-only commands

Reading `BAT0` `_BST` (for example `status` or `capacity`) runs `REBC(0x05, 0x02)`.
Reading the module's `ec_rsoc_raw` runs `REBC(0x05, 0x03)`. Both are read-only.

## Method argument for flags

See [USE_MYASUS_FLAGS.md](USE_MYASUS_FLAGS.md).
