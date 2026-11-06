#!/vendor/bin/sh
# Bind the GW USIM after stock qcril is up. WP EFS has automatic_selection=0
# and halt_subscription=1, so qcril never publishes the subscription.
# Then publish the primary NAS subscription (0x004b / DUAL_STANDBY_PREF) and
# start automatic registration. Without 0x004b the modem stays in POWER_SAVE
# with reg=0 even though the USIM is READY (measured on Talkman).
# Does not copy EFS, RESET, write NV, vote RF rails, or mux GPIOs.

export LD_LIBRARY_PATH=/vendor/lib64:/system/vendor/lib64
DMS=/vendor/bin/dms-tool
UIM=/vendor/bin/uim-tool
NAS=/vendor/bin/nas-tool

logmsg() {
    /system/bin/log -t talkman-ril "$*"
}

mode_of() {
    "$DMS" get-mode 2>/dev/null | sed -n 's/.*mode=\([0-9][0-9]*\).*/\1/p' | head -1
}

card_present() {
    "$UIM" card 2>/dev/null | grep -q "state=PRESENT"
}

card_ready() {
    "$UIM" card 2>/dev/null | grep -q "type=USIM state=READY"
}

camped() {
    "$NAS" serving 2>/dev/null | grep -q "reg=1"
}

# Publish the primary subscription mask. The vendor firmware expects the
# Cityman encoding: TLV 0x14, length 8, value 00 01 00 00 00 00 00 00.
# (A value of 01 00 ... is rejected with QMI_ERR_MALFORMED_MSG.)
activate() {
    "$NAS" send 0x4b 14 08 00 01 00 00 00 00 00 00 00
}

# Prefer stock qcril RADIO_POWER for LPM->ONLINE; only poke DMS if it stays LPM.
bring_online() {
    i=0
    m=$(mode_of)
    while [ "$i" -lt 40 ] && [ "$m" != "0" ] && [ "$m" != "1" ] && [ "$m" != "2" ]; do
        i=$((i + 1))
        sleep 2
        m=$(mode_of)
    done
    logmsg "mode=$m"
    if [ "$m" = "2" ]; then
        logmsg "FTM -> LPM"
        "$DMS" set-mode 1
        sleep 4
        m=$(mode_of)
        logmsg "after LPM mode=$m"
    fi
    i=0
    while [ "$i" -lt 30 ] && [ "$m" = "1" ]; do
        i=$((i + 1))
        sleep 2
        m=$(mode_of)
    done
    if [ "$m" = "1" ]; then
        logmsg "LPM still set; asking ONLINE"
        "$DMS" set-mode 0
        sleep 4
        m=$(mode_of)
    fi
    logmsg "mode=$m"
    [ "$m" = "0" ]
}

# Bind the USIM, wait for READY, publish the subscription and register.
provision_once() {
    logmsg "provision-gw"
    "$UIM" provision-gw
    # SUBSCRIPTION_OK can be accepted before the application reaches READY.
    i=0
    while [ "$i" -lt 20 ] && ! card_ready; do
        i=$((i + 1))
        sleep 2
    done
    if [ -x "$NAS" ]; then
        logmsg "activate subscription"
        activate
        logmsg "nas register+search"
        "$NAS" register
        "$NAS" search
    fi
}

logmsg "start"
i=0
while [ "$i" -lt 90 ]; do
    if [ -x "$DMS" ] && [ -e /dev/subsys_modem ]; then
        m=$(mode_of)
        if [ -n "$m" ]; then
            break
        fi
    fi
    i=$((i + 1))
    sleep 2
done
if ! [ -x "$DMS" ] || ! [ -e /dev/subsys_modem ]; then
    logmsg "no dms-tool, subsys_modem, or QCCI"
    exit 0
fi

if bring_online; then
    if card_present; then
        provision_once
    else
        logmsg "no card PRESENT (waiting for hot-insert)"
    fi
    logmsg "done mode=$(mode_of) sim=$(getprop gsm.sim.state) camped=$(camped && echo yes || echo no)"
else
    logmsg "modem not ONLINE yet; watchdog will retry"
fi

# Watchdog: an SSR clears the active-subscription mask and leaves the USIM
# DETECTED. Re-provision and re-activate when the modem is online but not
# camped. Stay quiet while registration is healthy.
while true; do
    sleep 15
    m=$(mode_of)
    if [ "$m" != "0" ]; then
        if [ "$m" = "1" ]; then
            logmsg "watchdog: LPM still set; asking ONLINE"
            "$DMS" set-mode 0
        fi
        continue
    fi
    if ! card_ready; then
        if card_present; then
            logmsg "watchdog: reprovision"
            provision_once
        fi
        continue
    fi
    if ! camped; then
        logmsg "watchdog: reactivate"
        activate
        "$NAS" register
        "$NAS" search
    fi
done
