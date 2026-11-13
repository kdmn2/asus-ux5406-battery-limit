# Changelog

## 1.1.2

- Removed the `use_myasus_flags` parameter. Testing showed that writing 80
  with the flag set left the battery at 100% with no limit.
- Documented the MyASUS test: MyASUS leaves byte 0 of the EC at `0xd0`,
  which the module's readback check rejects.

## 1.1.1

- The resume restore now logs that it ran and what the EC reads back
  afterwards (`pm event N, restoring X%` / `EC after restore: ...`).
- Added `test-resume.sh` to check that the limit survives a suspend.
- Documented the asusd interaction: asusd re-applies its own stored limit
  (default 100) on every wake. Set it with `asusctl battery limit 80`.

## 1.1.0

- Writes that the firmware does not confirm (`DEVS` result other than 1) now
  return `-EIO` and do not change the stored value. Previously they were
  reported as success.
- Only 60, 80 and 100 are accepted. Other values return `-EINVAL`.
- Reading the limit now returns an error if the EC read fails. It no longer
  falls back to the cached value.
- A mutex serialises the module's own writes (sysfs, init, resume).
- The module refuses to load if `asus-wmi` already supports the threshold
  natively, to avoid two drivers owning the same sysfs file.
- Build: clang is detected from the kernel's `.config`. The hardcoded
  `LLVM=1` in `dkms.conf` is removed; it broke builds for GCC kernels (LTS).
- `install.sh` builds for every installed kernel and checks the loaded module
  matches the installed one. Added `uninstall.sh` and `THRESHOLD` option.

## 1.0.0

- Initial version: attaches to `BAT0`, exposes `charge_control_end_threshold`
  and `ec_rsoc_raw`, default limit 80, re-applies after resume.
