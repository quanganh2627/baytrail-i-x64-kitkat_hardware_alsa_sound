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

#include "AudioRouteMSICVoice.h"
#include "AudioRouteManager.h"
#include "AudioRouteBT.h"
#include "AudioRouteMM.h"
#include "AudioRouteVoiceRec.h"
#include "AudioRoute.h"
#include "AudioConversion.h"
#include "ModemAudioManagerInstance.h"

#include "stmd.h"

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


/// Android Properties
const char* const AudioHardwareALSA::mDefaultGainPropName = "alsa.mixer.defaultGain";
const float AudioHardwareALSA::mDefaultGainValue = 1.0;
const char* const AudioHardwareALSA::mAudienceIsPresentPropName = "Audiocomms.Audience.IsPresent";
const bool AudioHardwareALSA::mAudienceIsPresentDefaultValue = false;

// Property name indicating if platform embeds a modem chip
const char* const AudioHardwareALSA::mModemEmbeddedPropName = "Audiocomms.Modem.IsPresent";
const bool AudioHardwareALSA::mModemEmbeddedDefaultValue = true;


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

// FM Mode type
const AudioHardwareALSA::SSelectionCriterionTypeValuePair AudioHardwareALSA::mFmModeValuePairs[] = {
    { AudioSystem::MODE_FM_OFF, "Off" },
    { AudioSystem::MODE_FM_ON,  "On"  }
};
const uint32_t AudioHardwareALSA::mNbFmModeValuePairs = sizeof(AudioHardwareALSA::mFmModeValuePairs)/sizeof(AudioHardwareALSA::mFmModeValuePairs[0]);

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
    mMixer(NULL),
    mDeviceList(),
    mLock(),
    mMicMuteState(false),
    mParameterMgrPlatformConnector(new CParameterMgrPlatformConnector("/etc/parameter-framework/ParameterFrameworkConfiguration.xml")),
    mParameterMgrPlatformConnectorLogger(new CParameterMgrPlatformConnectorLogger),
    mModeType(NULL),
    mInputDeviceType(NULL),
    mOutputDeviceType(NULL),
    mFmModeType(NULL),
    mSelectedMode(NULL),
    mSelectedInputDevice(NULL),
    mSelectedOutputDevice(NULL),
    mSelectedFmMode(NULL),
    mAudioRouteMgr(new AudioRouteManager),
    mModemAudioManager(CModemAudioManagerInstance::create(this)),
    mModemCallActive(false),
    mModemAvailable(false),
    mMSICVoiceRouteForcedOnMMRoute(false),
    mStreamOutList(),
    mStreamInList(),
    mHwDeviceArray(),
    mForceReconsiderInCallRoute(false),
    mCurrentTtyDevice(VPC_TTY_OFF),
    mCurrentHACSetting(VPC_HAC_OFF),
    mIsBluetoothEnabled(false),
    mHaveModem(false),
    mOutputDevices(0),
    mHwMode(AudioSystem::MODE_NORMAL),
    mLatchedAndroidMode(AudioSystem::MODE_NORMAL),
    mEchoReference(NULL)
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

    // FM Mode
    mFmModeType = mParameterMgrPlatformConnector->createSelectionCriterionType();
    fillSelectionCriterionType(mFmModeType, mFmModeValuePairs, mNbFmModeValuePairs);

    /// Criteria
    mSelectedMode = mParameterMgrPlatformConnector->createSelectionCriterion("Mode", mModeType);
    mSelectedInputDevice = mParameterMgrPlatformConnector->createSelectionCriterion("SelectedInputDevice", mInputDeviceType);
    mSelectedOutputDevice = mParameterMgrPlatformConnector->createSelectionCriterion("SelectedOutputDevice", mOutputDeviceType);
    mSelectedFmMode = mParameterMgrPlatformConnector->createSelectionCriterion("FmMode", mFmModeType);

    /// Creates the routes and adds them to the route mgr
    mAudioRouteMgr->addRoute(new AudioRouteMSICVoice(String8("MSIC_Voice")));
    mAudioRouteMgr->addRoute(new AudioRouteMM(String8("MultiMedia")));
    mAudioRouteMgr->addRoute(new AudioRouteBT(String8("BT")));
    mAudioRouteMgr->addRoute(new AudioRouteVoiceRec(String8("VoiceRec")));

    /// Start
    std::string strError;
    if (!mParameterMgrPlatformConnector->start(strError)) {

        ALOGE("parameter-framework start error: %s", strError.c_str());
    } else {

        ALOGI("parameter-framework successfully started!");
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

    //check if platform embeds a modem
    mHaveModem = TProperty<bool>(mModemEmbeddedPropName, mModemEmbeddedDefaultValue);
    if (mHaveModem) {
        LOGD("%s(): platform embeds a Modem chip", __FUNCTION__);
    } else {
        LOGD("%s(): platform does NOT embed a Modem chip", __FUNCTION__);
    }

    if (getAlsaHwDevice()) {

        getAlsaHwDevice()->init(getAlsaHwDevice(), getIntegerParameterValue(gapcDefaultSampleRates[ALSA_CONF_DIRECTION_IN], false, DEFAULT_SAMPLE_RATE),
                                getIntegerParameterValue(gapcDefaultSampleRates[ALSA_CONF_DIRECTION_OUT], false, DEFAULT_SAMPLE_RATE));
    }

    if (getVpcHwDevice()) {

        if (getVpcHwDevice()->init(getIntegerParameterValue(gapcModemPortClockSelection[IFX_I2S1_PORT], true, DEFAULT_IFX_CLK_SELECT),
                                   getIntegerParameterValue(gapcModemPortClockSelection[IFX_I2S2_PORT], true, DEFAULT_IFX_CLK_SELECT), mHaveModem))
        {
            ALOGE("VPC MODULE init FAILED");
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
    #ifndef FM_RX_ANALOG
        if (getFmHwDevice()) {
            getFmHwDevice()->init();
        } else {
            ALOGE("Cannot load FM HW Module");
        }
    #endif
#endif
    if(mHaveModem){
        // Starts the modem state listener
        if(mModemAudioManager->start()) {
            ALOGE("AudioHardwareALSA: could not start modem state listener");
        }
    }else{
        mAudioRouteMgr->setRouteAccessible(String8("VoiceRec"), false, hwMode());
        mAudioRouteMgr->setRouteAccessible(String8("MSIC_Voice"), true, hwMode());
    }

    mHaveAudience = TProperty<bool>(mAudienceIsPresentPropName, mAudienceIsPresentDefaultValue);
    if(mHaveAudience)
    {
        ALOGD("%s(): platform embeds an Audience chip", __FUNCTION__);
    }
    else
    {
        ALOGD("%s(): platform does NOT embed an Audience chip", __FUNCTION__);
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

status_t AudioHardwareALSA::setFmRxMode(int fm_mode)
{
    AutoW lock(mLock);

    ALOGD("%s: in", __FUNCTION__);

    if (AudioHardwareBase::setFmRxMode(fm_mode) != ALREADY_EXISTS &&
            (latchedAndroidMode() == AudioSystem::MODE_NORMAL || fm_mode == AudioSystem::MODE_FM_OFF)) {
        if (fm_mode == AudioSystem::MODE_FM_ON) {
            reconsiderRouting();
#ifndef FM_RX_ANALOG
            getFmHwDevice()->set_state(fm_mode);
#endif
        } else {
#ifndef FM_RX_ANALOG
            getFmHwDevice()->set_state(fm_mode);
#endif
            reconsiderRouting();
        }

        mSelectedFmMode->setCriterionState(fm_mode);

        std::string strError;
        if (!mParameterMgrPlatformConnector->applyConfigurations(strError)) {

            LOGE("%s", strError.c_str());
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

        ALOGE("%s: Open error, alsa hw device not valid", __FUNCTION__);
        err = DEAD_OBJECT;
        goto finish;
    }

    // Instantiante and initialize a new ALSA handle
    pHandle = new alsa_handle_t;

    err = getAlsaHwDevice()->initStream(pHandle, devices, hwMode(), getFmRxMode());
    if (err) {

        ALOGE("%s: init Stream error.", __FUNCTION__);
        goto finish;
    }

    out = new AudioStreamOutALSA(this, pHandle);

    err = out->set(format, channels, sampleRate);
    if (err) {

        ALOGE("%s: set error.", __FUNCTION__);
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
    float fDefaultGain = 0.0;

    mMicMuteState = false;
    fDefaultGain = TProperty<float>(mDefaultGainPropName, mDefaultGainValue);

    if ((devices & (devices - 1)) || (!devices & AudioSystem::DEVICE_IN_ALL)) {

        goto finish;
    }

    if (!getAlsaHwDevice()) {

        ALOGE("%s: Open error, alsa hw device not valid", __FUNCTION__);
        err = DEAD_OBJECT;
        goto finish;
    }

    // Instantiante and initialize a new ALSA handle
    pHandle = new alsa_handle_t;

    err = getAlsaHwDevice()->initStream(pHandle, devices, hwMode(), getFmRxMode());
    if (err) {

        ALOGE("%s: init Stream error.", __FUNCTION__);
        goto finish;
    }

    in = new AudioStreamInALSA(this, pHandle, acoustics);

    err = in->setGain(fDefaultGain);
    if (err != NO_ERROR) {

        ALOGE("%s: setGain", __FUNCTION__);
        goto finish;
    }

    err = in->set(format, channels, sampleRate);
    if (err != NO_ERROR) {

        ALOGE("%s: Set err", __FUNCTION__);
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
    AutoW lock(mLock);

    ALOGI("key value pair %s\n", keyValuePairs.string());

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
            ALOGV("bt enabled\n");
            mIsBluetoothEnabled = true;
        }
        else {
            //BT off or undefined: set flag to false and force BT path to off
            ALOGV("bt disabled\n");
            mIsBluetoothEnabled = false;
        }
        //BT mode change: apply new route accessibility rules
        applyRouteAccessibilityRules(EParamChange);
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
            ALOGV("tty full\n");
            iTtyDevice = VPC_TTY_FULL;
        }
        else if(strTtyDevice == AUDIO_PARAMETER_VALUE_TTY_HCO) {
            ALOGV("tty hco\n");
            iTtyDevice = VPC_TTY_HCO;
        }
        else if(strTtyDevice == AUDIO_PARAMETER_VALUE_TTY_VCO) {
            ALOGV("tty vco\n");
            iTtyDevice = VPC_TTY_VCO;
        }
        else if (strTtyDevice == AUDIO_PARAMETER_VALUE_TTY_OFF) {
            ALOGV("tty off\n");
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

    // Tablet supports only HSP, these requests should be ignored on tablet to avoid "non acoustic" audience profile by default.
    if(mHaveModem) {

        // Search BT NREC parameter
        String8 strBTnRecSetting;
        key = String8(AUDIO_PARAMETER_KEY_BT_NREC);

        // Get BT NREC setting value
        status = param.get(key, strBTnRecSetting);

        if(status == NO_ERROR) {
            if(strBTnRecSetting == AUDIO_PARAMETER_VALUE_ON) {
                LOGV("BT NREC on, headset is without noise reduction and echo cancellation algorithms");
                getVpcHwDevice()->bt_nrec(VPC_BT_NREC_ON);
            }
            else if(strBTnRecSetting == AUDIO_PARAMETER_VALUE_OFF) {
                LOGV("BT NREC off, headset is with noise reduction and echo cancellation algorithms");
                getVpcHwDevice()->bt_nrec(VPC_BT_NREC_OFF);

                /* We reconsider routing as VPC_BT_NREC_ON intent is sent first, then setStreamParameters and finally
                 * VPC_BT_NREC_OFF when the SCO link is enabled. But only VPC_BT_NREC_ON setting is applied in that
                 * context, resulting in loading the wrong Audience profile for BT SCO. This is where reconsiderRouting
                 * becomes necessary, to be aligned with VPC_BT_NREC_OFF to process the correct Audience profile.
                 */
                mForceReconsiderInCallRoute = true;
            }

        // Remove parameter
        param.remove(key);
        }
    }

    // Search HAC setting
    String8 strHACSetting;
    vpc_hac_set_t iHACSetting = VPC_HAC_OFF;

    key = String8(AUDIO_PARAMETER_KEY_HAC_SETTING);

    // Get HAC setting value
    status = param.get(key, strHACSetting);

    if (status == NO_ERROR) {
        if(strHACSetting == AUDIO_PARAMETER_VALUE_HAC_ON) {
            ALOGV("HAC setting is turned on, enable output on HAC device");
            iHACSetting = VPC_HAC_ON;
        }
        else if(strHACSetting == AUDIO_PARAMETER_VALUE_HAC_OFF) {
            ALOGV("HAC setting is turned off");
            iHACSetting = VPC_HAC_OFF;
        }

        if (mCurrentHACSetting != iHACSetting) {

            mCurrentHACSetting = iHACSetting;
            getVpcHwDevice()->set_hac(mCurrentHACSetting);
            // Reconsider routing only if current accessory is earpiece
            if (mOutputDevices & AudioSystem::DEVICE_OUT_EARPIECE)
                mForceReconsiderInCallRoute = true;
        }

        // Remove parameter
        param.remove(key);
    }

    // Reconsider the routing now in case of voice call or communication
    if (mForceReconsiderInCallRoute && isInCallOrComm()) {

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

    int currentLatchedAndroidMode;

    AutoW lock(mLock);

    currentLatchedAndroidMode = latchedAndroidMode();

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

    ALOGW("AudioHardwareALSA::setStreamParameters() for %s devices: 0x%08x", bForOutput ? "output" : "input", devices);

    if (bForOutput) {

        mOutputDevices = devices;

        /* Latch the android mode */
        latchAndroidMode();

        /* Update the HW mode (if needed) */
        updateHwMode();

        applyRouteAccessibilityRules(EAndroidModeChange);
    }

    /** VPC params
     * Refreshed only on out stream changes
     */
    if (devices && getVpcHwDevice() && getVpcHwDevice()->params && bForOutput) {
        // Pass hw mode translated to VPC, in case a multimedia path is needed during call
        // or any "transmode" is required.
        status = getVpcHwDevice()->params(hwMode(), (uint32_t)devices);


        if (status != NO_ERROR) {

            // Just log!
            ALOGE("VPC params error: %d", status);
        }
    }

    // Alsa routing
    if (getAlsaHwDevice()) {

        // WorkAround:
        // Reset the VoiceRec route in the modem to prevent from getting
        // glitches (DMA issue)
        if (bForOutput && hwMode() == AudioSystem::MODE_IN_CALL) {

            mAudioRouteMgr->setRouteAccessible(String8("VoiceRec"), false, AudioSystem::MODE_IN_CALL);
        }
        // End of WorkAround

        // Ask the route manager to route the new stream
        status = mAudioRouteMgr->route(pStream, devices, hwMode(), bForOutput);
        if (status != NO_ERROR) {

            // Just log!
            ALOGE("alsa route error: %d", status);
        }

        // WorkAround
        // Reset the VoiceRec route in the modem to prevent from getting
        // glitches (DMA issue)
        if (bForOutput && hwMode() == AudioSystem::MODE_IN_CALL) {

            mAudioRouteMgr->setRouteAccessible(String8("VoiceRec"), true, AudioSystem::MODE_IN_CALL);
        }
        // End of WorkAround
    }


#ifndef FM_RX_ANALOG
    // Handle Android Latched mode change while FM already ON
    // Checking upon Android Latched mode can guarantee the output device
    // has been changed, and the modem has closed its I2S ports.
    if (bForOutput && (getFmRxMode() == AudioSystem::MODE_FM_ON) &&
            (currentLatchedAndroidMode != latchedAndroidMode()) &&
            (latchedAndroidMode() == AudioSystem::MODE_NORMAL) ) {
        getFmHwDevice()->set_state(getFmRxMode());
    }
#endif

    // Mix disable
    if (devices && getVpcHwDevice() && getVpcHwDevice()->mix_disable) {

        getVpcHwDevice()->mix_disable(hwMode());
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

            ALOGE("%s", strError.c_str());
        }
    }

    // No more?
    if (param.size()) {

        // Just log!
        ALOGW("Unhandled argument.");
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
            mAudioRouteMgr->route(pOut, pOut->mHandle->curDev, hwMode(), true);
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
// Accessibility of route is independant of the mode
//
void AudioHardwareALSA::applyRouteAccessibilityRules(RoutingEvent routeEvent)
{
    LOGD("%s: in, mode %d, modemAvailable=%d, ModemCallActive=%d mIsBluetoothEnabled=%d", __FUNCTION__, hwMode(), mModemAvailable, mModemCallActive, mIsBluetoothEnabled);

    //
    // Evaluate BT route. This route has a dependency on:
    //      -Android Mode
    //      -Modem Call Status
    //      -Modem State
    // If Android Mode is still in call but the call status is false, do not
    // use BT route (glitches may occur)
    //
    if(!mHaveModem){
        mAudioRouteMgr->setRouteAccessible(String8("BT"), mIsBluetoothEnabled, hwMode());
    }else{
        bool isBtAccessible = mIsBluetoothEnabled && mModemAvailable;
        if (mode() == AudioSystem::MODE_IN_CALL && !mModemCallActive) {

            isBtAccessible = false;
        }
        mAudioRouteMgr->setRouteAccessible(String8("BT"), isBtAccessible, hwMode());

        // Enable the VoiceRec route only when we are in IN_CALL mode, in order to generate
        // silence on any VCR input while not in this mode
        mAudioRouteMgr->setRouteAccessible(String8("VoiceRec"), mModemAvailable && mModemCallActive && (hwMode() == AudioSystem::MODE_IN_CALL), hwMode());

        //
        // In case of ModemStateChange OR CallStatusChanged, might have not only to
        // set the route accessible/unaccessible but also to force to reconsider
        // the routing and providing a new route to listen end of call or lost
        // coverage tones.
        //
        if (routeEvent == EModemStateChange || routeEvent == ECallStatusChange) {

            reconsiderRouting();

            mAudioRouteMgr->setRouteAccessible(String8("MSIC_Voice"), mModemAvailable, hwMode());
        }
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

        // Update the HW Mode
        updateHwMode();

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

    // Update the HW Mode
    updateHwMode();

    // Re-evaluate accessibility of the audio routes
    applyRouteAccessibilityRules(EModemStateChange);

    ALOGD("%s: out", __FUNCTION__);
}

//
// From IATNotifier
// Called on Modem Audio PCM change report
//
void  AudioHardwareALSA::onModemAudioPCMChanged() {

    MODEM_CODEC modemCodec;
    vpc_band_t band;

    AutoW lock(mLock);
    LOGD("%s: in", __FUNCTION__);

    modemCodec = mModemAudioManager->getModemCodec();
    if (modemCodec == CODEC_TYPE_WB_AMR_SPEECH)
        band = VPC_BAND_WIDE;
    else
        band = VPC_BAND_NARROW;
    /*
     * Informs VPC of modem codec change
     */
    if (getVpcHwDevice() && getVpcHwDevice()->set_band) {
        getVpcHwDevice()->set_band(band, AudioSystem::MODE_IN_CALL);
    }

    LOGD("%s: out", __FUNCTION__);
}

inline bool AudioHardwareALSA::isInCallOrComm() const
{
    return ((latchedAndroidMode() == AudioSystem::MODE_IN_CALL) ||
          (latchedAndroidMode() == AudioSystem::MODE_IN_COMMUNICATION));
}

//
// This function updates the HwMode, informs the VPC and PFW as well
//
void AudioHardwareALSA::updateHwMode()
{
    if (checkAndSetHwMode()) {

        LOGD("%s: hwMode has changed to %d", __FUNCTION__, hwMode());
        // Refresh VPC mode
        if (getVpcHwDevice() && getVpcHwDevice()->set_mode) {

            getVpcHwDevice()->set_mode(hwMode());
        }

        // Refresh PFW mode
        mSelectedMode->setCriterionState(hwMode());

        std::string strError;
        if (!mParameterMgrPlatformConnector->applyConfigurations(strError)) {

            LOGE("%s", strError.c_str());
        }
    }
}

//
// HW Mode is used for routing purpose.
// It may match the Android Mode or not.
//
// This function evaluates the hw mode and update it if needed.
// Returns true if the hw mode has changed
//
bool AudioHardwareALSA::checkAndSetHwMode()
{
    LOGD("%s: Android Mode=%d, Latched Mode=%d, HW Mode=%d, nb of output devices=%d ", __FUNCTION__, mode(), mLatchedAndroidMode, hwMode(), popcount(mOutputDevices));

    //
    // Add here all "transmode" required:
    //
    // 1st case:    Android Latched Mode set to In call BUT modem Call Active flag is false
    //                          => NORMAL Mode
    // 2nd case:
    //              If a multimedia stream is output on several devices (e.g. speaker + headset)
    // during a call or a communication (like the camera shutter sound), the media
    // route (normal mode) is forced so that the audio dual route is correctly chosen
    //                          => NORMAL Mode
    //
    int hwMode = ((latchedAndroidMode() == AudioSystem::MODE_IN_CALL && !mModemCallActive) ||
            (popcount(mOutputDevices) > 1 && isInCallOrComm())) ? AudioSystem::MODE_NORMAL : latchedAndroidMode();

    if (hwMode != mHwMode) {

        mHwMode = hwMode;
        return true;
    }
    return false;
}

void AudioHardwareALSA::latchAndroidMode()
{
    // Latch the android mode
    mLatchedAndroidMode = mode();
}
/**
  * the purpose of this function is
  * - to stop the processing (i.e. writing of playback frames as echo reference for
  * for AEC effect) in AudioSteamOutALSA
  * - reset locally stored echo reference
  * @par reference: pointer to echo reference to reset
  * @return none
  */
void AudioHardwareALSA::resetEchoReference(struct echo_reference_itfe * reference)
{
    ALOGD(" %s(reference=%p)", __FUNCTION__, reference);
    if (mEchoReference != NULL && reference == mEchoReference)
    {
        if(mStreamOutList.empty())
        {
            ALOGE("%s: list of output streams is empty, i.e. AEC effect did not have necessary provide data reference!", __FUNCTION__);
        }
        else
        {
            if (mStreamOutList.size() > 1)
            {
                ALOGW("%s: list of output streams contains more than 1 stream, take 1st one as data reference", __FUNCTION__);
            }
            //by default, we use the first stream in list
            AudioStreamOutALSA* pOut = *(mStreamOutList.begin());
            pOut->removeEchoReference(reference);
        }
        release_echo_reference(reference);
        mEchoReference = NULL;
    }
}

/**
  * the purpose of this function is
  * - create echo_reference_itfe using input stream and output stream parameters
  * - add echo_reference_itfs to AudioSteamOutALSA which will use it for providing playback frames as echo reference for AEC effect
  * - store locally the created reference
  * - return created echo_reference_itfe to caller (i.e. AudioSteamInALSA)
  * Note: created echo_reference_itfe is used as backlink between playback which provides reference of output data and record which applies AEC effect
  * @par format: input stream format
  * @par channel_count: input stream channels count
  * @par sampling_rate: input stream sampling rate
  * @return NULL is creation of echo_reference_itfe failed overwise, pointer to created echo_reference_itfe
  */
struct echo_reference_itfe * AudioHardwareALSA::getEchoReference(int format, uint32_t channel_count, uint32_t sampling_rate)
{
    ALOGD("%s ()", __FUNCTION__);
    resetEchoReference(mEchoReference);

    if(mStreamOutList.empty())
    {
        ALOGE("%s: list of output streams is empty, so problem to provide data reference for AEC effect!", __FUNCTION__);
    }
    else
    {
        if (mStreamOutList.size() > 1)
        {
            ALOGW("%s: list of output streams is empty, i.e. AEC effect did not have necessary provide data reference!", __FUNCTION__);
        }
        //by default, we use the first stream in list
        AudioStreamOutALSA* pOut = *(mStreamOutList.begin());

        int wr_format = pOut->format();
        uint32_t wrChannelCount = pOut->channelCount();
        uint32_t wrSampleRate = AudioHardwareALSA::DEFAULT_SAMPLE_RATE;

        if (create_echo_reference((audio_format_t)format,
                                  channel_count,
                                  sampling_rate,
                                  (audio_format_t)wr_format,
                                  wrChannelCount,
                                  wrSampleRate,
                                  &mEchoReference) < 0)
        {
            ALOGE("%s: Could not create echo reference", __FUNCTION__);
            mEchoReference = NULL;
        }
        else
        {
            pOut->addEchoReference(mEchoReference);
        }
    }
    ALOGD(" %s() will return that mEchoReference=%p", __FUNCTION__, mEchoReference);
    return mEchoReference;
}

}       // namespace android
