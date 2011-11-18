/* ALSAMixer.cpp
 **
 ** Copyright 2008-2009 Wind River Systems
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

#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>

#define LOG_TAG "AudioHardwareALSA"
#include <utils/Log.h>
#include <utils/String8.h>

#include <cutils/properties.h>
#include <media/AudioRecord.h>
#include <hardware_legacy/power.h>

#include "AudioHardwareALSA.h"

#define SND_MIXER_VOL_RANGE_MIN  (0)
#define SND_MIXER_VOL_RANGE_MAX  (100)

#define ALSA_NAME_MAX 128
#define MEDFIELDAUDIO "medfieldaudio"

namespace android_audio_legacy
{

// ----------------------------------------------------------------------------

struct mixer_info_t;

struct alsa_properties_t
{
    const AudioSystem::audio_devices device;
    const char         *propName;
    const char         *propDefault;
    mixer_info_t       *mInfo;
};

static alsa_properties_t mixerPropOut[] = {
    {AudioSystem::DEVICE_OUT_EARPIECE, "alsa.mixer.earpiece", "Headphone", NULL},
    {AudioSystem::DEVICE_OUT_SPEAKER, "alsa.mixer.speaker", "Speaker", NULL},
    {AudioSystem::DEVICE_OUT_WIRED_HEADSET, "alsa.mixer.headset", "Headphone", NULL},
    {AudioSystem::DEVICE_OUT_WIRED_HEADPHONE, "alsa.mixer.headphone", "Headphone", NULL},
    {static_cast<AudioSystem::audio_devices>(0), "alsa.mixer.", NULL, NULL}
};

static alsa_properties_t mixerPropIn[] = {
    {AudioSystem::DEVICE_IN_BUILTIN_MIC, "alsa.mixer.builtinMic", "Mic1", NULL},
    {AudioSystem::DEVICE_IN_WIRED_HEADSET, "alsa.mixer.headsetMic", "Mic1", NULL}, //FIXME: Mic1 is adjusted whith Headset? Is there AMic? It looks like should be adjusted AMic.
    {static_cast<AudioSystem::audio_devices>(0), "alsa.mixer.", NULL, NULL}
};


struct mixer_info_t
{
    mixer_info_t() :
        elem(0),
        min(SND_MIXER_VOL_RANGE_MIN),
        max(SND_MIXER_VOL_RANGE_MAX),
        volume(0),
        mute(false)
    {
        name[0] = '\0';
    }

    snd_mixer_elem_t *elem;
    long              min;
    long              max;
    long              volume;
    bool              mute;
    char              name[ALSA_NAME_MAX];
};

static int initMixer (snd_mixer_t **mixer, const char *name)
{
    int err;
    int card = 0;
    char str[PROPERTY_VALUE_MAX];

    if ((err = snd_mixer_open(mixer, 0)) < 0) {
        LOGE("Unable to open mixer: %s", snd_strerror(err));
        return err;
    }

    if ((err = snd_mixer_attach(*mixer, name)) < 0) {
        LOGW("Unable to attach mixer to device %s: %s",
             name, snd_strerror(err));

        property_get ("alsa.mixer.defaultCard",
                      str,
                      MEDFIELDAUDIO);
        err = snd_card_get_index(str);
        if (err < 0) {
             LOGE("Get card index error:%s\n", snd_strerror(err));
             return err;
        }
        card = err;
        sprintf(str, "hw:CARD=%d", card);
        LOGV("card: %s", str);

        if ((err = snd_mixer_attach(*mixer, str)) < 0) {
            LOGE("Unable to attach mixer to device default: %s",
                 snd_strerror(err));

            snd_mixer_close (*mixer);
            *mixer = NULL;
            return err;
        }
    }

    if ((err = snd_mixer_selem_register(*mixer, NULL, NULL)) < 0) {
        LOGE("Unable to register mixer elements: %s", snd_strerror(err));
        snd_mixer_close (*mixer);
        *mixer = NULL;
        return err;
    }

    // Get the mixer controls from the kernel
    if ((err = snd_mixer_load(*mixer)) < 0) {
        LOGE("Unable to load mixer elements: %s", snd_strerror(err));
        snd_mixer_close (*mixer);
        *mixer = NULL;
        return err;
    }

    return 0;
}

typedef int (*hasVolume_t)(snd_mixer_elem_t*);

static const hasVolume_t hasVolume[] = {
    snd_mixer_selem_has_playback_volume,
    snd_mixer_selem_has_capture_volume
};

typedef int (*getVolumeRange_t)(snd_mixer_elem_t*, long int*, long int*);

static const getVolumeRange_t getVolumeRange[] = {
    snd_mixer_selem_get_playback_volume_range,
    snd_mixer_selem_get_capture_volume_range
};

typedef int (*setVolume_t)(snd_mixer_elem_t*, long int);

static const setVolume_t setVol[] = {
    snd_mixer_selem_set_playback_volume_all,
    snd_mixer_selem_set_capture_volume_all
};

ALSAMixer::ALSAMixer(AudioHardwareALSA *hardwareAlsa)
{
    int err;

    if (hardwareAlsa)
        mHardwareAlsa = hardwareAlsa;
    else
        mHardwareAlsa = NULL;

    initMixer (&mMixer[SND_PCM_STREAM_PLAYBACK], "AndroidPlayback");
    initMixer (&mMixer[SND_PCM_STREAM_CAPTURE], "AndroidRecord");

    snd_mixer_selem_id_t *sid;
    snd_mixer_selem_id_alloca(&sid);

    for (int i = 0; mixerPropOut[i].device; i++) {

        mixer_info_t *info = mixerPropOut[i].mInfo = new mixer_info_t;

        property_get (mixerPropOut[i].propName,
                      info->name,
                      mixerPropOut[i].propDefault);

        for (snd_mixer_elem_t *elem = snd_mixer_first_elem(mMixer[SND_PCM_STREAM_PLAYBACK]);
                elem;
                elem = snd_mixer_elem_next(elem)) {

            if (!snd_mixer_selem_is_active(elem))
                continue;

            snd_mixer_selem_get_id(elem, sid);

            // Find PCM playback volume control element.
            const char *elementName = snd_mixer_selem_id_get_name(sid);

            if (info->elem == NULL &&
                    strcmp(elementName, info->name) == 0 &&
                    hasVolume[SND_PCM_STREAM_PLAYBACK] (elem)) {

                info->elem = elem;
                getVolumeRange[SND_PCM_STREAM_PLAYBACK] (elem, &info->min, &info->max);
                info->volume = info->max;
                setVol[SND_PCM_STREAM_PLAYBACK] (elem, info->volume);
                if (snd_mixer_selem_has_playback_switch (elem))
                    snd_mixer_selem_set_playback_switch_all (elem, 1);
                break;
            }
        }
        LOGV("Mixer: playback route '%s' %s.", info->name, info->elem ? "found" : "not found");
    }

    for (int j = 0; mixerPropIn[j].device; j++) {

        mixer_info_t *info = mixerPropIn[j].mInfo = new mixer_info_t;

        property_get (mixerPropIn[j].propName,
                      info->name,
                      mixerPropIn[j].propDefault);

        for (snd_mixer_elem_t *elem = snd_mixer_first_elem(mMixer[SND_PCM_STREAM_CAPTURE]);
                elem;
                elem = snd_mixer_elem_next(elem)) {

            if (!snd_mixer_selem_is_active(elem))
                continue;

            snd_mixer_selem_get_id(elem, sid);

            // Find PCM playback volume control element.
            const char *elementName = snd_mixer_selem_id_get_name(sid);

            if (info->elem == NULL &&
                    strcmp(elementName, info->name) == 0 &&
                    hasVolume[SND_PCM_STREAM_CAPTURE] (elem)) {

                info->elem = elem;
                getVolumeRange[SND_PCM_STREAM_CAPTURE] (elem, &info->min, &info->max);
                info->volume = info->max;
                setVol[SND_PCM_STREAM_CAPTURE] (elem, info->volume);
                break;
            }
        }
        LOGV("Mixer: capture route '%s' %s.", info->name, info->elem ? "found" : "not found");
    }
    LOGV("mixer initialized.");
}

ALSAMixer::~ALSAMixer()
{
    for (int i = 0; i <= SND_PCM_STREAM_LAST; i++) {
        if (mMixer[i]) snd_mixer_close (mMixer[i]);
    }

    for (int j = 0; mixerPropOut[j].device; j++) {
        if (mixerPropOut[j].mInfo) {
            delete mixerPropOut[j].mInfo;
            mixerPropOut[j].mInfo = NULL;
        }
    }
    for (int k = 0; mixerPropIn[k].device; k++) {
        if (mixerPropIn[k].mInfo) {
            delete mixerPropIn[k].mInfo;
            mixerPropIn[k].mInfo = NULL;
        }
    }

    LOGV("mixer destroyed.");
}

status_t ALSAMixer::setMasterVolume(float volume)
{
    status_t err = NO_ERROR;

    if (mHardwareAlsa && mHardwareAlsa->mlpedevice && mHardwareAlsa->mlpedevice->lpeSetMasterVolume)
        err = mHardwareAlsa->mlpedevice->lpeSetMasterVolume(volume);
    return err;
}

status_t ALSAMixer::setMasterGain(float gain)
{
    status_t err = NO_ERROR;

    if (mHardwareAlsa && mHardwareAlsa->mlpedevice && mHardwareAlsa->mlpedevice->lpeSetMasterGain)
        err = mHardwareAlsa->mlpedevice->lpeSetMasterGain(gain);
    return err;
}

status_t ALSAMixer::setVolume(uint32_t device, float left, float right)
{
    for (int j = 0; mixerPropOut[j].device; j++)
        if (mixerPropOut[j].device & device) {

            mixer_info_t *info = mixerPropOut[j].mInfo;
            if (!info || !info->elem) return INVALID_OPERATION;

            long minVol = info->min;
            long maxVol = info->max;

            // Make sure volume is between bounds.
            long vol = minVol + left * (maxVol - minVol);
            if (vol > maxVol) vol = maxVol;
            if (vol < minVol) vol = minVol;

            info->volume = vol;
            snd_mixer_selem_set_playback_volume_all (info->elem, vol);
        }

    return NO_ERROR;
}

status_t ALSAMixer::setGain(uint32_t device, float gain)
{
    for (int j = 0; mixerPropIn[j].device; j++)
        if (mixerPropIn[j].device & device) {

            mixer_info_t *info = mixerPropIn[j].mInfo;
            if (!info || !info->elem) return INVALID_OPERATION;

            long minVol = info->min;
            long maxVol = info->max;

            // Make sure volume is between bounds.
            long vol = minVol + gain * (maxVol - minVol);
            if (vol > maxVol) vol = maxVol;
            if (vol < minVol) vol = minVol;

            info->volume = vol;
            snd_mixer_selem_set_capture_volume_all (info->elem, vol);
        }

    return NO_ERROR;
}

status_t ALSAMixer::setCaptureMuteState(uint32_t device, bool state)
{
    for (int j = 0; mixerPropIn[j].device; j++)
        if (mixerPropIn[j].device & device) {

            mixer_info_t *info = mixerPropIn[j].mInfo;
            if (!info || !info->elem) return INVALID_OPERATION;

            if (snd_mixer_selem_has_capture_switch (info->elem)) {

                int err = snd_mixer_selem_set_capture_switch_all (info->elem, static_cast<int>(!state));
                if (err < 0) {
                    LOGE("Unable to %s capture mixer switch %s",
                         state ? "enable" : "disable", info->name);
                    return INVALID_OPERATION;
                }
            }

            info->mute = state;
        }

    return NO_ERROR;
}

status_t ALSAMixer::getCaptureMuteState(uint32_t device, bool *state)
{
    if (!state) return BAD_VALUE;

    for (int j = 0; mixerPropIn[j].device; j++)
        if (mixerPropIn[j].device & device) {

            mixer_info_t *info = mixerPropIn[j].mInfo;
            if (!info || !info->elem) return INVALID_OPERATION;

            *state = info->mute;
            return NO_ERROR;
        }

    return BAD_VALUE;
}

status_t ALSAMixer::setPlaybackMuteState(uint32_t device, bool state)
{
    for (int j = 0; mixerPropOut[j].device; j++)
        if (mixerPropOut[j].device & device) {

            mixer_info_t *info = mixerPropOut[j].mInfo;
            if (!info || !info->elem) return INVALID_OPERATION;

            if (snd_mixer_selem_has_playback_switch (info->elem)) {

                int err = snd_mixer_selem_set_playback_switch_all (info->elem, static_cast<int>(!state));
                if (err < 0) {
                    LOGE("Unable to %s playback mixer switch %s",
                         state ? "enable" : "disable", info->name);
                    return INVALID_OPERATION;
                }
            }

            info->mute = state;
        }

    return NO_ERROR;
}

status_t ALSAMixer::getPlaybackMuteState(uint32_t device, bool *state)
{
    if (!state) return BAD_VALUE;

    for (int j = 0; mixerPropOut[j].device; j++)
        if (mixerPropOut[j].device & device) {

            mixer_info_t *info = mixerPropOut[j].mInfo;
            if (!info || !info->elem) return INVALID_OPERATION;

            *state = info->mute;
            return NO_ERROR;
        }

    return BAD_VALUE;
}

};        // namespace android
