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

#include "AudioRouteMSICVoice.h"
#include "AudioRouteManager.h"
#include "AudioRouteBT.h"
#include "AudioRouteMM.h"
#include "AudioRouteVoiceRec.h"
#include "AudioRoute.h"
#include "AudioConversion.h"
#include "ModemAudioManagerInstance.h"

#include "stmd.h"

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

#define AUDIO_AT_CHANNEL_NAME   "/dev/gsmtty20"

// Defines path to parameters in PFW XML config files
const char* const AudioHardwareALSA::gapcDefaultSampleRates [AudioHardwareALSA::ALSA_CONF_NB_DIRECTIONS] = {
    "/Audio/CONFIGURATION/ALSA_CONF/IN/DEFAULT_SAMPLE_RATE", // Type = unsigned integer
    "/Audio/CONFIGURATION/ALSA_CONF/OUT/DEFAULT_SAMPLE_RATE" // Type = unsigned integer
};
// Default sampling rate in case the value is not found in xml file
const uint32_t AudioHardwareALSA::DEFAULT_SAMPLE_RATE = 44100;
const uint32_t AudioHardwareALSA::DEFAULT_CHANNEL_COUNT = 2;
const uint32_t AudioHardwareALSA::DEFAULT_FORMAT = AUDIO_FORMAT_PCM_16_BIT;

// Defines path to parameters in PFW XML config files
const char* const AudioHardwareALSA::gapcModemPortClockSelection[AudioHardwareALSA::IFX_NB_I2S_PORT] = {
    "/Audio/CONFIGURATION/IFX_MODEM/I2S1/CLK_SELECT", // Type = signed integer
    "/Audio/CONFIGURATION/IFX_MODEM/I2S2/CLK_SELECT" // Type = signed integer
};

// Default clock selection
const uint32_t AudioHardwareALSA::DEFAULT_IFX_CLK_SELECT = -1;

// HAL modules table
const AudioHardwareALSA::hw_module AudioHardwareALSA::hw_module_list [AudioHardwareALSA::NB_HW_DEV]= {
    { ALSA_HARDWARE_MODULE_ID, ALSA_HARDWARE_NAME },
    { ACOUSTICS_HARDWARE_MODULE_ID, ACOUSTICS_HARDWARE_NAME },
    { VPC_HARDWARE_MODULE_ID, VPC_HARDWARE_NAME },
    { FM_HARDWARE_MODULE_ID, FM_HARDWARE_MODULE_ID },
};

/// PFW related definitions
// Logger
class CParameterMgrPlatformConnectorLogger : public CParameterMgrPlatformConnector::ILogger
{
public:
    CParameterMgrPlatformConnectorLogger() {}

    virtual void log(const std::string& strLog)
    {
        ALOGD("%s",strLog.c_str());
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
    { AudioSystem::DEVICE_OUT_WIDI, "_Widi"},
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
    if (l < 0) {

        LOGE("%s: error while formating the log", __FUNCTION__);
        // Bailing out
        goto error;
    }
    if (l >= BUFSIZ) {

        // return of snprintf higher than size -> the output has been truncated
        LOGE("%s: log truncated", __FUNCTION__);
        l = BUFSIZ - 1;
    }
    vsnprintf(buf + l, BUFSIZ - l, fmt, arg);
    buf[BUFSIZ-1] = '\0';
    ALOGE("ALSALib", "%s", buf);

error:
    va_end(arg);
}

AudioHardwareInterface *AudioHardwareALSA::create() {
    return new AudioHardwareALSA();
}

AudioHardwareALSA::AudioHardwareALSA() :
    mParameterMgrPlatformConnector(new CParameterMgrPlatformConnector("/etc/parameter-framework/ParameterFrameworkConfiguration.xml")),
    mParameterMgrPlatformConnectorLogger(new CParameterMgrPlatformConnectorLogger),
    mAudioRouteMgr(new AudioRouteManager),
    mModemAudioManager(CModemAudioManagerInstance::create(this)),
    mModemCallActive(false),
    mModemAvailable(false),
    mMSICVoiceRouteForcedOnMMRoute(false),
    mStreamOutList(),
    mStreamInList(),
    mForceReconsiderInCallRoute(false),
    mCurrentTtyDevice(VPC_TTY_OFF),
    mCurrentHACSetting(VPC_HAC_OFF),
    mIsBluetoothEnabled(false)
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

    /// Creates the routes and adds them to the route mgr
    mAudioRouteMgr->addRoute(new AudioRouteMSICVoice(String8("MSIC_Voice")));
    mAudioRouteMgr->addRoute(new AudioRouteMM(String8("MultiMedia")));
    mAudioRouteMgr->addRoute(new AudioRouteBT(String8("BT")));
    mAudioRouteMgr->addRoute(new AudioRouteVoiceRec(String8("VoiceRec")));

    /// Start
    std::string strError;
    if (!mParameterMgrPlatformConnector->start(strError)) {

        LOGE("parameter-framework start error: %s", strError.c_str());
    } else {

        LOGI("parameter-framework successfully started!");
    }

    // Reset
    snd_lib_error_set_handler(&ALSAErrorHandler);
    mMixer = new ALSAMixer(this);

    // HW Modules initialisation
    hw_module_t* module;
    hw_device_t* device;

    for (int i = 0; i < NB_HW_DEV; i++)
    {
        if (hw_get_module(hw_module_list[i].module_id, (hw_module_t const**)&module))
        {
            LOGE("%s Module not found!!!", hw_module_list[i].module_id);
            mHwDeviceArray.push_back(NULL);
        }
        else if (module->methods->open(module, hw_module_list[i].module_name, &device))
        {
            LOGE("%s Module could not be opened!!!", hw_module_list[i].module_name);
            mHwDeviceArray.push_back(NULL);
        }
        else {

            mHwDeviceArray.push_back(device);
        }
    }
    if (getAlsaHwDevice()) {

        getAlsaHwDevice()->init(getAlsaHwDevice(), getIntegerParameterValue(gapcDefaultSampleRates[ALSA_CONF_DIRECTION_IN], false, DEFAULT_SAMPLE_RATE),
            getIntegerParameterValue(gapcDefaultSampleRates[ALSA_CONF_DIRECTION_OUT], false, DEFAULT_SAMPLE_RATE));
    }

    if (getVpcHwDevice()) {

        if (getVpcHwDevice()->init(getIntegerParameterValue(gapcModemPortClockSelection[IFX_I2S1_PORT], true, DEFAULT_IFX_CLK_SELECT),
                                   getIntegerParameterValue(gapcModemPortClockSelection[IFX_I2S2_PORT], true, DEFAULT_IFX_CLK_SELECT)))
        {
            LOGE("VPC MODULE init FAILED");
            // if any open issue, bailing out...
            getVpcHwDevice()->common.close(&getVpcHwDevice()->common);
            mHwDeviceArray[VPC_HW_DEV] = NULL;
        }
        else
        {
            getVpcHwDevice()->set_modem_state(mModemAudioManager->getModemStatus());
        }
    }

#ifdef WITH_FM_SUPPORT
    if (getFmHwDevice()) {
        getFmHwDevice()->init();
    } else {
        LOGE("Cannot load FM HW Module");
    }
#endif

    // Starts the modem state listener
    if(mModemAudioManager->start()) {
        LOGE("AudioHardwareALSA: could not start modem state listener");
    }
}

alsa_device_t* AudioHardwareALSA::getAlsaHwDevice() const
{
    assert(mHwDeviceArray.size() > ALSA_HW_DEV);

    return (alsa_device_t *)mHwDeviceArray[ALSA_HW_DEV];
}

vpc_device_t* AudioHardwareALSA::getVpcHwDevice() const
{
    assert(mHwDeviceArray.size() > VPC_HW_DEV);

    return (vpc_device_t *)mHwDeviceArray[VPC_HW_DEV];
}

fm_device_t* AudioHardwareALSA::getFmHwDevice() const
{
    assert(mHwDeviceArray.size() > FM_HW_DEV);

    return (fm_device_t *)mHwDeviceArray[FM_HW_DEV];
}

acoustic_device_t* AudioHardwareALSA::getAcousticHwDevice() const
{
    assert(mHwDeviceArray.size() > ACOUSTIC_HW_DEV);

    return (acoustic_device_t *)mHwDeviceArray[ACOUSTIC_HW_DEV];
}

AudioHardwareALSA::~AudioHardwareALSA()
{
    // Delete ATManager
    delete mModemAudioManager;
    // Delete route manager, it will detroy all the registered routes
    delete mAudioRouteMgr;
    // Unset logger
    mParameterMgrPlatformConnector->setLogger(NULL);
    // Remove logger
    delete mParameterMgrPlatformConnectorLogger;
    // Remove connector
    delete mParameterMgrPlatformConnector;

    // Delete all ALSA handles
    for (ALSAHandleList::iterator it = mDeviceList.begin(); it != mDeviceList.end(); ++it) {

        delete *it;
    }

    // Delete All Output Stream
    CAudioStreamOutALSAListIterator outIt;

    for (outIt = mStreamOutList.begin(); outIt != mStreamOutList.end(); ++outIt) {

        delete *outIt;
    }

    // Delete All Input Stream
    CAudioStreamOutALSAListIterator inIt;

    for (inIt = mStreamOutList.begin(); inIt != mStreamOutList.end(); ++inIt) {

        delete *inIt;
    }

    delete mMixer;

    for (int i = 0; i < NB_HW_DEV; i++) {

        if (mHwDeviceArray[i]) {

            mHwDeviceArray[i]->close(mHwDeviceArray[i]);
        }

    }
}

status_t AudioHardwareALSA::initCheck()
{

    if (getAlsaHwDevice() && mMixer && mMixer->isValid())

        return NO_ERROR;
    else
        return NO_INIT;
}

// Used to fill types for PFW
void AudioHardwareALSA::fillSelectionCriterionType(ISelectionCriterionTypeInterface* pSelectionCriterionType, const SSelectionCriterionTypeValuePair* pSelectionCriterionTypeValuePairs, uint32_t uiNbEntries) const
{
    uint32_t uiIndex;

    for (uiIndex = 0; uiIndex < uiNbEntries; uiIndex++) {

        const SSelectionCriterionTypeValuePair* pValuePair = &pSelectionCriterionTypeValuePairs[uiIndex];

        pSelectionCriterionType->addValuePair(pValuePair->iNumerical, pValuePair->pcLiteral);
    }
}

// Default Alsa sample rate discovery
uint32_t AudioHardwareALSA::getIntegerParameterValue(const string& strParameterPath, bool bSigned, uint32_t uiDefaultValue) const
{
    ALOGD("%s in", __FUNCTION__);

    if (!mParameterMgrPlatformConnector->isStarted()) {

        return uiDefaultValue;
    }

    string strError;
    // Get handle
    CParameterHandle* pParameterHandle = mParameterMgrPlatformConnector->createParameterHandle(strParameterPath, strError);

    if (!pParameterHandle) {

        strError = strParameterPath.c_str();
        strError += " not found!";

        LOGE("Unable to get parameter handle: %s", strError.c_str());

        ALOGD("%s returning %d", __FUNCTION__, uiDefaultValue);

        return uiDefaultValue;
    }

    // Retrieve value
    uint32_t uiValue;

    if ((!bSigned && !pParameterHandle->getAsInteger(uiValue, strError)) || (bSigned && !pParameterHandle->getAsSignedInteger((int32_t&)uiValue, strError))) {

        LOGE("Unable to get value: %s, from parameter path: %s", strError.c_str(), strParameterPath.c_str());

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
    AutoW lock(mLock);
    if (getVpcHwDevice())
        return getVpcHwDevice()->volume(volume);
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

    ALOGD("%s: in", __FUNCTION__);

    status_t status = NO_ERROR;

    if (mode != mMode) {

        status = AudioHardwareBase::setMode(mode);

        // Refresh VPC mode
        if (getVpcHwDevice() && getVpcHwDevice()->set_mode) {

            getVpcHwDevice()->set_mode(mode);
        }

        mSelectedMode->setCriterionState(mode);

        // According to the new mode, re-evaluate accessibility of the audio routes
        applyRouteAccessibilityRules(EModeChange);
    }

    return status;
}

status_t AudioHardwareALSA::setFmRxMode(int fm_mode)
{
    AutoW lock(mLock);

    ALOGD("%s: in", __FUNCTION__);

    if (AudioHardwareBase::setFmRxMode(fm_mode) != ALREADY_EXISTS) {
        if (fm_mode == AudioSystem::MODE_FM_ON) {
            reconsiderRouting();
            getFmHwDevice()->set_state(fm_mode);
        } else {
            getFmHwDevice()->set_state(fm_mode);
            reconsiderRouting();
        }
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
    AutoW lock(mLock);

    ALOGD("%s: called for devices: 0x%08x", __FUNCTION__, devices);

    status_t err = BAD_VALUE;
    AudioStreamOutALSA* out = 0;
    alsa_handle_t* pHandle = NULL;

    if ((devices & (devices - 1)) || (!devices & AudioSystem::DEVICE_OUT_ALL)) {

        ALOGD("%s: called with bad devices", __FUNCTION__);
        goto finish;
    }

    if (!getAlsaHwDevice()) {

        LOGE("%s: Open error, alsa hw device not valid", __FUNCTION__);
        err = DEAD_OBJECT;
        goto finish;
    }

    // Instantiante and initialize a new ALSA handle
    pHandle = new alsa_handle_t;

    err = getAlsaHwDevice()->initStream(pHandle, devices, mode());
    if (err) {

        LOGE("%s: init Stream error.", __FUNCTION__);
        goto finish;
    }

    out = new AudioStreamOutALSA(this, pHandle);

    err = out->set(format, channels, sampleRate);
    if (err) {

        LOGE("%s: set error.", __FUNCTION__);
        goto finish;
    }

    // Add ALSA handle to the list
    mDeviceList.push_back(pHandle);

    // Add Stream Out to the list
    mStreamOutList.push_back(out);

finish:
    if (err) {

        delete pHandle;
        pHandle = NULL;

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
   // Remove output stream from the list

    CAudioStreamOutALSAListIterator it;

    for (it = mStreamOutList.begin(); it != mStreamOutList.end(); ++it) {

        AudioStreamOutALSA* pOut = *it;

        if (pOut == out) {

            // Remove ALSA handle from list
            mDeviceList.remove(pOut->mHandle);

            // keep track of ALSA handle pointer
            alsa_handle_t * pHandle = pOut->mHandle;

            // Remove element
            mStreamOutList.erase(it);

            // Delete the stream and the ALSA handle
            delete out;
            out = NULL;
            delete pHandle;

            // Done
            break;
        }
    }
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

    ALOGD("%s: IN", __FUNCTION__);

    status_t err = BAD_VALUE;
    AudioStreamInALSA* in = 0;
    alsa_handle_t* pHandle = NULL;
    char str[PROPERTY_VALUE_MAX];
    float defaultGain = 0.0;

    mMicMuteState = false;

    if ((devices & (devices - 1)) || (!devices & AudioSystem::DEVICE_IN_ALL)) {

        goto finish;
    }

    property_get ("alsa.mixer.defaultGain",
            str,
            DEFAULTGAIN);
    defaultGain = strtof(str, NULL);

    if (!getAlsaHwDevice()) {

        LOGE("%s: Open error, alsa hw device not valid", __FUNCTION__);
        err = DEAD_OBJECT;
        goto finish;
    }

    // Instantiante and initialize a new ALSA handle
    pHandle = new alsa_handle_t;

    err = getAlsaHwDevice()->initStream(pHandle, devices, mode());
    if (err) {

        LOGE("%s: init Stream error.", __FUNCTION__);
        goto finish;
    }

    in = new AudioStreamInALSA(this, pHandle, acoustics);

    err = in->setGain(defaultGain);
    if (err != NO_ERROR) {

        LOGE("%s: setGain", __FUNCTION__);
        goto finish;
    }

    err = in->set(format, channels, sampleRate);
    if (err != NO_ERROR) {

        LOGE("%s: Set err", __FUNCTION__);
        goto finish;
    }

    // Add ALSA handle to the list
    mDeviceList.push_back(pHandle);

    // Add Stream Out to the list
    mStreamInList.push_back(in);

finish:
    if (err) {

        delete pHandle;
        pHandle = NULL;

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

    // Remove input stream from the list

    CAudioStreamInALSAListIterator it;

    for (it = mStreamInList.begin(); it != mStreamInList.end(); ++it) {

        AudioStreamInALSA* pIn = *it;

        if (pIn == in) {

            // Remove ALSA handle from list
            mDeviceList.remove(pIn->mHandle);

            // keep track of ALSA handle pointer
            alsa_handle_t * pHandle = pIn->mHandle;

            // Remove element
            mStreamInList.erase(it);

            // Delete the stream and the ALSA handle
            delete in;
            in = NULL;
            delete pHandle;

            // Done
            break;
        }
    }
    ALOGD("closeInputStream");
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
    AutoW lock(mLock);

    LOGI("key value pair %s\n", keyValuePairs.string());

    if (!getVpcHwDevice()) {
        return NO_ERROR;
    }

    AudioParameter param = AudioParameter(keyValuePairs);
    status_t status;

    //
    // Search BT STATE parameter
    //
    String8 strBtState;
    String8 key = String8(AUDIO_PARAMETER_KEY_BLUETOOTH_STATE);

    // Get concerned devices
    status = param.get(key, strBtState);
    if(status == NO_ERROR)
    {
        if (strBtState == AUDIO_PARAMETER_VALUE_BLUETOOTH_STATE_ON) {
            LOGV("bt enabled\n");
            mIsBluetoothEnabled = true;
        }
        else {
            //BT off or undefined: set flag to false and force BT path to off
            LOGV("bt disabled\n");
            mIsBluetoothEnabled = false;
        }
        //BT mode change: apply new route accessibility rules
        applyRouteAccessibilityRules(EModeChange);
        // Remove parameter
        param.remove(key);
    }

    //
    // Search TTY mode
    //
    String8 strTtyDevice;
    vpc_tty_t iTtyDevice = VPC_TTY_OFF;
    key = String8(AUDIO_PARAMETER_KEY_TTY_MODE);

    // Get concerned devices
    status = param.get(key, strTtyDevice);

    if (status == NO_ERROR) {

        if(strTtyDevice == AUDIO_PARAMETER_VALUE_TTY_FULL) {
            LOGV("tty full\n");
            iTtyDevice = VPC_TTY_FULL;
        }
        else if(strTtyDevice == AUDIO_PARAMETER_VALUE_TTY_HCO) {
            LOGV("tty hco\n");
            iTtyDevice = VPC_TTY_HCO;
        }
        else if(strTtyDevice == AUDIO_PARAMETER_VALUE_TTY_VCO) {
            LOGV("tty vco\n");
            iTtyDevice = VPC_TTY_VCO;
        }
        else if (strTtyDevice == AUDIO_PARAMETER_VALUE_TTY_OFF) {
            LOGV("tty off\n");
            iTtyDevice = VPC_TTY_OFF;

        }
        if (mCurrentTtyDevice != iTtyDevice) {

            mCurrentTtyDevice = iTtyDevice;
            getVpcHwDevice()->set_tty(mCurrentTtyDevice);
            mForceReconsiderInCallRoute = true;
        }
        // Remove parameter
        param.remove(key);
    }

    // Search BT NREC parameter
    String8 strBTnRecSetting;
    key = String8(AUDIO_PARAMETER_KEY_BT_NREC);

    // Get BT NREC setting value
    status = param.get(key, strBTnRecSetting);

    if (status == NO_ERROR) {
        if(strBTnRecSetting == AUDIO_PARAMETER_VALUE_ON) {
            LOGV("BT NREC on, headset is without noise reduction and echo cancellation algorithms");
            getVpcHwDevice()->bt_nrec(VPC_BT_NREC_ON);
        }
        else if(strBTnRecSetting == AUDIO_PARAMETER_VALUE_OFF) {
            LOGV("BT NREC off, headset is with noise reduction and echo cancellation algorithms");
            getVpcHwDevice()->bt_nrec(VPC_BT_NREC_OFF);

            // We reconsider routing as VPC_BT_NREC_ON intent is sent first, then setStreamParameters and finally
            // VPC_BT_NREC_OFF when the SCO link is enabled. But only VPC_BT_NREC_ON setting is applied in that
            // context, resulting in loading the wrong Audience profile for BT SCO. This is where reconsiderRouting
            // becomes necessary, to be aligned with VPC_BT_NREC_OFF to process the correct Audience profile.
            mForceReconsiderInCallRoute = true;
        }

        // Remove parameter
        param.remove(key);
    }

    // Search HAC setting
    String8 strHACSetting;
    vpc_hac_set_t iHACSetting = VPC_HAC_OFF;

    key = String8(AUDIO_PARAMETER_KEY_HAC_SETTING);

    // Get HAC setting value
    status = param.get(key, strHACSetting);

    if (status == NO_ERROR) {
        if(strHACSetting == AUDIO_PARAMETER_VALUE_HAC_ON) {
            LOGV("HAC setting is turned on, enable output on HAC device");
            iHACSetting = VPC_HAC_ON;
        }
        else if(strHACSetting == AUDIO_PARAMETER_VALUE_HAC_OFF) {
            LOGV("HAC setting is turned off");
            iHACSetting = VPC_HAC_OFF;
        }

        if (mCurrentHACSetting != iHACSetting) {

            mCurrentHACSetting = iHACSetting;
            getVpcHwDevice()->set_hac(mCurrentHACSetting);
            mForceReconsiderInCallRoute = true;
        }

        // Remove parameter
        param.remove(key);
    }

    // Reconsider the routing now in case of voice call or communication
    if (mForceReconsiderInCallRoute && isInCallOrComm(audioMode())) {

        reconsiderRouting();
    }

    mForceReconsiderInCallRoute = false;

    // Not a problem if a key does not exist, its value will
    // simply not be read and used, thus return NO_ERROR
    return NO_ERROR;
}


// set Stream Parameters
status_t AudioHardwareALSA::setStreamParameters(ALSAStreamOps* pStream, bool bForOutput, const String8& keyValuePairs)
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
    // Refreshed only on out stream changes
    if (devices && getVpcHwDevice() && getVpcHwDevice()->params && bForOutput) {

        status = getVpcHwDevice()->params(mode(), (uint32_t)devices);

        if (status != NO_ERROR) {

            // Just log!
            LOGE("VPC params error: %d", status);
        }
    }

    // Alsa routing
    if (getAlsaHwDevice()) {

        // WorkAround:
        // Reset the VoiceRec route in the modem to prevent from getting
        // glitches (DMA issue)
        if (bForOutput && audioMode() == AudioSystem::MODE_IN_CALL) {

            mAudioRouteMgr->setRouteAccessible(String8("VoiceRec"), false, AudioSystem::MODE_IN_CALL);
        }
        // End of WorkAround

        // Ask the route manager to route the new stream
        status = mAudioRouteMgr->route(pStream, devices, audioMode(), bForOutput);
        if (status != NO_ERROR) {

            // Just log!
            LOGE("alsa route error: %d", status);
        }

        // WorkAround
        // Reset the VoiceRec route in the modem to prevent from getting
        // glitches (DMA issue)
        if (bForOutput && audioMode() == AudioSystem::MODE_IN_CALL) {

            mAudioRouteMgr->setRouteAccessible(String8("VoiceRec"), true, AudioSystem::MODE_IN_CALL);
        }
        // End of WorkAround
    }

    // Mix disable
    if (devices && getVpcHwDevice() && getVpcHwDevice()->mix_disable) {

        getVpcHwDevice()->mix_disable(mode());
    }

    if (mParameterMgrPlatformConnector->isStarted()) {

        if (bForOutput) {

            // Output devices changed

            // Warn PFW
            mSelectedOutputDevice->setCriterionState(devices);

        } else {

           // Input devices changed

           // Warn PFW
           mSelectedInputDevice->setCriterionState(devices);
        }

        std::string strError;
        if (!mParameterMgrPlatformConnector->applyConfigurations(strError)) {

            LOGE("%s", strError.c_str());
        }
    }

    // No more?
    if (param.size()) {

        // Just log!
        LOGW("Unhandled argument.");
    }

    return NO_ERROR;
}

//
// This function forces a re-routing to be applied in Out Streams
//
void AudioHardwareALSA::reconsiderRouting() {

    ALOGD("%s", __FUNCTION__);

    CAudioStreamOutALSAListConstIterator it;

    for (it = mStreamOutList.begin(); it != mStreamOutList.end(); ++it)
    {
        AudioStreamOutALSA* pOut = *it;

        if (pOut->mHandle && pOut->mHandle->openFlag)
        {

            // Ask the route manager to reconsider the routing
            mAudioRouteMgr->route(pOut, pOut->mHandle->curDev, audioMode(), true);
        }
    }
}

//
// Route accessibility state machine
// NOTE:
//  An audio route is considered as accessible when a stream can be safely opened
//  on this route.
//  An audio route is unaccessible when the platform conditions does not allow to
//  open streams safely for 2 main reasons:
//      - Glitch could happen because the modem goes configures its ports and audio
//        path internally that can have side effects on I2S bus lines
//      - Modem is resetting, to avoid electrical issues, all I2S port of the platform
//        must be closed
//
void AudioHardwareALSA::applyRouteAccessibilityRules(RoutingEvent aRoutEvent)
{
    ALOGD("%s: in, mode %d, modemAvailable=%d, ModemCallActive=%d", __FUNCTION__, mode(), mModemAvailable, mModemCallActive);

    switch(mode()) {
    case AudioSystem::MODE_IN_CALL:

        // Mode in call but ModemCallActive is false => route on Media path
        // Mode in call, ModemCallActive is true => route on Voice path
        forceMediaRoute(!mModemCallActive);

        if (!mModemAvailable) {
            /* NOTE:
             * Side effect of delaying setMode(NORMAL) once call is finished in case of modem reset
             * Lost Network tone cannot be played on IN_CALL path as modem is ... in reset!!!
             * We will force a mode change to NORMAL in order to be able
             * to listen the lost network tones. In other words, it will route the stream on
             * MM route.
             * If delayed mode change is reverted, this following code can be remove
             * Remark: the device is kept the same, only the mode is changed in order not to disturb
             * user experience.
             */
            reconsiderRouting();
        }
        else if (aRoutEvent == ECallStatusChange) {

            reconsiderRouting();
        }

        // Accessibility depends on 2 conditions: "Modem is available" AND "Modem Call is Active"
        mAudioRouteMgr->setRouteAccessible(String8("VoiceRec"), mModemCallActive && mModemAvailable, mode());
        mAudioRouteMgr->setRouteAccessible(String8("BT"), mIsBluetoothEnabled && mModemCallActive && mModemAvailable, mode());

        mAudioRouteMgr->setRouteAccessible(String8("MSIC_Voice"), mModemCallActive && mModemAvailable, mode(), AudioRoute::Playback);

        // Capture on MSIC_Voice route is not accessible since controled by the modem
        mAudioRouteMgr->setRouteAccessible(String8("MSIC_Voice"), false, mode(), AudioRoute::Capture);
        break;

    case AudioSystem::MODE_IN_COMMUNICATION:
        /*
         * Accessibility depends on the modem availability
         * Modem not Available (down or cold reset):
         *      If any Input / output streams are opened on the shared I2S bus, make their routes
         *      inaccessible (ie close the hw device but returning silence for capture
         *      and trashing the playback datas) until the modem is back up.
         * Modem Available (Up):
         *      Set the route accessibility to true
         * No Modem on the platform:
         *      Set the route accessibility to true
         */
#ifdef CUSTOM_BOARD_WITHOUT_MODEM
        mAudioRouteMgr->setRouteAccessible(String8("VoiceRec"), true, mode());
        mAudioRouteMgr->setRouteAccessible(String8("BT"), mIsBluetoothEnabled, mode());
        mAudioRouteMgr->setRouteAccessible(String8("MSIC_Voice"), true, mode());
#else
        mAudioRouteMgr->setRouteAccessible(String8("VoiceRec"), mModemAvailable, mode());
        mAudioRouteMgr->setRouteAccessible(String8("BT"), mIsBluetoothEnabled && mModemAvailable, mode());
        mAudioRouteMgr->setRouteAccessible(String8("MSIC_Voice"), mModemAvailable, mode());
#endif
        break;

    default:
        // ie NORMAL, RINGTONE ...
#ifdef CUSTOM_BOARD_WITHOUT_MODEM
        mAudioRouteMgr->setRouteAccessible(String8("VoiceRec"), true, mode());
        mAudioRouteMgr->setRouteAccessible(String8("BT"), mIsBluetoothEnabled, mode());
        mAudioRouteMgr->setRouteAccessible(String8("MSIC_Voice"), true, mode());
#else
        mAudioRouteMgr->setRouteAccessible(String8("VoiceRec"), mModemAvailable, mode());
        mAudioRouteMgr->setRouteAccessible(String8("BT"), mIsBluetoothEnabled && mModemAvailable, mode());
        mAudioRouteMgr->setRouteAccessible(String8("MSIC_Voice"), mModemAvailable, mode());
#endif
        break;
    }

    ALOGD("%s: out", __FUNCTION__);
}

//
// From IATNotifier
// Called when an unsollicited command for which we registered arrived
//
void AudioHardwareALSA::onModemAudioStatusChanged()
{
    AutoW lock(mLock);
    ALOGD("%s: in", __FUNCTION__);

    bool isAudioPathAvail = mModemAudioManager->isModemAudioAvailable();


    // Modem Call State has changed?
    if (mModemCallActive != isAudioPathAvail){

        // ModemCallActive has changed, keep track
        mModemCallActive = isAudioPathAvail;

        // Inform VPC of call status
        if (getVpcHwDevice() && getVpcHwDevice()->set_call_status) {

            getVpcHwDevice()->set_call_status(mModemCallActive);
        }

        // Re-evaluate accessibility of the audio routes
        applyRouteAccessibilityRules(ECallStatusChange);

    }
    ALOGD("%s: out", __FUNCTION__);
}

//
// From IATNotifier
// Called on Modem State change reported by STMD
//
void  AudioHardwareALSA::onModemStateChanged() {

    AutoW lock(mLock);

    LOGD("%s: in", __FUNCTION__);

    mModemAvailable = mModemAudioManager->isModemAlive();


    /*
     * Informs VPC of modem status change
     * VPC might not be loaded at boot time, so
     * the modem state will also be set after loading VPC
     * HW module
     */
    if (getVpcHwDevice() && getVpcHwDevice()->set_modem_state) {
        getVpcHwDevice()->set_modem_state(mModemAudioManager->getModemStatus());
    }

    // Reset ModemCallStatus boolean
    mModemCallActive = false;

    // Re-evaluate accessibility of the audio routes
    applyRouteAccessibilityRules(EModemStateChange);

    ALOGD("%s: out", __FUNCTION__);
}

inline int AudioHardwareALSA::audioMode()
{
    //
    // The mode to be set is matching the mode of AudioHAL
    // EXCEPT when we are in call and the audio route is forced on MM
    //
    return (mMSICVoiceRouteForcedOnMMRoute && mode() == AudioSystem::MODE_IN_CALL)?
                        AudioSystem::MODE_NORMAL : mode();
}

void AudioHardwareALSA::forceMediaRoute(bool isForced)
{
    mMSICVoiceRouteForcedOnMMRoute = isForced;
}


inline bool AudioHardwareALSA::isInCallOrComm(int audMode)
{
    return ((audMode == AudioSystem::MODE_IN_CALL) ||
            (audMode == AudioSystem::MODE_IN_COMMUNICATION));
}

}       // namespace android
