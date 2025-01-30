#
# System prop for Lumia 950 (talkman)
#

# Audio
PRODUCT_PROPERTY_OVERRIDES += \
    persist.bt.enableAptXHD=true \
    persist.service.btui.use_aptx=1 \
    persist.vendor.bt.a2dp_offload_cap=sbc-aptx-aptxtws-aptxhd-aac-ldac \
    persist.vendor.btstack.a2dp_offload_cap=sbc-aptx-aptxtws-aptxhd-aac-ldac \
    media.aac_51_output_enabled=true \
    ro.qc.sdk.audio.fluencetype=fluencepro \
    persist.audio.fluence.voicecall=true \
    persist.audio.fluence.voicecomm=true \
    persist.audio.fluence.voicerec=false \
    persist.audio.fluence.speaker=true \
    persist.speaker.prot.enable=true \
    ro.config.vc_call_vol_steps=7 \
    af.fast_track_multiplier=1 \
    audio_hal.period_size=192 \
    ro.audio.flinger_standbytime_ms=300

# Bluetooth
PRODUCT_PROPERTY_OVERRIDES += \
    qcom.bluetooth.soc=rome \
    bluetooth.enable_timeout_ms=12000 \
    ro.boot.btmacaddr=00:00:00:00:00:00 \
    ro.bt.bdaddr_path=/persist/bdaddr.txt

# Blur - Diable Blur in A12+
PRODUCT_PROPERTY_OVERRIDES += \
     ro.surface_flinger.supports_background_blur=0 \
     ro.sf.blurs_are_expensive=0

# Camera
PRODUCT_PROPERTY_OVERRIDES += \
    persist.camera.eis.enable=1 \
    persist.camera.is_type=4 \
    persist.camera.tnr.preview=0 \
    persist.camera.tnr.video=0 \
    persist.camera.HAL3.enabled=1 \
    ro.camera.notify_nfc=1

# Configstore
PRODUCT_PROPERTY_OVERRIDES += \
    ro.surface_flinger.force_hwc_copy_for_virtual_displays=true \
    ro.surface_flinger.max_frame_buffer_acquired_buffers=3 \
    ro.surface_flinger.max_virtual_display_dimension=2048 \
    ro.surface_flinger.use_color_management=true \
    ro.surface_flinger.protected_contents=true \
    ro.surface_flinger.vsync_event_phase_offset_ns=2000000 \
    ro.surface_flinger.vsync_sf_event_phase_offset_ns=6000000

# CTS
PRODUCT_PROPERTY_OVERRIDES += \
    ro.product.first_api_level=23

# Dalvik
PRODUCT_PROPERTY_OVERRIDES += \
    dalvik.vm.boot-dex2oat-threads=4 \
    dalvik.vm.dex2oat-threads=4 \
    dalvik.vm.image-dex2oat-threads=4 \
    ro.hwui.texture_cache_size=56 \
    ro.hwui.layer_cache_size=32 \
    ro.hwui.r_buffer_cache_size=8 \
    ro.hwui.path_cache_size=16 \
    ro.hwui.gradient_cache_size=1 \
    ro.hwui.drop_shadow_cache_size=6 \
    ro.hwui.texture_cache_flushrate=0.4 \
    ro.hwui.text_small_cache_width=1024 \
    ro.hwui.text_small_cache_height=1024 \
    ro.hwui.text_large_cache_width=2048 \
    ro.hwui.text_large_cache_height=1024

# Disable rescue party
PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.disable_rescue=true

# Display
PRODUCT_PROPERTY_OVERRIDES += \
    ro.opengles.version=196610 \
    ro.sf.lcd_density=420 \
    persist.hwc.mdpcomp.enable=true

# DRM
PRODUCT_PROPERTY_OVERRIDES += \
    drm.service.enabled=true

# Faceunlock
PRODUCT_PROPERTY_OVERRIDES += \
    ro.facelock.black_timeout=700 \
    ro.facelock.det_timeout=2500 \
    ro.facelock.rec_timeout=3500 \
    ro.facelock.est_max_time=600

# FRP
PRODUCT_PROPERTY_OVERRIDES += \
    ro.frp.pst=/dev/block/platform/soc.0/f9824900.sdhci/by-name/persistent

# Media
PRODUCT_PROPERTY_OVERRIDES += \
    debug.stagefright.omx_default_rank.sw-audio=1 \
    debug.stagefright.omx_default_rank=0 \
    vidc.debug.perf.mode=2 \
    vidc.enc.dcvs.extra-buff-count=2 \
    media.recorder.show_manufacturer_and_model=true \
    persist.media.treble_omx=false \

# Perf
PRODUCT_PROPERTY_OVERRIDES += \
    ro.min_freq_0=384000 \
    ro.min_freq_4=384000

# IMS
PRODUCT_PROPERTY_OVERRIDES += \
    persist.dbg.ims_volte_enable=1 \
    persist.dbg.volte_avail_ovr=1 \
    persist.dbg.vt_avail_ovr=1 \
    persist.dbg.wfc_avail_ovr=1

# Properties for Surfaceflinger
PRODUCT_PROPERTY_OVERRIDES += \
    debug.renderengine.backend=threaded

# RIL
PRODUCT_PROPERTY_OVERRIDES += \
    ro.vendor.extension_library=libqti-perfd-client.so \
    vendor.rild.libpath=/vendor/lib64/libril-qc-qmi-1.so \
    ro.telephony.default_cdma_sub=0 \
    persist.qcril.disable_retry=true \
    persist.radio.always_send_plmn=true \
    ro.telephony.call_ring.multiple=0 \
    persist.radio.use_cc_names=true \
    persist.radio.eri64_as_home=1 \
    persist.radio.rat_on=combine \
    persist.radio.data_ltd_sys_ind=1 \
    persist.radio.data_con_rprt=1 \
    persist.radio.calls.on.ims=1 \
    persist.radio.apm_sim_not_pwdn=1 \
    persist.radio.custom_ecc=1 \
    persist.radio.data_no_toggle=1 \
    persist.radio.process_sups_ind=1 \
    ro.telephony.default_network=22 \
    telephony.lteOnCdmaDevice=1 \
    persist.radio.mode_pref_nv10=1 \
    ril.nosim.ecc_list_count=1 \
    ril.nosim.ecc_list_1=111,113,117,122,125 \
    persist.radio.redir_party_num=0 \
    persist.vendor.radio.rat_on=combine

# SdcardFs
PRODUCT_PROPERTY_OVERRIDES += \
    persist.fuse_sdcard=true

# Wifi
PRODUCT_PROPERTY_OVERRIDES += \
    wifi.interface=wlan0 \
    wifi.supplicant_scan_interval=15

# BPF bypass
PRODUCT_PROPERTY_OVERRIDES += \
    ro.kernel.ebpf.supported=false

# VNDK
PRODUCT_PROPERTY_OVERRIDES += \
    ro.vndk.version=current

# Xml
PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.binary_xml=false
