/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Open-source stand-in for Qualcomm's proprietary libqti-perfd-client.so.
 *
 * talkman's CAF thermal-engine has a DT_NEEDED on this library, so the
 * process cannot even start without a file of this name on vendor. The
 * 8992 conf applies cluster / GPU / LCD / battery policy through
 * libthermalioctl and sysfs. MP-CTL profile switches are optional and
 * are resolved with dlsym("perf_lock_use_profile"); returning -1 there
 * is "no profile", not a crash.
 *
 * This is the same approach Lineage uses on LGE devices when perfd is
 * not shipped. It is not a dummy thermal-engine.
 */

int perf_lock_acq(int handle, int duration, int list[], int numArgs)
{
    (void)handle;
    (void)duration;
    (void)list;
    (void)numArgs;
    return -1;
}

int perf_lock_rel(int handle)
{
    (void)handle;
    return 0;
}

int perf_hint(int hint, const char *pkg, int duration, int type)
{
    (void)hint;
    (void)pkg;
    (void)duration;
    (void)type;
    return -1;
}

int perf_lock_use_profile(int handle, int profile)
{
    (void)handle;
    (void)profile;
    return -1;
}

int perf_lock_get_feedback(int req)
{
    (void)req;
    return 0;
}
