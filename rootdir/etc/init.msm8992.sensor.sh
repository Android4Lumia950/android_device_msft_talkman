#!/system/bin/sh
# Copyright (c) 2009-2015, The Linux Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of The Linux Foundation nor
#       the names of its contributors may be used to endorse or promote
#       products derived from this software without specific prior written
#       permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

#
# Function to start sensors for DSPS enabled platforms
#
start_sensors()
{
        # LGE_START
        if [ ! -s /persist/sensors/sensors_settings ]; then
            # If the settings file is empty, enable sensors HAL
            # Otherwise leave the file with it's current contents
            echo 1 > /persist/sensors/sensors_settings
        fi
        # LGE_END
    if [ -c /dev/msm_dsps -o -c /dev/sensors ]; then
        mkdir -p /persist/sensors
        chmod 775 /persist/sensors
        chmod 664 /persist/sensors/sensors_settings
        chown -h system.root /persist/sensors/sensors_settings
        chmod 664 /persist/sensors/gyro_sensitity_cal
        chown -h system.root /persist/sensors/gyro_sensitity_cal

        mkdir -p /data/misc/sensors
        chmod 775 /data/misc/sensors

        # LGE_START, make /sns/cal folder for save sensor registy
        # If the sns directory exists, make sns/cal/ directory
        if [ -d /sns ]; then
          mkdir -p /sns/cal
        fi
        # LGE_END, make /sns/cal folder for save sensor registy


        start sensors
    fi
}

start_sensors