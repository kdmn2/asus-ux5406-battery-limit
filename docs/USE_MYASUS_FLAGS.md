# The former `use_myasus_flags` option (removed in 1.1.2)

## Result

Removed in 1.1.2. Testing showed that the flag path leaves the battery with
**no charge limit**.

## What was tested (2026-11-13, BIOS UX5406AA.310, kernel 7.3.3)

1. MyASUS set to 80% in Windows, then restart into Linux (before the module
   wrote anything). The EC read `d0 28 05`. Byte 0 `0xd0` is not a valid
   limit value.
2. With `use_myasus_flags=1`, a write of 80 read back `64 38 05`: **100%**.
   The battery then discharged with no limit in place.
3. Restored to normal mode and 80%: `50 ..` (see TESTING.md for the final
   reading).

## Why it was removed

- The flag path produced a limit of 100% instead of 80%.
- Its meaning was never confirmed, and the only evidence contradicts the
  BIOS source reading.
- The plain write of 80 is the confirmed working path.

## Open question

MyASUS sets byte 0 to `0xd0`. The BIOS readback check uses `0x50`, so MyASUS's
state may not be recognized as an 80% limit. Whether it stops charging at 80%
is unknown. This matters only if you ever want to switch from MyASUS to Linux
mid-cycle, and it does not affect the module.
