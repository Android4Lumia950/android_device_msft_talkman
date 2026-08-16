# Talkman NFC configs

Kernel side lives in `android_kernel_mmo_msm8994` (`lineage-18.1-talkman`):
`nq-nci` binds the PN547 on I²C6 as `/dev/pn547`.

These two files are the userspace half:

| File | Why it differs from the old overlay |
|---|---|
| `libnfc-nxp.conf` | PLL / 19.2 MHz (BBCLK2), NFC-A 106 kbit/s RF block, Classic keys |
| `libnfc-nci.conf` | `LEGACY_MIFARE_READER=1`, Kovio poll `0x77`, presence-check 0 |

NTAG / Type 2 and ISO-DEP (Wallet) work from the kernel alone.
Mifare Classic 1K NDEF needs these configs.

Antenna is the back cover, Qi / center — not the top edge.
Leave extra NFC reader apps closed; two clients on the PN547 can
wedge `com.android.nfc`.
