/* AudioHardwareALSACommon.h
 **
 ** Copyright 2012 Intel Corporation
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#ifndef ANDROID_AUDIO_HARDWARE_ALSA_COMMON_H
#define ANDROID_AUDIO_HARDWARE_ALSA_COMMON_H

#include <hardware_legacy/AudioHardwareBase.h>

#include <tinyalsa/asoundlib.h>

#include <hardware/hardware.h>

#define NULL_IN_DEVICE 0

using namespace android;

namespace android_audio_legacy
{

/**
 * The id of TinyALSA module
 */
#define TINYALSA_HARDWARE_MODULE_ID "tinyalsa"
#define TINYALSA_HARDWARE_NAME      "tinyalsa"

struct alsa_device_t;

struct alsa_handle_t {
    alsa_device_t *     module;

    pcm*                handle;
    pcm_config          config;
    int                 flags;

    void *              modPrivate;
    bool                openFlag;        // If handle is opened then openFlag = 1
};

struct alsa_device_t {
    hw_device_t common;

    // Methods
    status_t (*init)(alsa_device_t *);
    status_t (*open)(alsa_handle_t *, int, int, const pcm_config&);
    status_t (*stop)(alsa_handle_t *);
    status_t (*standby)(alsa_handle_t *);
    status_t (*close)(alsa_handle_t *);
    status_t (*volume)(alsa_handle_t *, uint32_t, float);
    status_t (*initStream)(alsa_handle_t *, bool, uint32_t, uint32_t, pcm_format);
};

/**
 * The id of acoustics module
 */
#define ACOUSTICS_HARDWARE_MODULE_ID    "acoustics"
#define ACOUSTICS_HARDWARE_NAME         "acoustics"

struct acoustic_device_t {
    hw_device_t common;

    // Required methods...
    status_t (*use_handle)(acoustic_device_t *, alsa_handle_t *);
    status_t (*cleanup)(acoustic_device_t *);

    status_t (*set_params)(acoustic_device_t *, AudioSystem::audio_in_acoustics, void *);

    // Optional methods...
    ssize_t (*read)(acoustic_device_t *, void *, size_t);
    ssize_t (*write)(acoustic_device_t *, const void *, size_t);
    status_t (*recover)(acoustic_device_t *, int);

    void *              modPrivate;
};

// ----------------------------------------------------------------------------

};        // namespace android
#endif    // ANDROID_AUDIO_HARDWARE_ALSA_COMMON_H
