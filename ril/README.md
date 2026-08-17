# Talkman RIL groundwork (RM-1104)

This is **not** working cellular radio. It only ships the userspace that gets a
physical USIM to Android `LOADED`. The modem still does not key a WWAN band or
camp.

Stock `rild` / `libril-qc-qmi-1` stays. The tree used to list `qmihal`, which
this image never shipped. These helpers are original QCCI clients (public QMI
message IDs). They are **not** a port of that HAL.

## What works

- MSS PIL comes **ONLINE**. After the one-time WP EFS copy onto Android
  `modemst`/`fsg`/`fsc`, a reboot boots **LPM** and qcril `RADIO_POWER` takes
  **ONLINE**. Do not recopy those partitions.
- WP EFS has `automatic_selection=0` and `halt_subscription=1`. Stock qcril
  does not bind the GW USIM or send `QMI_UIM_SUBSCRIPTION_OK` (`0x0040`).
- `talkman-ril.sh` (after `sys.boot_completed`) waits for `/dev/subsys_modem`,
  leaves FTM if needed, prefers qcril for LPM→ONLINE, then
  `uim-tool provision-gw` (CHANGE_PROVISIONING + `0x0040`). A present card
  reaches modem USIM **READY** and Android `gsm.sim.state=LOADED`.
- `nas-tool register` + `search` after the bind so NAS leaves POWER_SAVE.
  Search returns success; it does **not** camp.
- Airplane on/off keeps READY. Empty tray times out and exits.

## What does not work

WWAN camp / attach. Live NAS stays `reg=2` searching with `first_if=NONE`,
CS/PS detached, GSM/WCDMA/LTE NONE, RF/SIG **QMI 74**. GNSS RX does key.
Policyman is open (MCC 232, GSM+WCDMA+LTE). DIAG saw CM allocate from
`qmi_mmode` and never schedule SD/WWAN L1. Further search / SAR / airplane /
AP RF-rail votes / GPIO mux / DIAG EFS writes do not close that gap.

## Do not

- Recopy `MODEM_FS*` / `modemst*` / `fsg` / `fsc` on boot (runtime NV diverges).
- Flash random `mba` / `modem` PIL.
- Mux TLMM GPIO 53/54 (CNSS WLAN bootstrap, not a SIM fix).
- Write NV 10 / `svc_mode`, or replay NVI / RetailModeNvi / simlock.
- Vendor `Android4Lumia950/qmihal` into this tree.
- Raise charger ICL/FCC or touch UC120 as part of radio work.

## Pickup

Userspace QMI/DIAG on this Android image is a dead end for camp. Next useful
work is a QMI+DIAG trace on a stack that **does** camp (Windows Phone / WOA on
a second talkman), then compare what that stack sends that this one does not.
A kernel always-on L8 vote is the wrong next experiment until CM actually
schedules an acquire.

`system.prop` already has `persist.radio.rat_on=combine`.
