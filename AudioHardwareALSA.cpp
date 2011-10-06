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

#include <cutils/properties.h>
#include <media/AudioRecord.h>
#include <hardware_legacy/power.h>
#include "ParameterMgrPlatformConnector.h"
#include "SelectionCriterionInterface.h"
#include "AudioHardwareALSA.h"

#define DEFAULTGAIN "1.0"

extern "C"
{
    //
    // Function for dlsym() to look up for creating a new AudioHardwareInterface.
    //
    android::AudioHardwareInterface *createAudioHardware(void) {
        return android::AudioHardwareALSA::create();
    }
}         // extern "C"

namespace android
{
/// PFW related definitions
// Logger
class CParameterMgrPlatformConnectorLogger : public CParameterMgrPlatformConnector::ILogger
{
public:
    CParameterMgrPlatformConnectorLogger() {}

    virtual void log(const std::string& strLog)
    {
        LOGD("%s",strLog.c_str());
    }
};

// Mode type
const AudioHardwareALSA::SSelectionCriterionTypeValuePair AudioHardwareALSA::mModeValuePairs[] = {
    { AudioSystem::MODE_NORMAL, "Normal" },
    { AudioSystem::MODE_RINGTONE, "RingTone" },
    { AudioSystem::MODE_IN_CALL, "InCsvCall" },
    { AudioSystem::MODE_IN_COMMUNICATION, "InVoipCall" }
};
const uint32_t AudioHardwareALSA::mNbModeValuePairs = sizeof(AudioHardwareALSA::mModeValuePairs)/sizeof(AudioHardwareALSA::mModeValuePairs[0]);

// Selected Input Device type
const AudioHardwareALSA::SSelectionCriterionTypeValuePair AudioHardwareALSA::mSelectedInputDeviceValuePairs[] = {
    { AudioSystem::DEVICE_IN_COMMUNICATION, "Communication" },
    { AudioSystem::DEVICE_IN_AMBIENT, "Ambient" },
    { AudioSystem::DEVICE_IN_BUILTIN_MIC, "Main" },
    { AudioSystem::DEVICE_IN_BLUETOOTH_SCO_HEADSET, "SCO_Headset" },
    { AudioSystem::DEVICE_IN_WIRED_HEADSET, "Headset" },
    { AudioSystem::DEVICE_IN_AUX_DIGITAL, "AuxDigital" },
    { AudioSystem::DEVICE_IN_VOICE_CALL, "VoiceCall" },
    { AudioSystem::DEVICE_IN_BACK_MIC, "Back" }
};
const uint32_t AudioHardwareALSA::mNbSelectedInputDeviceValuePairs = sizeof(AudioHardwareALSA::mSelectedInputDeviceValuePairs)/sizeof(AudioHardwareALSA::mSelectedInputDeviceValuePairs[0]);

// Selected Output Device type
const AudioHardwareALSA::SSelectionCriterionTypeValuePair AudioHardwareALSA::mSelectedOutputDeviceValuePairs[] = {
    { AudioSystem::DEVICE_OUT_EARPIECE, "Earpiece" },
    { AudioSystem::DEVICE_OUT_SPEAKER, "IHF" },
    { AudioSystem::DEVICE_OUT_WIRED_HEADSET, "Headset" },
    { AudioSystem::DEVICE_OUT_WIRED_HEADPHONE, "Headphones" },
    { AudioSystem::DEVICE_OUT_BLUETOOTH_SCO, "SCO" },
    { AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET, "SCO_Headset" },
    { AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT, "SCO_CarKit" },
    { AudioSystem::DEVICE_OUT_BLUETOOTH_A2DP, "A2DP" },
    { AudioSystem::DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES, "A2DP_Headphones" },
    { AudioSystem::DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER, "A2DP_Speaker" },
    { AudioSystem::DEVICE_OUT_AUX_DIGITAL, "AuxDigital" },
    { AudioSystem::DEVICE_OUT_HDMI, "HDMI" }
};
const uint32_t AudioHardwareALSA::mNbSelectedOutputDeviceValuePairs = sizeof(AudioHardwareALSA::mSelectedOutputDeviceValuePairs)/sizeof(AudioHardwareALSA::mSelectedOutputDeviceValuePairs[0]);


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
    vsnprintf(buf + l, BUFSIZ - l, fmt, arg);
    buf[BUFSIZ-1] = '\0';
    LOG(LOG_ERROR, "ALSALib", "%s", buf);
    va_end(arg);
}

AudioHardwareInterface *AudioHardwareALSA::create() {
    return new AudioHardwareALSA();
}

AudioHardwareALSA::AudioHardwareALSA() :
    mALSADevice(0),
    mAcousticDevice(0),
    mvpcdevice(0),
    mlpedevice(0),
    mParameterMgrPlatformConnector(new CParameterMgrPlatformConnector("Audio")),
    mParameterMgrPlatformConnectorLogger(new CParameterMgrPlatformConnectorLogger)
{
    // Logger
    mParameterMgrPlatformConnector->setLogger(mParameterMgrPlatformConnectorLogger);

    /// Criteria Types
    // Mode
    mModeType = mParameterMgrPlatformConnector->createSelectionCriterionType();
    fillSelectionCriterionType(mModeType, mModeValuePairs, mNbModeValuePairs);

    // InputDevice
    mInputDeviceType = mParameterMgrPlatformConnector->createSelectionCriterionType(true);
    fillSelectionCriterionType(mInputDeviceType, mSelectedInputDeviceValuePairs, mNbSelectedInputDeviceValuePairs);

    // OutputDevice
    mOutputDeviceType = mParameterMgrPlatformConnector->createSelectionCriterionType(true);
    fillSelectionCriterionType(mOutputDeviceType, mSelectedOutputDeviceValuePairs, mNbSelectedOutputDeviceValuePairs);

    /// Criteria
    mSelectedMode = mParameterMgrPlatformConnector->createSelectionCriterion("Mode", mModeType);
    mSelectedInputDevice = mParameterMgrPlatformConnector->createSelectionCriterion("SelectedInputDevice", mInputDeviceType);
    mSelectedOutputDevice = mParameterMgrPlatformConnector->createSelectionCriterion("SelectedOutputDevice", mOutputDeviceType);

    /// Start
    std::string strError;
    if (!mParameterMgrPlatformConnector->start(strError)) {

        LOGE("%s",strError.c_str());
    }

    // Reset
    snd_lib_error_set_handler(&ALSAErrorHandler);
    mMixer = new ALSAMixer(this);
#ifdef USE_INTEL_SRC
    mResampler = new AudioResamplerALSA();
#endif
    status_t err_a = BAD_VALUE;

    hw_module_t *module;
    int err = hw_get_module(ALSA_HARDWARE_MODULE_ID,
                            (hw_module_t const**)&module);

    if (err == 0) {
        hw_device_t* device;
        err = module->methods->open(module, ALSA_HARDWARE_NAME, &device);
        if (err == 0) {
            mALSADevice = (alsa_device_t *)device;
            mALSADevice->init(mALSADevice, mDeviceList);
        } else
            LOGE("ALSA Module could not be opened!!!");
    } else
        LOGE("ALSA Module not found!!!");

    err = hw_get_module(ACOUSTICS_HARDWARE_MODULE_ID,
    (hw_module_t const**)&module);
    if (err == 0) {
        hw_device_t* device;
        err = module->methods->open(module,
        ACOUSTICS_HARDWARE_NAME, &device);
        if (err == 0)
            mAcousticDevice = (acoustic_device_t *)device;
        else
            LOGE("Acoustics Module not found.");
    }

    err = hw_get_module(VPC_HARDWARE_MODULE_ID,
    (hw_module_t const**)&module);

    if (err == 0) {
        hw_device_t* device;
        err = module->methods->open(module,VPC_HARDWARE_NAME, &device);
        if (err == 0){
            LOGD("VPC MODULE OK.");
            mvpcdevice = (vpc_device_t *) device;
            err = mvpcdevice->init();
            if (err)
                LOGE("audience init FAILED");
            }
        else
            LOGE("VPC Module not found");
    }

    err = hw_get_module(LPE_HARDWARE_MODULE_ID,
    (hw_module_t const**)&module);

    if (err == 0) {
        hw_device_t* device;
        err = module->methods->open(module,LPE_HARDWARE_NAME, &device);
        if (err == 0){
            LOGD("LPE MODULE OK.");
            mlpedevice = (lpe_device_t *) device;
            err = mlpedevice->init();
            if (err)
                LOGE("LPE init FAILED");
            }
        else
            LOGE("LPE Module not found");
    }
}

AudioHardwareALSA::~AudioHardwareALSA()
{
    // Unset logger
    mParameterMgrPlatformConnector->setLogger(NULL);
    // Remove logger
    delete mParameterMgrPlatformConnectorLogger;
    // Remove connector
    delete mParameterMgrPlatformConnector;

    if (mMixer) delete mMixer;

#ifdef USE_INTEL_SRC
    if (mResampler) delete mResampler;
#endif

    if (mALSADevice)
        mALSADevice->common.close(&mALSADevice->common);
    if (mAcousticDevice)
        mAcousticDevice->common.close(&mAcousticDevice->common);
    if (mvpcdevice)
        mvpcdevice->common.close(&mvpcdevice->common);
    if (mlpedevice)
        mlpedevice->common.close(&mlpedevice->common);
}

status_t AudioHardwareALSA::initCheck()
{
#ifdef USE_INTEL_SRC
    if (mALSADevice && mMixer && mMixer->isValid() && mResampler)
#else
    if (mALSADevice && mMixer && mMixer->isValid())
#endif
        return NO_ERROR;
    else
        return NO_INIT;
}

// Used to fill types for PFW
void AudioHardwareALSA::fillSelectionCriterionType(ISelectionCriterionTypeInterface* pSelectionCriterionType, const SSelectionCriterionTypeValuePair* pSelectionCriterionTypeValuePairs, uint32_t uiNbEntries)
{
    uint32_t uiIndex;

    for (uiIndex = 0; uiIndex < uiNbEntries; uiIndex++) {

        const SSelectionCriterionTypeValuePair* pValuePair = &pSelectionCriterionTypeValuePairs[uiIndex];

        pSelectionCriterionType->addValuePair(pValuePair->iNumerical, pValuePair->pcLiteral);
    }
}

status_t AudioHardwareALSA::setVoiceVolume(float volume)
{
    // The voice volume is used by the VOICE_CALL audio stream.
    AutoW lock(mLock);
    if (mvpcdevice)
        return mvpcdevice->volume(volume);
    else
        return INVALID_OPERATION;
}

status_t AudioHardwareALSA::setMasterVolume(float volume)
{
    AutoW lock(mLock);
    if (mMixer)
        return mMixer->setMasterVolume(volume);
    else
        return INVALID_OPERATION;
}

status_t AudioHardwareALSA::setMode(int mode)
{
    AutoW lock(mLock);
    status_t status = NO_ERROR;
    status_t err_a = BAD_VALUE;

    if (mode != mMode)
        status = AudioHardwareBase::setMode(mode);

    mSelectedMode->setCriterionState(mode);

    return status;
}

AudioStreamOut *
AudioHardwareALSA::openOutputStream(uint32_t devices,
                                    int *format,
                                    uint32_t *channels,
                                    uint32_t *sampleRate,
                                    status_t *status)
{
    AutoW lock(mLock);

    LOGD("openOutputStream called for devices: 0x%08x", devices);

    status_t err = BAD_VALUE;
    AudioStreamOutALSA *out = 0;

    if (devices & (devices - 1)) {
        if (status) *status = err;
        LOGD("openOutputStream called with bad devices");
        return out;
    }

    // Find the appropriate alsa device
    for(ALSAHandleList::iterator it = mDeviceList.begin();
            it != mDeviceList.end(); ++it)
        if (it->devices & devices) {
            err = mALSADevice->open(&(*it), devices, mode());
            if (err) {
                LOGE("Open error.");
                break;
            }
            out = new AudioStreamOutALSA(this, &(*it));
            err = out->set(format, channels, sampleRate);
            if (err) {
                LOGE("set error.");
                break;
            }
            if (mvpcdevice && mvpcdevice->params && mvpcdevice->route) {
                mvpcdevice->params(mode(), devices);
                err = mvpcdevice->route(VPC_ROUTE_CLOSE);
                if (err) {
                    LOGE("openOutputStream called with bad devices");
                }
            }
            break;
        }

    if (status) *status = err;
    return out;
}

void
AudioHardwareALSA::closeOutputStream(AudioStreamOut* out)
{
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
    AutoW lock(mLock);

    status_t err = BAD_VALUE;
    AudioStreamInALSA *in = 0;
    char str[PROPERTY_VALUE_MAX];
    float defaultGain = 0.0;

    mMicMuteState = false;

    if (devices & (devices - 1)) {
        if (status) *status = err;
        return in;
    }

    property_get ("alsa.mixer.defaultGain",
            str,
            DEFAULTGAIN);
    defaultGain = strtof(str, NULL);

    // Find the appropriate alsa device
    for(ALSAHandleList::iterator it = mDeviceList.begin();
            it != mDeviceList.end(); ++it)
        if (it->devices & devices) {
            err = mALSADevice->open(&(*it), devices, mode());
            if (err) {
                LOGE("Open error.");
                break;
            }
            in = new AudioStreamInALSA(this, &(*it), acoustics);
            err = in->setGain(defaultGain);
            if (err != NO_ERROR) {
                LOGE("SetGain error.");
                break;
            }
            err = in->set(format, channels, sampleRate);
            if (err != NO_ERROR) {
                LOGE("Set error.");
                break;
            }
            if (mlpedevice && mlpedevice->lpecontrol) {
                err = mlpedevice->lpecontrol(mode(), devices);
                if (err) {
                    LOGE("openOutputStream called with bad devices");
                }
            }
            break;
        }

    if (status) *status = err;
    return in;
}

void
AudioHardwareALSA::closeInputStream(AudioStreamIn* in)
{
    mMicMuteState = false;

    delete in;
}

status_t AudioHardwareALSA::setMicMute(bool state)
{
    mMicMuteState = state;
    if(state)
        LOGD("Set MUTE true");
    else
        LOGD("Set MUTE false");

    return NO_ERROR;
}

status_t AudioHardwareALSA::getMicMute(bool *state)
{
    *state = mMicMuteState;
    if(*state)
        LOGD("Get MUTE true");
    else
        LOGD("Get MUTE false");

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
        LOGW("getInputBufferSize bad sampling rate: %d", sampleRate);
        return 0;
    }
    if (format != AudioSystem::PCM_16_BIT) {
        LOGW("getInputBufferSize bad format: %d", format);
        return 0;
    }
    if ((channelCount < 1) || (channelCount > 2)) {
        LOGW("getInputBufferSize bad channel count: %d", channelCount);
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
    LOGI("key value pair %s\n", keyValuePairs.string());

    // Search TTY mode
    if(strstr(keyValuePairs.string(), "tty_mode=tty_full") != NULL) {
        LOGV("tty full\n");
        mvpcdevice->tty(VPC_TTY_FULL);
    }
    else if(strstr(keyValuePairs.string(), "tty_mode=tty_hco") != NULL) {
        LOGV("tty hco\n");
        mvpcdevice->tty(VPC_TTY_HCO);
    }
    else if(strstr(keyValuePairs.string(), "tty_mode=tty_vco") != NULL) {
        LOGV("tty vco\n");
        mvpcdevice->tty(VPC_TTY_VCO);
    }
    else if (strstr(keyValuePairs.string(), "tty_mode=tty_off") != NULL) {
        LOGV("tty off\n");
        mvpcdevice->tty(VPC_TTY_OFF);
    }

    // Search BT NREC parameter
    if ((strstr(keyValuePairs.string(), "bt_headset_nrec=on")) != NULL) {
        LOGV("bt with acoustic\n");
        mvpcdevice->bt_nrec(VPC_BT_NREC_ON);
    }
    else if ((strstr(keyValuePairs.string(), "bt_headset_nrec=off")) != NULL) {
        LOGV("bt without acoustic\n");
        mvpcdevice->bt_nrec(VPC_BT_NREC_OFF);
    }

    return NO_ERROR;
}


// set Stream Parameters
status_t AudioHardwareALSA::setStreamParameters(alsa_handle_t* pAlsaHandle, bool bForOutput, const String8& keyValuePairs)
{
    AudioParameter param = AudioParameter(keyValuePairs);
    String8 key = String8(AudioParameter::keyRouting);
    status_t status;
    int devices;

    AutoW lock(mLock);

    // Get concerned devices
    status = param.getInt(key, devices);

    if (status != NO_ERROR) {
        // Note: this is the only place where we should bail out with an error
        // since all processing are unrelated to each other, any other error occuring here should be logged
        // and should not prevent subsequent processings to take place
        return status;
    }
    // Remove parameter
    param.remove(key);

    LOGW("AudioHardwareALSA::setStreamParameters() for %s devices: 0x%08x", bForOutput ? "output" : "input", devices);

    // VPC params
    if (devices && mvpcdevice && mvpcdevice->params) {

        status = mvpcdevice->params(mode(), (uint32_t)devices);

        if (status != NO_ERROR) {

            // Just log!
            LOGE("VPC params error: %d", status);
        }
    }

    // Alsa routing
    if (devices && mALSADevice && mALSADevice->route) {

        status = mALSADevice->route(pAlsaHandle, (uint32_t)devices, mode());

        if (status != NO_ERROR) {

            // Just log!
            LOGE("alsa route error: %d", status);
        }
    }

    // Mix disable
    if (devices && mvpcdevice && mvpcdevice->mix_disable) {
        mvpcdevice->mix_disable(mode());
    }

    if (bForOutput) {

        // Output devices changed

        // Warn PFW
        mSelectedOutputDevice->setCriterionState(devices);

    } else {

        // Input devices changed

        // Call lpecontrol
        if (mlpedevice && mlpedevice->lpecontrol) {

            status = mlpedevice->lpecontrol(mode(), (uint32_t)devices);

            if (status != NO_ERROR) {

                // Just log!
                LOGE("lpecontrol error: %d", status);
            }
        }

       // Warn PFW
       mSelectedInputDevice->setCriterionState(devices);
    }

    std::string strError;
    if (!mParameterMgrPlatformConnector->applyConfigurations(strError)) {
        LOGE("%s",strError.c_str());
    }

    // No more?
    if (param.size()) {

        // Just log!
        LOGW("Unhandled argument.");
    }

    return NO_ERROR;
}


}       // namespace android
