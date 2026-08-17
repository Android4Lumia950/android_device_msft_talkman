#!/vendor/bin/sh
# Bind the GW USIM after stock qcril is up. WP EFS has automatic_selection=0
# and halt_subscription=1, so qcril never publishes the subscription.
# Does not copy EFS, RESET, write NV, vote RF rails, or mux GPIOs.

export LD_LIBRARY_PATH=/vendor/lib64:/system/vendor/lib64
DMS=/vendor/bin/dms-tool
UIM=/vendor/bin/uim-tool
NAS=/vendor/bin/nas-tool

logmsg() {
    /system/bin/log -t talkman-ril "$*"
}

wait_modem() {
    i=0
    while [ "$i" -lt 90 ]; do
        if [ -x "$DMS" ] && [ -e /dev/subsys_modem ]; then
            m=$(mode_of)
            if [ -n "$m" ]; then
                return 0
            fi
        fi
        i=$((i + 1))
        sleep 2
    done
    return 1
}

mode_of() {
    "$DMS" get-mode 2>/dev/null | sed -n 's/.*mode=\([0-9][0-9]*\).*/\1/p' | head -1
}

card_present() {
    "$UIM" card 2>/dev/null | grep -q "state=PRESENT"
}

logmsg "start"
if ! wait_modem; then
    logmsg "no dms-tool, subsys_modem, or QCCI"
    exit 0
fi

# DMS 5 is shutting-down during early PIL; wait for a stable mode.
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
# Prefer stock qcril RADIO_POWER for LPM->ONLINE; only poke DMS if it stays LPM.
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
if [ "$m" != "0" ] || [ ! -x "$UIM" ]; then
    logmsg "skip provision mode=$m"
    exit 0
fi
i=0
while [ "$i" -lt 30 ] && ! card_present; do
    i=$((i + 1))
    sleep 2
done
if ! card_present; then
    logmsg "no card PRESENT (empty tray is fine)"
    logmsg "done mode=$m"
    exit 0
fi
logmsg "provision-gw"
"$UIM" provision-gw
# RADIO_POWER ran before the USIM was bound, so NAS stays POWER_SAVE
# until something starts automatic registration.
if [ -x "$NAS" ]; then
    logmsg "nas register+search"
    "$NAS" register
    "$NAS" search
fi
logmsg "done mode=$(mode_of) sim=$(getprop gsm.sim.state)"
exit 0
