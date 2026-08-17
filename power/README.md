# talkman power / thermal-engine

Snappiness is configured here in the device tree, not in the kernel
sources and not with Magisk.

`init.talkman.rc` (`on boot`) runs `/vendor/bin/init.talkman.power.sh`,
which writes kernel sysfs (interactive governor, `cpu_boost`, HMP
up/down migrate, kgsl min/default pwrlevel). Max CPU frequencies stay
1.44 GHz (A53) / 1.82 GHz (A57).

`device.mk` already ships `android.hardware.power-service-qti` (HIDL
Power HAL). The current prebuilt LGE `vendor.img` does not contain that
service, so a ROM rebuild is what actually installs it.

## Why thermal-engine died on the prebuilt vendor

CAF `thermal-engine` has a DT_NEEDED on `libqti-perfd-client.so`. That
library is **not** in the talkman LGE vendor image, so the daemon never
started:

```
CANNOT LINK EXECUTABLE "/vendor/bin/thermal-engine":
library "libqti-perfd-client.so" not found
```

`proprietary-blobs.txt` already lists `/system/bin/perfd` and
`/system/bin/thermal-engine`. It does **not** list the client library,
and the LGE dump does not provide it either.

The in-tree stub (`libqti-perfd-client/`) is enough for thermal-engine
to start. Talkman's `thermal-engine-8992.conf` applies CPU/GPU/LCD
policy through ioctl/sysfs; `perf_lock_use_profile` is optional and the
stub returns -1 ("no profile").

## Optional msm8992 blobs (not in this git tree)

Do **not** commit Qualcomm binaries here. If you want real MP-CTL
perflocks (Power HAL launch/interaction hints), copy the **msm8992 /
Android 7–8** blobs from Nexus 5X (bullhead). Do not use msm8996 or
Pie perfd.

Source (TheMuppets `proprietary_vendor_lge`, branch **lineage-16.0**):

| Blob | Upstream path |
|---|---|
| `perfd` | `bullhead/proprietary/bin/perfd` |
| 64-bit client | `bullhead/proprietary/vendor/lib64/libqti-perfd-client.so` |
| 32-bit client | `bullhead/proprietary/vendor/lib/libqti-perfd-client.so` |

Place them in the vendor image (same names thermal-engine / Power HAL
already look for):

| Install path | Notes |
|---|---|
| `/vendor/bin/perfd` | also listed as `/system/bin/perfd` in `proprietary-blobs.txt` |
| `/vendor/lib64/libqti-perfd-client.so` | replaces the stub at runtime |
| `/vendor/lib/libqti-perfd-client.so` | 32-bit |

ELF check: Android 27, NEEDED `libthermalclient` on `perfd` (already on
talkman), client NEEDED `libc++` / `libc` / `libm` / `libdl`.

If those files are present in the vendor extract, drop
`libqti-perfd-client` from `PRODUCT_PACKAGES` so the stub and the blob
do not collide as the same make module.

Socket after `perfd` starts: `/data/misc/perfd/mpctl`.
