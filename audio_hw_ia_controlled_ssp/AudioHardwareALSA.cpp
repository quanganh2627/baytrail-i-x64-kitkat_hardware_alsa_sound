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
//#define LOG_NDEBUG 0
#include <utils/Log.h>
#include <utils/String8.h>

#include <media/AudioRecord.h>
#include <hardware_legacy/power.h>
#include "Property.h"
#include "ParameterMgrPlatformConnector.h"
#include "SelectionCriterionInterface.h"
#include "AudioHardwareALSA.h"
#include "AudioStreamInALSA.h"
#include "AudioStreamOutALSA.h"
#include "ALSAStreamOps.h"

#include "AudioRouteManager.h"
#include "AudioAutoRoutingLock.h"
#include "AudioConversion.h"

#define DEFAULTGAIN "1.0"

#define VOICE_GAIN_MAX      (88)
#define VOICE_GAIN_MIN      (40)
#define VOICE_GAIN_OFFSET   (40)
#define VOICE_GAIN_SLOPE    (48)

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

// HAL modules table
const AudioHardwareALSA::hw_module AudioHardwareALSA::hw_module_list [AudioHardwareALSA::NB_HW_DEV]= {
    { TINYALSA_HARDWARE_MODULE_ID, TINYALSA_HARDWARE_NAME },
    { FM_HARDWARE_MODULE_ID, FM_HARDWARE_NAME },
};

/// Android Properties

const char* const AudioHardwareALSA::mFmSupportedPropName = "Audiocomms.FM.Supported";
const bool AudioHardwareALSA::mFmSupportedDefaultValue = false;

const char* const AudioHardwareALSA::mFmIsAnalogPropName = "Audiocomms.FM.IsAnalog";
const bool AudioHardwareALSA::mFmIsAnalogDefaultValue = false;

// ----------------------------------------------------------------------------

static void ALSAErrorHandler(const char *file,
                             int line,
                             const char *function,
                             int err,
                             const char *fmt,
                             ...)
{
    char buf[BUFSIZ];
    va_list arg;
    int l;

    va_start(arg, fmt);
    l = snprintf(buf, BUFSIZ, "%s:%i:(%s) ", file, line, function);
    if (l < 0) {

        ALOGE("%s: error while formating the log", __FUNCTION__);
        // Bailing out
        goto error;
    }
    if (l >= BUFSIZ) {

        // return of snprintf higher than size -> the output has been truncated
        ALOGE("%s: log truncated", __FUNCTION__);
        l = BUFSIZ - 1;
    }
    vsnprintf(buf + l, BUFSIZ - l, fmt, arg);
    buf[BUFSIZ-1] = '\0';
    ALOG(LOG_ERROR, "ALSALib", "%s", buf);

error:
    va_end(arg);
}

AudioHardwareInterface *AudioHardwareALSA::create() {
    return new AudioHardwareALSA();
}

AudioHardwareALSA::AudioHardwareALSA() :
    mRouteMgr(new CAudioRouteManager(this))
{
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
    if (getAlsaHwDevice()) {

        getAlsaHwDevice()->init(getAlsaHwDevice());
    }

    bool bFmSupported = TProperty<bool>(mFmSupportedPropName, mFmSupportedDefaultValue);
    bool bFmIsAnalog = TProperty<bool>(mFmIsAnalogPropName, mFmIsAnalogDefaultValue);
    if (bFmSupported) {
        if (!bFmIsAnalog) {
            if (getFmHwDevice()) {
                getFmHwDevice()->init();
            } else {
                ALOGE("Cannot load FM HW Module");
            }
        }
    }

    // Start the route manager service
    if (mRouteMgr->start() != NO_ERROR) {

        ALOGE("%s: could not start route manager, NO ROUTING AVAILABLE!!!", __FUNCTION__);
    }
}

alsa_device_t* AudioHardwareALSA::getAlsaHwDevice() const
{
    assert(mHwDeviceArray.size() > ALSA_HW_DEV);

    return (alsa_device_t *)mHwDeviceArray[ALSA_HW_DEV];
}

fm_device_t* AudioHardwareALSA::getFmHwDevice() const
{
    assert(mHwDeviceArray.size() > FM_HW_DEV);

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

    if (getAlsaHwDevice() && mRouteMgr->isStarted())

        return NO_ERROR;
    else
        return NO_INIT;
}

status_t AudioHardwareALSA::setVoiceVolume(float volume)
{
    // The voice volume is used by the VOICE_CALL audio stream.
    CAudioAutoRoutingLock lock(this);

    int gain = 0;
    int range = VOICE_GAIN_SLOPE;

    gain = volume * range + VOICE_GAIN_OFFSET;
    gain = (gain >= VOICE_GAIN_MAX) ? VOICE_GAIN_MAX : gain;
    gain = (gain <= VOICE_GAIN_MIN) ? VOICE_GAIN_MIN : gain;

    return mRouteMgr->setVoiceVolume(gain);
}

status_t AudioHardwareALSA::setFmRxVolume(float volume)
{
    ALOGD("%s", __FUNCTION__);

    mRouteMgr->setFmRxVolume(volume);

    return NO_ERROR;
}

status_t AudioHardwareALSA::setMasterVolume(float volume)
{
    CAudioAutoRoutingLock lock(this);

    ALOGW("%s: missing implementation", __FUNCTION__);

    return NO_ERROR;
}

status_t AudioHardwareALSA::setFmRxMode(int fm_mode)
{
    ALOGD("%s: in", __FUNCTION__);

    if (AudioHardwareBase::setFmRxMode(fm_mode) != ALREADY_EXISTS) {

        mRouteMgr->setFmRxMode(fm_mode);
    }

    return NO_ERROR;
}

AudioStreamOut *
AudioHardwareALSA::openOutputStream(uint32_t devices,
                                    int *format,
                                    uint32_t *channels,
                                    uint32_t *sampleRate,
                                    status_t *status)
{
    ALOGD("%s: called for devices: 0x%08x", __FUNCTION__, devices);

    status_t err = BAD_VALUE;
    AudioStreamOutALSA* out = NULL;

    if ((devices & (devices - 1)) || (!(devices & AudioSystem::DEVICE_OUT_ALL))) {

        ALOGD("%s: called with bad devices", __FUNCTION__);
        if (status) *status = err;
        return out;
    }

    if (!getAlsaHwDevice()) {

        ALOGE("%s: Open error, alsa hw device not valid", __FUNCTION__);
        err = DEAD_OBJECT;
        if (status) *status = DEAD_OBJECT;
        return out;
    }

    out = new AudioStreamOutALSA(this);

    err = out->set(format, channels, sampleRate);
    if (err != NO_ERROR) {

        ALOGE("%s: set error.", __FUNCTION__);
        delete out;
        out = NULL;
    } else {

        // Informs the route manager of stream creation
        mRouteMgr->addStream(out);
    }

    if (status) *status = err;

    ALOGD("%s: OUT", __FUNCTION__);
    return out;
}

void
AudioHardwareALSA::closeOutputStream(AudioStreamOut* out)
{
    // Informs the route manager of stream destruction
    mRouteMgr->removeStream((AudioStreamOutALSA* )out);

    delete out;
    out = NULL;
}

AudioStreamIn *
AudioHardwareALSA::openInputStream(uint32_t devices,
                                   int *format,
                                   uint32_t *channels,
                                   uint32_t *sampleRate,
                                   status_t *status,
                                   AudioSystem::audio_in_acoustics acoustics)
{
    ALOGD("%s: IN", __FUNCTION__);

    status_t err = BAD_VALUE;
    AudioStreamInALSA* in = NULL;

    mMicMuteState = false;

    if ((devices & (devices - 1)) || (!(devices & AudioSystem::DEVICE_IN_ALL))) {

        if (status) *status = err;
        return in;
    }

    if (!getAlsaHwDevice()) {

        ALOGE("%s: Open error, alsa hw device not valid", __FUNCTION__);
        if (status) *status = DEAD_OBJECT;
        return in;
    }

    in = new AudioStreamInALSA(this, acoustics);

    err = in->set(format, channels, sampleRate);
    if (err != NO_ERROR) {

        ALOGE("%s: Set err", __FUNCTION__);
        delete in;
        in = NULL;
    } else {

        // Informs the route manager of stream creation
        mRouteMgr->addStream(in);
    }

    if (status) *status = err;

    ALOGD("%s: OUT", __FUNCTION__);
    return in;
}

void
AudioHardwareALSA::closeInputStream(AudioStreamIn* in)
{
    mMicMuteState = false;
    // Informs the route manager of stream destruction
    mRouteMgr->removeStream((AudioStreamInALSA* )in);

    delete in;
    in = NULL;
}

status_t AudioHardwareALSA::setMicMute(bool state)
{
    mMicMuteState = state;
    if(state)
        ALOGD("Set MUTE true");
    else
        ALOGD("Set MUTE false");

    return NO_ERROR;
}

status_t AudioHardwareALSA::getMicMute(bool *state)
{
    *state = mMicMuteState;
    if(*state)
        ALOGD("Get MUTE true");
    else
        ALOGD("Get MUTE false");

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

status_t AudioHardwareALSA::dump(int fd, const Vector<String16>& args)
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
    return mRouteMgr->startStream(pStream);
}

status_t AudioHardwareALSA::stopStream(ALSAStreamOps* pStream)
{
    return mRouteMgr->stopStream(pStream);
}

}       // namespace android
