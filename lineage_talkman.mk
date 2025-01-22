#
# Copyright 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Sample: This is where we'd set a backup provider if we had one
# $(call inherit-product, device/sample/products/backup_overlay.mk)

# Get the long list of APNs
PRODUCT_COPY_FILES := device/msft/talkman/configs/apns-full-conf.xml:system/etc/apns-conf.xml

# Inherit some common PixelExperience stuff.
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

# Inherit from the common Open Source product configuration
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/aosp_base_telephony.mk)

PRODUCT_NAME := lineage_talkman
PRODUCT_DEVICE := talkman
PRODUCT_BRAND := Microsoft
PRODUCT_MODEL := Lumia 950
PRODUCT_MANUFACTURER := Microsoft
PRODUCT_RESTRICT_VENDOR_FILES := false

# Vendor security patch level
PRODUCT_PROPERTY_OVERRIDES += \
    ro.lineage.build.vendor_security_patch=2018-11-01

#PRODUCT_COPY_FILES += device/msft/talkman/fstab.aosp_talkman:root/fstab.talkman

$(call inherit-product, device/msft/talkman/device.mk)
$(call inherit-product, vendor/msft/talkman/talkman-vendor.mk)

# Device Fingerprint
PRODUCT_BUILD_PROP_OVERRIDES += \
    PRIVATE_BUILD_DESC="bullhead-user 8.1.0 OPM3.171019.014 4503998 release-keys"

BUILD_FINGERPRINT=google/bullhead/bullhead:8.1.0/OPM3.171019.014/4503998:user/release-keys 

PRODUCT_PROPERTY_OVERRIDES += \
    ro.build.fingerprint=google/bullhead/bullhead:8.1.0/OPM3.171019.014/4503998:user/release-keys

PRODUCT_PACKAGES += \
    Launcher3 \
    WallpaperPicker



