/* AudioHardwareALSA.cpp
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
#include <string>
#include <cutils/properties.h>

#define LOG_TAG "AudioHardwareALSA"

#include <utils/Log.h>
#include <utils/String8.h>

#ifdef USE_FRAMEWORK_GTI
#include "gtiservice/GtiService.h"
#endif

#include <media/AudioRecord.h>
#include <hardware_legacy/power.h>
#include "Property.h"
#include "ParameterMgrPlatformConnector.h"
#include "SelectionCriterionInterface.h"
#include "AudioHardwareALSA.h"
#include "AudioStreamInALSA.h"
#include "AudioStreamOutALSA.h"
#include "ALSAStreamOps.h"
#include "Utils.h"

#include "AudioRouteManager.h"
#include "AudioAutoRoutingLock.h"
#include "AudioConversion.h"

#define DEFAULTGAIN "1.0"

using namespace std;
using namespace android;

namespace android_audio_legacy
{
extern "C"
{
//
// Function for dlsym() to look up for creating a new AudioHardwareInterface.
//
AudioHardwareInterface *createAudioHardware(void) {
    return AudioHardwareALSA::create();
}
}         // extern "C"

// Default sampling rate in case the value is not found in xml file
const uint32_t AudioHardwareALSA::DEFAULT_SAMPLE_RATE = 48000;
const uint32_t AudioHardwareALSA::DEFAULT_CHANNEL_COUNT = 2;
const uint32_t AudioHardwareALSA::DEFAULT_FORMAT = AUDIO_FORMAT_PCM_16_BIT;

const int32_t AudioHardwareALSA::VOICE_GAIN_MAX      = 88;
const int32_t AudioHardwareALSA::VOICE_GAIN_MIN      = 40;
const uint32_t AudioHardwareALSA::VOICE_GAIN_OFFSET   = 40;
const uint32_t AudioHardwareALSA::VOICE_GAIN_SLOPE    = 48;

// HAL modules table
const AudioHardwareALSA::hw_module AudioHardwareALSA::hw_module_list [AudioHardwareALSA::NB_HW_DEV] = {
    { FM_HARDWARE_MODULE_ID, FM_HARDWARE_NAME },
};

/// Android Properties

const char* const AudioHardwareALSA::FM_SUPPORTED_PROP_NAME = "Audiocomms.FM.Supported";
const bool AudioHardwareALSA::FM_SUPPORTED_PROP_DEFAULT_VALUE = false;

const char* const AudioHardwareALSA::FM_IS_ANALOG_PROP_NAME = "Audiocomms.FM.IsAnalog";
const bool AudioHardwareALSA::FM_IS_ANALOG_DEFAUT_VALUE = false;


AudioHardwareInterface *AudioHardwareALSA::create() {

    ALOGD("Using Audio HAL Configurable");

    return new AudioHardwareALSA();
}

AudioHardwareALSA::AudioHardwareALSA() :
    mRouteMgr(new CAudioRouteManager(this))
{
#ifdef USE_FRAMEWORK_GTI
    GtiService::Start();
#endif

    // HW Modules initialisation
    hw_module_t* module;
    hw_device_t* device;

    for (int i = 0; i < NB_HW_DEV; i++)
    {
        if (hw_get_module(hw_module_list[i].module_id, (hw_module_t const**)&module))
        {
            ALOGE("%s Module not found!!!", hw_module_list[i].module_id);
            mHwDeviceArray.push_back(NULL);
        }
        else if (module->methods->open(module, hw_module_list[i].module_name, &device))
        {
            ALOGE("%s Module could not be opened!!!", hw_module_list[i].module_name);
            mHwDeviceArray.push_back(NULL);
        }
        else {

            mHwDeviceArray.push_back(device);
        }
    }

    bool bFmSupported = TProperty<bool>(FM_SUPPORTED_PROP_NAME, FM_SUPPORTED_PROP_DEFAULT_VALUE);
    bool bFmIsAnalog = TProperty<bool>(FM_IS_ANALOG_PROP_NAME, FM_IS_ANALOG_DEFAUT_VALUE);
    if (bFmSupported && !bFmIsAnalog) {

        if (getFmHwDevice()) {

            getFmHwDevice()->init();
        } else {

            ALOGE("Cannot load FM HW Module");
        }
    }

    // Start the route manager service
    if (mRouteMgr->start() != NO_ERROR) {

        ALOGE("%s: could not start route manager, NO ROUTING AVAILABLE", __FUNCTION__);
    }
}

fm_device_t* AudioHardwareALSA::getFmHwDevice()
{
    LOG_ALWAYS_FATAL_IF(mHwDeviceArray.size() <= FM_HW_DEV);

    return (fm_device_t *)mHwDeviceArray[FM_HW_DEV];
}

AudioHardwareALSA::~AudioHardwareALSA()
{
    // Delete route manager, it will detroy all the registered routes
    delete mRouteMgr;

    for (int i = 0; i < NB_HW_DEV; i++) {

        if (mHwDeviceArray[i]) {

            mHwDeviceArray[i]->close(mHwDeviceArray[i]);
        }

    }
}

status_t AudioHardwareALSA::initCheck()
{

    if (mRouteMgr->isStarted())

        return NO_ERROR;
    else
        return NO_INIT;
}

status_t AudioHardwareALSA::setVoiceVolume(float volume)
{
    // The voice volume is used by the VOICE_CALL audio stream.
    CAudioAutoRoutingLock lock(this);

    int gain;
    int range = VOICE_GAIN_SLOPE;

    gain = volume * range + VOICE_GAIN_OFFSET;
    gain = min(gain, VOICE_GAIN_MAX);
    gain = max(gain, VOICE_GAIN_MIN);

    return mRouteMgr->setVoiceVolume(gain);
}

status_t AudioHardwareALSA::setFmRxVolume(float volume)
{
    ALOGD("%s", __FUNCTION__);

    return mRouteMgr->setFmRxVolume(volume);
}

status_t AudioHardwareALSA::setMasterVolume(float __UNUSED volume)
{
    ALOGW("%s: missing implementation", __FUNCTION__);
    return NO_ERROR;
}

status_t AudioHardwareALSA::setFmRxMode(int fm_mode)
{
    ALOGD("%s: in", __FUNCTION__);

    if (AudioHardwareBase::setFmRxMode(fm_mode) != ALREADY_EXISTS) {

        return mRouteMgr->setFmRxMode(fm_mode);
    }

    return NO_ERROR;
}

AudioStreamOut* AudioHardwareALSA::openOutputStream(uint32_t devices,
                                    int *format,
                                    uint32_t *channels,
                                    uint32_t *sampleRate,
                                    status_t *status)
{
    ALOGD("%s: called for devices: 0x%08x", __FUNCTION__, devices);

    LOG_ALWAYS_FATAL_IF(status == NULL);

    status_t &err = *status;

    if (!audio_is_output_device(devices)) {

        ALOGD("%s: called with bad devices", __FUNCTION__);
        err = BAD_VALUE;
        return NULL;
    }

    AudioStreamOutALSA* out = new AudioStreamOutALSA(this);

    err = out->set(format, channels, sampleRate);
    if (err != NO_ERROR) {

        ALOGE("%s: set error.", __FUNCTION__);
        delete out;
        return NULL;
    }

    // Informs the route manager of stream creation
    mRouteMgr->addStream(out);

    ALOGD("%s: output created with status=%d", __FUNCTION__, err);
    return out;
}

void AudioHardwareALSA::closeOutputStream(AudioStreamOut* out)
{
    // Informs the route manager of stream destruction
    mRouteMgr->removeStream((AudioStreamOutALSA* )out);

    delete out;
}

AudioStreamIn* AudioHardwareALSA::openInputStream(uint32_t devices,
                                   int *format,
                                   uint32_t *channels,
                                   uint32_t *sampleRate,
                                   status_t *status,
                                   AudioSystem::audio_in_acoustics acoustics)
{
    ALOGD("%s: IN", __FUNCTION__);

    LOG_ALWAYS_FATAL_IF(status == NULL);

    status_t &err = *status;

    mMicMuteState = false;

    if (!audio_is_input_device(devices)) {

        err = BAD_VALUE;
        return NULL;
    }

    AudioStreamInALSA* in = new AudioStreamInALSA(this, acoustics);

    err = in->set(format, channels, sampleRate);
    if (err != NO_ERROR) {

        ALOGE("%s: Set err", __FUNCTION__);
        delete in;
        return NULL;
    }
    // Informs the route manager of stream creation
    mRouteMgr->addStream(in);

    ALOGD("%s: OUT status=%d", __FUNCTION__, err);
    return in;
}

void AudioHardwareALSA::closeInputStream(AudioStreamIn* in)
{
    mMicMuteState = false;
    // Informs the route manager of stream destruction
    mRouteMgr->removeStream((AudioStreamInALSA* )in);

    delete in;
}

status_t AudioHardwareALSA::setMicMute(bool state)
{
    mMicMuteState = state;

    ALOGD("Set MUTE %s", state? "true" : "false");

    return NO_ERROR;
}

status_t AudioHardwareALSA::getMicMute(bool *state)
{
    if (state != NULL) {

        ALOGD("Get MUTE %s", *state? "true" : "false");
        *state = mMicMuteState;
    }

    return NO_ERROR;

}

size_t AudioHardwareALSA::getInputBufferSize(uint32_t sampleRate, int format, int channelCount)
{
    switch (sampleRate) {
    case 8000:
    case 11025:
    case 12000:
    case 16000:
    case 22050:
    case 24000:
    case 32000:
    case 44100:
    case 48000:
        break;
    default:
        ALOGW("getInputBufferSize bad sampling rate: %d", sampleRate);
        return 0;
    }
    if (format != AudioSystem::PCM_16_BIT) {
        ALOGW("getInputBufferSize bad format: %d", format);
        return 0;
    }
    if ((channelCount < 1) || (channelCount > 2)) {
        ALOGW("getInputBufferSize bad channel count: %d", channelCount);
        return 0;
    }

    // Returns 20ms buffer size per channel
    return (sampleRate / 25) * channelCount;
}

status_t AudioHardwareALSA::dump(int __UNUSED fd, const Vector<String16> __UNUSED &args)
{
    return NO_ERROR;
}

status_t AudioHardwareALSA::setParameters(const String8& keyValuePairs)
{
    ALOGD("%s", __FUNCTION__);
    return mRouteMgr->setParameters(keyValuePairs);
}


//
// Direct Output Stream to be identified by new KeyValuePair
//
// set Stream Parameters
status_t AudioHardwareALSA::setStreamParameters(ALSAStreamOps* pStream, const String8& keyValuePairs)
{
    ALOGD("%s: key value pair %s", __FUNCTION__, keyValuePairs.string());
    return mRouteMgr->setStreamParameters(pStream, keyValuePairs, mode());
}

// Lock the routing
void AudioHardwareALSA::lockRouting()
{
    mRouteMgr->lock();
}

// Unlock the routing
void AudioHardwareALSA::unlockRouting()
{
    mRouteMgr->unlock();
}

status_t AudioHardwareALSA::startStream(ALSAStreamOps* pStream)
{
    bool bIsStreamOut;
    {
        CAudioAutoRoutingLock lock(this);
        if (pStream->isStarted()) {

            return OK;
        }

        // Start stream
        pStream->setStarted(true);
        bIsStreamOut = pStream->isOut();
    }
    return mRouteMgr->startStream(bIsStreamOut);
}

status_t AudioHardwareALSA::stopStream(ALSAStreamOps* pStream)
{
    bool bIsStreamOut;
    {
        CAudioAutoRoutingLock lock(this);
        if (!pStream->isStarted()) {

            return OK;
        }

        // Stop stream
        pStream->setStarted(false);
        bIsStreamOut = pStream->isOut();
    }
    return mRouteMgr->stopStream(bIsStreamOut);
}

}       // namespace android
