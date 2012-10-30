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
#include "AudioPlatformHardware.h"

#include "AudioRouteManager.h"
#include "AudioRoute.h"
#include "AudioAutoRoutingLock.h"
#include "AudioConversion.h"

#define DEFAULTGAIN "1.0"

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

const char* AudioHardwareALSA::gpcVoiceVolume = "/Audio/IMC/SOUND_CARD/PORTS/I2S1/TX/VOLUME/LEVEL"; // Type = unsigned integer

// Default sampling rate in case the value is not found in xml file
const uint32_t AudioHardwareALSA::DEFAULT_SAMPLE_RATE = 48000;
const uint32_t AudioHardwareALSA::DEFAULT_CHANNEL_COUNT = 2;
const uint32_t AudioHardwareALSA::DEFAULT_FORMAT = AUDIO_FORMAT_PCM_16_BIT;

// Default clock selection
const uint32_t AudioHardwareALSA::DEFAULT_IFX_CLK_SELECT = -1;

// HAL modules table
const AudioHardwareALSA::hw_module AudioHardwareALSA::hw_module_list [AudioHardwareALSA::NB_HW_DEV]= {
    { TINYALSA_HARDWARE_MODULE_ID, TINYALSA_HARDWARE_NAME },
//    { ACOUSTICS_HARDWARE_MODULE_ID, ACOUSTICS_HARDWARE_NAME },
//    { FM_HARDWARE_MODULE_ID, FM_HARDWARE_MODULE_ID },
};

/// Android Properties

// Property name indicating if platform embeds a modem chip
const char* const AudioHardwareALSA::mModemEmbeddedPropName = "Audiocomms.Modem.IsPresent";
const bool AudioHardwareALSA::mModemEmbeddedDefaultValue = true;

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
    /// Creates the routes and adds them to the route mgr
    for (int rte = 0; rte < NB_ROUTE; rte++) {

        mRouteMgr->addRoute(audio_routes[rte]);
    }

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

#ifdef WITH_FM_SUPPORT
    if (getFmHwDevice()) {
        getFmHwDevice()->init();
    } else {
        LOGE("Cannot load FM HW Module");
    }
#endif

    // Start the route manager service
    mRouteMgr->start();
}

alsa_device_t* AudioHardwareALSA::getAlsaHwDevice() const
{
    assert(mHwDeviceArray.size() > ALSA_HW_DEV);

    return (alsa_device_t *)mHwDeviceArray[ALSA_HW_DEV];
}

fm_device_t* AudioHardwareALSA::getFmHwDevice() const
{
//    assert(mHwDeviceArray.size() > FM_HW_DEV);

    return NULL; //(fm_device_t *)mHwDeviceArray[FM_HW_DEV];
}

acoustic_device_t* AudioHardwareALSA::getAcousticHwDevice() const
{
//    assert(mHwDeviceArray.size() > ACOUSTIC_HW_DEV);

    return NULL; //(acoustic_device_t *)mHwDeviceArray[ACOUSTIC_HW_DEV];
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

    if (getAlsaHwDevice())

        return NO_ERROR;
    else
        return NO_INIT;
}


// Default Alsa sample rate discovery
uint32_t AudioHardwareALSA::getIntegerParameterValue(const string& strParameterPath, bool bSigned, uint32_t uiDefaultValue) const
{
    ALOGD("%s in", __FUNCTION__);

    CParameterMgrPlatformConnector* pParameterMgrPlatformConnector = mRouteMgr->getParameterMgrPlatformConnector();

    if (!pParameterMgrPlatformConnector->isStarted()) {

        return uiDefaultValue;
    }

    string strError;
    // Get handle
    CParameterHandle* pParameterHandle = pParameterMgrPlatformConnector->createParameterHandle(strParameterPath, strError);

    if (!pParameterHandle) {

        strError = strParameterPath.c_str();
        strError += " not found!";

        ALOGE("Unable to get parameter handle: %s", strError.c_str());

        ALOGD("%s returning %d", __FUNCTION__, uiDefaultValue);

        return uiDefaultValue;
    }

    // Retrieve value
    uint32_t uiValue;

    if ((!bSigned && !pParameterHandle->getAsInteger(uiValue, strError)) || (bSigned && !pParameterHandle->getAsSignedInteger((int32_t&)uiValue, strError))) {

        ALOGE("Unable to get value: %s, from parameter path: %s", strError.c_str(), strParameterPath.c_str());

        ALOGD("%s returning %d", __FUNCTION__, uiDefaultValue);

        // Remove handle
        delete pParameterHandle;

        return uiDefaultValue;
    }

    // Remove handle
    delete pParameterHandle;

    ALOGD("%s returning %d", __FUNCTION__, uiValue);

    return uiValue;
}

status_t AudioHardwareALSA::setVoiceVolume(float volume)
{
    // The voice volume is used by the VOICE_CALL audio stream.
    CAudioAutoRoutingLock lock(this);

    int gain = 0;
    int range = 48;

    gain = volume * range + 40;
    gain = (gain >= 88) ? 88 : gain;
    gain = (gain <= 40) ? 40 : gain;

    return mRouteMgr->setIntegerParameterValue(gpcVoiceVolume, false, gain);
}

status_t AudioHardwareALSA::setFmRxVolume(float volume)
{
    return NO_ERROR;
}

status_t AudioHardwareALSA::setMasterVolume(float volume)
{
    CAudioAutoRoutingLock lock(this);

    // Through PFW ?

    return INVALID_OPERATION;
}

status_t AudioHardwareALSA::setFmRxMode(int fm_mode)
{
    CAudioAutoRoutingLock lock(this);
//    AutoW lock(mLock);

    LOGD("%s: in", __FUNCTION__);

    if (AudioHardwareBase::setFmRxMode(fm_mode) != ALREADY_EXISTS) {

        mRouteMgr->reconsiderRouting();

        getFmHwDevice()->set_state(fm_mode);
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
        goto finish;
    }

    if (!getAlsaHwDevice()) {

        ALOGE("%s: Open error, alsa hw device not valid", __FUNCTION__);
        err = DEAD_OBJECT;
        goto finish;
    }

    out = new AudioStreamOutALSA(this);

    err = out->set(format, channels, sampleRate);
    if (err) {

        ALOGE("%s: set error.", __FUNCTION__);
        goto finish;
    }

    // Informs the route manager of stream creation
    mRouteMgr->addStream(out);

finish:
    if (err) {

        delete out;
        out = NULL;
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
    float fDefaultGain = 0.0;

    mMicMuteState = false;
//    fDefaultGain = TProperty<float>(mDefaultGainPropName, mDefaultGainValue);

    if ((devices & (devices - 1)) || (!(devices & AudioSystem::DEVICE_IN_ALL))) {

        goto finish;
    }

    if (!getAlsaHwDevice()) {

        ALOGE("%s: Open error, alsa hw device not valid", __FUNCTION__);
        err = DEAD_OBJECT;
        goto finish;
    }

    in = new AudioStreamInALSA(this, acoustics);

//    err = in->setGain(fDefaultGain);
//    if (err != NO_ERROR) {

//        ALOGE("%s: setGain", __FUNCTION__);
//        goto finish;
//    }

    err = in->set(format, channels, sampleRate);
    if (err != NO_ERROR) {

        ALOGE("%s: Set err", __FUNCTION__);
        goto finish;
    }

    // Informs the route manager of stream creation
    mRouteMgr->addStream(in);

finish:
    if (err) {

        delete in;
        in = NULL;
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
