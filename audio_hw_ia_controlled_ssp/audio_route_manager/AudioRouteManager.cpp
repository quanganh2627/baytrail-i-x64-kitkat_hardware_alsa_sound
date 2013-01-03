/* AudioRouteManager.cpp
 **
 ** Copyright 2012 Intel Corporation
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **      http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#define LOG_TAG "RouteManager"
#include <utils/Log.h>
#include <string>

#include <AudioHardwareALSA.h>
#include <ALSAStreamOps.h>
#include <AudioStreamInALSA.h>
#include "EventThread.h"
#include "AudioRouteManager.h"
#include "AudioRoute.h"
#include "AudioStreamRoute.h"
#include "AudioPortGroup.h"
#include "AudioPort.h"
#include "AudioPlatformState.h"
#include "ParameterMgrPlatformConnector.h"
#include "SelectionCriterionInterface.h"
#include "ModemAudioManagerInstance.h"
#include "Property.h"

#include "AudioPlatformHardware.h"
#include "AudioParameterHandler.h"

typedef RWLock::AutoRLock AutoR;
typedef RWLock::AutoWLock AutoW;

#define MASK_32_BITS_MSB    0x7FFFFFFF
#define REMOVE_32_BITS_MSB(bitfield) bitfield & MASK_32_BITS_MSB

namespace android_audio_legacy
{

// Defines the name of the Android property describing the name of the PFW configuration file
const char* const CAudioRouteManager::mPFWConfigurationFileNamePropertyName = "AudioComms.PFW.ConfPath";

const uint32_t CAudioRouteManager::_uiTimeoutSec = 2;

const char* CAudioRouteManager::gpcVoiceVolume = "/Audio/IMC/SOUND_CARD/PORTS/I2S1/TX/VOLUME/LEVEL"; // Type = unsigned integer

const uint32_t CAudioRouteManager::ENABLED = 1;
const uint32_t CAudioRouteManager::DISABLED = 0;

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

const char* const CAudioRouteManager::gapcLineInToHeadsetLineVolume = "/Audio/CIRRUS/SOUND_CARD/MIXER/HEADPHONE_LINE/INPUT_PATH_SOURCE/VOLUME"; // Type = integer
const char* const CAudioRouteManager::gapcLineInToSpeakerLineVolume = "/Audio/CIRRUS/SOUND_CARD/MIXER/SPEAKERPHONE_LINE/INPUT_PATH_SOURCE/VOLUME"; // Type = integer
const char* const CAudioRouteManager::gapcLineInToEarSpeakerLineVolume = "/Audio/CIRRUS/SOUND_CARD/MIXER/EAR_SPEAKER_LINE/INPUT_PATH_SOURCE/VOLUME"; // Type = integer

const char* const CAudioRouteManager::gapcDefaultFmRxMaxVolume[CAudioRouteManager::FM_RX_NB_DEVICE] = {
    "/Audio/CONFIGURATION/FM_CONF/SPEAKER/VOLUME", // Type = unsigned integer
    "/Audio/CONFIGURATION/FM_CONF/HEADSET/VOLUME"  // Type = unsigned integer
};

const CAudioRouteManager::CriteriaInterface CAudioRouteManager::_apCriteriaInterface[CAudioRouteManager::ENbCriteria] = {
    {"Mode", CAudioRouteManager::EModeCriteriaType},
    {"FmMode", CAudioRouteManager::EFmModeCriteriaType},
    {"TTYDirection", CAudioRouteManager::ETtyDirectionCriteriaType},
    {"RoutageState", CAudioRouteManager::ERoutingStageCriteriaType},
    {"PreviousRouteCapture", CAudioRouteManager::ERouteCriteriaType},
    {"PreviousRoutePlayback", CAudioRouteManager::ERouteCriteriaType},
    {"CurrentRouteCapture", CAudioRouteManager::ERouteCriteriaType},
    {"CurrentRoutePlayback", CAudioRouteManager::ERouteCriteriaType},
    {"SelectedInputDevice", CAudioRouteManager::EInputDeviceCriteriaType},
    {"SelectedOutputDevice", CAudioRouteManager::EOutputDeviceCriteriaType},
    {"AudioSource", CAudioRouteManager::EInputSourceCriteriaType},
    {"BandRinging", CAudioRouteManager::EBandRingingCriteriaType},
    {"BandType", CAudioRouteManager::EBandCriteriaType},
    {"BtHeadsetNrEc", CAudioRouteManager::EBtHeadsetNrEcCriteriaType},
    {"HAC", CAudioRouteManager::EHacModeCriteriaType},
};


const uint32_t CAudioRouteManager::FM_RX_STREAM_MAX_VOLUME = 15;
const uint32_t CAudioRouteManager::DEFAULT_FM_RX_VOL_MAX = 55;

// Mode type
const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioRouteManager::_stModeValuePairs[] = {
    { AudioSystem::MODE_NORMAL, "Normal" },
    { AudioSystem::MODE_RINGTONE, "RingTone" },
    { AudioSystem::MODE_IN_CALL, "InCsvCall" },
    { AudioSystem::MODE_IN_COMMUNICATION, "InVoipCall" }
};

// Selected Input Device type
const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioRouteManager::_stInputDeviceValuePairs[] = {
    { REMOVE_32_BITS_MSB(AudioSystem::DEVICE_IN_COMMUNICATION), "Communication" },
    { REMOVE_32_BITS_MSB(AudioSystem::DEVICE_IN_AMBIENT), "Ambient" },
    { REMOVE_32_BITS_MSB(AudioSystem::DEVICE_IN_BUILTIN_MIC), "Main" },
    { REMOVE_32_BITS_MSB(AudioSystem::DEVICE_IN_BLUETOOTH_SCO_HEADSET), "SCO_Headset" },
    { REMOVE_32_BITS_MSB(AudioSystem::DEVICE_IN_WIRED_HEADSET), "Headset" },
    { REMOVE_32_BITS_MSB(AudioSystem::DEVICE_IN_AUX_DIGITAL), "AuxDigital" },
    { REMOVE_32_BITS_MSB(AudioSystem::DEVICE_IN_VOICE_CALL), "VoiceCall" },
    { REMOVE_32_BITS_MSB(AudioSystem::DEVICE_IN_BACK_MIC), "Back" },
    { REMOVE_32_BITS_MSB(AudioSystem::DEVICE_IN_FM_RECORD), "FmRecord" }
};

// Selected Output Device type
const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioRouteManager::_stOutputDeviceValuePairs[] = {
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

// Routing mode
const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioRouteManager::_stRoutingStageValuePairs[] = {
    { CAudioRouteManager::EMute , "Mute" },
    { CAudioRouteManager::EDisable , "Disable" },
    { CAudioRouteManager::EConfigure , "Configure" },
    { CAudioRouteManager::EEnable , "Enable" },
    { CAudioRouteManager::EUnmute , "Unmute" }
};

// Audio Source
const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioRouteManager::_stAudioSourceValuePairs[] = {
    { AUDIO_SOURCE_DEFAULT, "Default" },
    { AUDIO_SOURCE_MIC, "Mic" },
    { AUDIO_SOURCE_VOICE_UPLINK, "VoiceUplink" },
    { AUDIO_SOURCE_VOICE_DOWNLINK, "VoiceDownlink" },
    { AUDIO_SOURCE_VOICE_CALL, "VoiceCall" },
    { AUDIO_SOURCE_CAMCORDER, "Camcorder" },
    { AUDIO_SOURCE_VOICE_RECOGNITION, "VoiceRecognition" },
    { AUDIO_SOURCE_VOICE_COMMUNICATION, "VoiceCommunication" }
};

// Voice Codec Band
// Band ringing
const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioRouteManager::_stBandTypeValuePairs[] = {
    { 0 , "NB" },
    { 1 , "WB" }
};

// TTY mode
const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioRouteManager::_stTTYDirectionValuePairs[] = {
    { 1 , "Downlink" },
    { 2 , "Uplink" },
};

// FM Mode
const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioRouteManager::_stFmModeValuePairs[] = {
    { 0 , "Off" },
    { 1 , "On" }
};

// Band ringing
const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioRouteManager::_stBandRingingValuePairs[] = {
    { 1 , "NetworkGenerated" },
    { 2 , "PhoneGenerated" }
};

// BT Headset NrEc
const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioRouteManager::_stBtHeadsetNrEcValuePairs[] = {
    { 0 , "False" },
    { 1 , "True" }
};

// HAC Mode
const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioRouteManager::_stHACModeValuePairs[] = {
    { 0 , "Off" },
    { 1 , "On" }
};

const CAudioRouteManager::SSelectionCriterionTypeInterface CAudioRouteManager::_asCriteriaType[ENbCriteriaType] = {
    // Mode
    {
        CAudioRouteManager::EModeCriteriaType,
        CAudioRouteManager::_stModeValuePairs,
        sizeof(CAudioRouteManager::_stModeValuePairs)/sizeof(CAudioRouteManager::_stModeValuePairs[0]),
        false
    },
    // FM Mode
    {
        CAudioRouteManager::EFmModeCriteriaType,
        CAudioRouteManager::_stFmModeValuePairs,
        sizeof(CAudioRouteManager::_stFmModeValuePairs)/sizeof(CAudioRouteManager::_stFmModeValuePairs[0]),
        false
    },
    // TTY Direction
    {
        CAudioRouteManager::ETtyDirectionCriteriaType,
        CAudioRouteManager::_stTTYDirectionValuePairs,
        sizeof(CAudioRouteManager::_stTTYDirectionValuePairs)/sizeof(CAudioRouteManager::_stTTYDirectionValuePairs[0]),
        true
    },
    // Routing Stage
    {
        CAudioRouteManager::ERoutingStageCriteriaType,
        CAudioRouteManager::_stRoutingStageValuePairs,
        sizeof(CAudioRouteManager::_stRoutingStageValuePairs)/sizeof(CAudioRouteManager::_stRoutingStageValuePairs[0]),
        false
    },
    // Route
    {
        CAudioRouteManager::ERouteCriteriaType,
        CAudioPlatformHardware::_stRouteValuePairs,
        CAudioPlatformHardware::_uiNbRouteValuePairs,
        true
    },
    // InputDevice
    {
        CAudioRouteManager::EInputDeviceCriteriaType,
        CAudioRouteManager::_stInputDeviceValuePairs,
        sizeof(CAudioRouteManager::_stInputDeviceValuePairs)/sizeof(CAudioRouteManager::_stInputDeviceValuePairs[0]),
        true
    },
    // OutputDevice
    {
        CAudioRouteManager::EOutputDeviceCriteriaType,
        CAudioRouteManager::_stOutputDeviceValuePairs,
        sizeof(CAudioRouteManager::_stOutputDeviceValuePairs)/sizeof(CAudioRouteManager::_stOutputDeviceValuePairs[0]),
        true
    },
    // Input Source
    {
        CAudioRouteManager::EInputSourceCriteriaType,
        CAudioRouteManager::_stAudioSourceValuePairs,
        sizeof(CAudioRouteManager::_stAudioSourceValuePairs)/sizeof(CAudioRouteManager::_stAudioSourceValuePairs[0]),
        false
    },
    // Band Ringing
    {
        CAudioRouteManager::EBandRingingCriteriaType,
        CAudioRouteManager::_stBandRingingValuePairs,
        sizeof(CAudioRouteManager::_stBandRingingValuePairs)/sizeof(CAudioRouteManager::_stBandRingingValuePairs[0]),
        false
    },
    // Band Type
    {
        CAudioRouteManager::EBandCriteriaType,
        CAudioRouteManager::_stBandTypeValuePairs,
        sizeof(CAudioRouteManager::_stBandTypeValuePairs)/sizeof(CAudioRouteManager::_stBandTypeValuePairs[0]),
        false
    },
    // BT HEADSET NrEc
    {
        CAudioRouteManager::EBtHeadsetNrEcCriteriaType,
        CAudioRouteManager::_stBtHeadsetNrEcValuePairs,
        sizeof(CAudioRouteManager::_stBtHeadsetNrEcValuePairs)/sizeof(CAudioRouteManager::_stBtHeadsetNrEcValuePairs[0]),
        false
    },
    // HAC mode
    {
        CAudioRouteManager::EHacModeCriteriaType,
        CAudioRouteManager::_stHACModeValuePairs,
        sizeof(CAudioRouteManager::_stHACModeValuePairs)/sizeof(CAudioRouteManager::_stHACModeValuePairs[0]),
        false
    }
};

// Property name indicating if platform embeds a modem chip
const char* const CAudioRouteManager::mModemEmbeddedPropName = "Audiocomms.Modem.IsPresent";
const bool CAudioRouteManager::mModemEmbeddedDefaultValue = true;

const char* const CAudioRouteManager::mBluetoothHFPSupportedPropName = "Audiocomms.BT.HFP.Supported";
const bool CAudioRouteManager::mBluetoothHFPSupportedDefaultValue = true;

const char* const CAudioRouteManager::mFmSupportedPropName = "Audiocomms.FM.Supported";
const bool CAudioRouteManager::mFmSupportedDefaultValue = false;

const char* const CAudioRouteManager::mFmIsAnalogPropName = "Audiocomms.FM.IsAnalog";
const bool CAudioRouteManager::mFmIsAnalogDefaultValue = false;


const string CAudioRouteManager::print_criteria(int32_t uiValue, CriteriaType eCriteriaType) const {

    string strPrint = "{";
    bool bFirst = true;
    bool bFound = false;

    const SSelectionCriterionTypeValuePair* sPair = _asCriteriaType[eCriteriaType]._pCriterionTypeValuePairs;
    uint32_t uiNbPairs = _asCriteriaType[eCriteriaType]._uiNbValuePairs;
    bool bIsInclusive = _asCriteriaType[eCriteriaType]._bIsInclusive;

    for (uint32_t i=0; i < uiNbPairs; i++) {

        if (bIsInclusive) {
            if (uiValue & sPair[i].iNumerical) {

                bFound = true;
                if (!bFirst) {

                    strPrint += "|";
                } else {

                    bFirst = false;
                }
                strPrint += sPair[i].pcLiteral;
            }
        } else {

            if (uiValue == sPair[i].iNumerical) {

                bFound = true;
                strPrint += sPair[i].pcLiteral;
                break;
            }
        }
    }
    if (!bFound) {

        strPrint += "none";
    }
    strPrint += "}";
    return strPrint;
}

CAudioRouteManager::CAudioRouteManager(AudioHardwareALSA *pParent) :
    _pParameterMgrPlatformConnectorLogger(new CParameterMgrPlatformConnectorLogger),
#ifdef VB_HAL_AUDIO_TEMP
    _pModemAudioManager(NULL/*CModemAudioManagerInstance::create(this)*/),
#else
    _pModemAudioManager(CModemAudioManagerInstance::create(this)),
#endif
    _pPlatformState(new CAudioPlatformState(this)),
#ifdef VB_HAL_AUDIO_TEMP
    _bModemCallActive(true),
    _bModemAvailable(true),
#else
    _bModemCallActive(false),
    _bModemAvailable(false),
#endif
    _pEventThread(new CEventThread(this)),
    _bIsStarted(false),
    _bClientWaiting(false),
    _pParent(pParent),
    _pAudioParameterHandler(new CAudioParameterHandler())
{
    _uiNeedToReconfigureRoutes[INPUT] = 0;
    _uiNeedToReconfigureRoutes[OUTPUT] = 0;

    _uiEnabledRoutes[INPUT] = 0;
    _uiEnabledRoutes[OUTPUT] = 0;

    _uiPreviousEnabledRoutes[INPUT] = 0;
    _uiPreviousEnabledRoutes[OUTPUT] = 0;

    /// Connector
    // Fetch the name of the PFW configuration file: this name is stored in an Android property
    // and can be different for each hardware
    string strParameterConfigurationFilePath = TProperty<string>(mPFWConfigurationFileNamePropertyName,
                                                                 "/etc/parameter-framework/ParameterFrameworkConfiguration.xml");
    LOGI("parameter-framework: using configuration file: %s", strParameterConfigurationFilePath.c_str());

    // Actually create the Connector
    _pParameterMgrPlatformConnector = new CParameterMgrPlatformConnector(strParameterConfigurationFilePath);

    // Logger
    _pParameterMgrPlatformConnector->setLogger(_pParameterMgrPlatformConnectorLogger);

    /// Criteria Types
    for (int i = 0; i < ENbCriteriaType; i++) {

        _apCriteriaTypeInterface[i] = createAndFillSelectionCriterionType((CriteriaType)i);
    }

    /// Criteria
    for (int i = 0; i < ENbCriteria; i++) {

        _apSelectedCriteria[i] = _pParameterMgrPlatformConnector->createSelectionCriterion(_apCriteriaInterface[i].pcName,
                                                                  _apCriteriaTypeInterface[_apCriteriaInterface[i].eCriteriaType]);
    }

    createAudioHardwarePlatform();

    _bFmSupported = TProperty<bool>(mFmSupportedPropName, mFmSupportedDefaultValue);
    _bFmIsAnalog = TProperty<bool>(mFmIsAnalogPropName, mFmIsAnalogDefaultValue);
    if (_bFmSupported) {

        if (_bFmIsAnalog) {

            ALOGE("Cannot load FM HW Module");
            _uiFmRxSpeakerMaxVolumeValue = getIntegerParameterValue(gapcDefaultFmRxMaxVolume[FM_RX_SPEAKER], DEFAULT_FM_RX_VOL_MAX);
            _uiFmRxHeadsetMaxVolumeValue = getIntegerParameterValue(gapcDefaultFmRxMaxVolume[FM_RX_HEADSET], DEFAULT_FM_RX_VOL_MAX);
        }
    }

    //Check if platform supports Bluetooth HFP
    _bBluetoothHFPSupported = TProperty<bool>(mBluetoothHFPSupportedPropName, mBluetoothHFPSupportedDefaultValue);
    if(_bBluetoothHFPSupported){

        ALOGI("%s(): platform supports Bluetooth HFP", __FUNCTION__);
    } else {

        ALOGI("%s(): platform does NOT support Bluetooth HFP", __FUNCTION__);
    }

    // Client wait semaphore
    bzero(&_clientWaitSemaphore, sizeof(_clientWaitSemaphore));
    sem_init(&_clientWaitSemaphore, 0, 0);

}

status_t CAudioRouteManager::start()
{
    assert(!_bIsStarted);

    status_t status = NO_ERROR;
    _bIsStarted = true;

    // Start Event thread
    _pEventThread->start();

#ifndef VB_HAL_AUDIO_TEMP
    // Start AT Manager
    if(_bHaveModem){

        startATManager();
    }
#endif

    // Start PFW
    std::string strError;
    if (!_pParameterMgrPlatformConnector->start(strError)) {

        ALOGE("parameter-framework start error: %s", strError.c_str());
        status = NO_INIT;
    } else {

        ALOGI("parameter-framework successfully started!");
    }

    return status;
}

// From AudioHardwareALSA, set FM mode
// must be called with AudioHardwareALSA::mLock held
void CAudioRouteManager::setFmRxMode(int fmMode)
{
    AutoW lock(mLock);

    // Update the platform state: fmmode
    _pPlatformState->setFmRxMode(fmMode);

    ALOGD("-------------------------------------------------------------------------------------------------------");
    ALOGD("%s: Reconsider Routing due to FM Mode change", __FUNCTION__);
    ALOGD("-------------------------------------------------------------------------------------------------------");
    //
    // SYNCHRONOUSLY RECONSIDERATION of the routing in case of stream start
    //
    reconsiderRouting();
}

// From AudioHardwareALSA, set TTY mode
void CAudioRouteManager::setTtyDirection(int iTtyDirection)
{
    _pPlatformState->setTtyDirection(iTtyDirection);
}

// From AudioHardwareALSA, set HAC mode
void CAudioRouteManager::setHacMode(bool bEnabled)
{
    _pPlatformState->setHacMode(bEnabled);
}

// From AudioHardwareALSA, set BT_NREC
void CAudioRouteManager::setBtNrEc(bool bIsAcousticSupportedOnBT)
{
    _pPlatformState->setBtHeadsetNrEc(bIsAcousticSupportedOnBT);
}

// From AudioHardwareALSA, set BT_NREC
void CAudioRouteManager::setBtEnable(bool bIsBtEnabled)
{
    _pPlatformState->setBtEnabled(bIsBtEnabled);
}

CAudioRouteManager::~CAudioRouteManager()
{
    // Semaphores
    sem_destroy(&_clientWaitSemaphore);

    // Delete Modem Audio Manager
    delete _pModemAudioManager;

    RouteListIterator it;

    // Delete all routes
    for (it = _routeList.begin(); it != _routeList.end(); ++it) {

        delete *it;
    }

    // Unset logger
    _pParameterMgrPlatformConnector->setLogger(NULL);
    // Remove logger
    delete _pParameterMgrPlatformConnectorLogger;
    // Remove connector
    delete _pParameterMgrPlatformConnector;
    // Remove parameter handler
    delete _pAudioParameterHandler;
}

//
// Must be called from WLocked context
//
status_t CAudioRouteManager::reconsiderRouting(bool bIsSynchronous)
{
    ALOGD("%s", __FUNCTION__);

    assert(_bStarted && !_pEventThread->inThreadContext());

    status_t eStatus = NO_ERROR;

    if (_bClientWaiting) {

        eStatus = PERMISSION_DENIED;
        goto end;
    }

    if (bIsSynchronous) {

        // Set the client wait sema flag
        _bClientWaiting = true;
    }


    // Trig the processing of the list
    _pEventThread->trig();

    if (!bIsSynchronous) {

        return eStatus;
    }

    // Wait
    sem_wait(&_clientWaitSemaphore);

    // Consume
    _bClientWaiting = false;

end:

    ALOGD("%s: DONE", __FUNCTION__);
    return eStatus;
}

//
// From worker thread context
// This function requests to evaluate the routing for all the streams
// after a mode change, a modem event ...
// must be called with AudioHardwareALSA::mLock held
//
void CAudioRouteManager::doReconsiderRouting()
{
    ALOGD("%s: following conditions:", __FUNCTION__);
    ALOGD("%s:          -Modem Alive = %d %s", __FUNCTION__,
          _pPlatformState->isModemAlive(),
          _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EModemStateChange)? "[has changed]" : "");
    ALOGD("%s:          -Modem Call Active = %d %s", __FUNCTION__,
          _pPlatformState->isModemAudioAvailable(),
          _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EModemAudioStatusChange)? "[has changed]" : "");
    ALOGD("%s:          -Is Shared I2S glitch free=%d %s", __FUNCTION__, _pPlatformState->isSharedI2SBusAvailable(),
          _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::ESharedI2SStateChange)? "[has changed]" : "");
    ALOGD("%s:          -Android Telephony Mode = %s %s", __FUNCTION__,
          print_criteria(_pPlatformState->getMode(), EModeCriteriaType).c_str(),
          _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EAndroidModeChange)? "[has changed]" : "");
    ALOGD("%s:          -RTE MGR HW Mode = %s %s", __FUNCTION__,
          print_criteria(_pPlatformState->getHwMode(), EModeCriteriaType).c_str(),
          _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EHwModeChange)? "[has changed]" : "");
    ALOGD("%s:          -Android FM Mode = %s %s", __FUNCTION__,
          print_criteria(_pPlatformState->getFmRxHwMode(), EFmModeCriteriaType).c_str(),
          _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EFmHwModeChange)? "[has changed]" : "");
    ALOGD("%s:          -RTE MGR FM HW Mode = %s %s", __FUNCTION__,
          print_criteria(_pPlatformState->getFmRxMode(), EFmModeCriteriaType).c_str(),
          _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EFmModeChange)? "[has changed]" : "");
    ALOGD("%s:          -BT Enabled = %d %s", __FUNCTION__, _pPlatformState->isBtEnabled(),
          _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EBtEnableChange)? "[has changed]" : "");
    ALOGD("%s:          -Platform output device = %s %s", __FUNCTION__,
          print_criteria(_pPlatformState->getDevices(true), EOutputDeviceCriteriaType).c_str(),
          _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EOutputDevicesChange)? "[has changed]" : "");
    ALOGD("%s:          -Platform input device = %s %s", __FUNCTION__,
          print_criteria(_pPlatformState->getDevices(false), EInputDeviceCriteriaType).c_str(),
          _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EInputDevicesChange)? "[has changed]" : "");
    ALOGD("%s:          -Platform input source = %s %s", __FUNCTION__,
          print_criteria(_pPlatformState->getInputSource(), EInputSourceCriteriaType).c_str(),
          _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EInputSourceChange)? "[has changed]" : "");
    ALOGD("%s:          -Platform Band type = %s %s", __FUNCTION__,
          print_criteria(_pPlatformState->getBandType(), EBandCriteriaType).c_str(),
          _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EBandTypeChange)? "[has changed]" : "");
    ALOGD("%s:          -Platform Has Direct Stream = %s %s", __FUNCTION__,
          _pPlatformState->hasDirectStreams()? "yes" : "no",
          _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EStreamEvent)? "[has changed]" : "");
    ALOGD("%s:          -Platform TTY direction = %s %s", __FUNCTION__,
          print_criteria(_pPlatformState->getTtyDirection(), ETtyDirectionCriteriaType).c_str(),
          _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::ETtyDirectionChange)? "[has changed]" : "");
    ALOGD("%s:          -Platform Direct HAC Mode = %s %s", __FUNCTION__,
          print_criteria(_pPlatformState->isHacEnabled(), ETtyDirectionCriteriaType).c_str(),
          _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EHacModeChange)? "[has changed]" : "");


    // Reset availability of all route (All routes to be available)
    resetAvailability();

    // Parse all streams and affect route to it according to applicability of the route
    bool bRoutesHaveChanged = virtuallyConnectRoutes();

    ALOGD("%s:          -Previously Enabled Route in Input = %s", __FUNCTION__,
          print_criteria(_uiPreviousEnabledRoutes[INPUT], ERouteCriteriaType).c_str());
    ALOGD("%s:          -Previously Enabled Route in Output = %s", __FUNCTION__,
          print_criteria(_uiPreviousEnabledRoutes[OUTPUT], ERouteCriteriaType).c_str());
    ALOGD("%s:          -Selected Route in Input = %s", __FUNCTION__,
          print_criteria(_uiEnabledRoutes[INPUT], ERouteCriteriaType).c_str());
    ALOGD("%s:          -Selected Route in Output = %s", __FUNCTION__,
          print_criteria(_uiEnabledRoutes[OUTPUT], ERouteCriteriaType).c_str());
    ALOGD("%s:          -Route that need reconfiguration in Input = %s", __FUNCTION__,
          print_criteria(_uiNeedToReconfigureRoutes[INPUT], ERouteCriteriaType).c_str());
    ALOGD("%s:          -Route that need reconfiguration in Output = %s", __FUNCTION__,
          print_criteria(_uiNeedToReconfigureRoutes[OUTPUT], ERouteCriteriaType).c_str());

    if (bRoutesHaveChanged) {
        for (int iRouteStage = 0; iRouteStage < ENbRoutingStage; iRouteStage++) {

            ALOGD("---------------------------------------------------------------------------------------");
            ALOGD("\t\t Routing Stage = %s",
                  print_criteria(iRouteStage, ERoutingStageCriteriaType).c_str());
            ALOGD("---------------------------------------------------------------------------------------");
            executeRoutingStage(iRouteStage);
        }
    }

    // Clear Platform State flag
    _pPlatformState->clearPlatformStateEvents();

    // Is a client waiting for routing reconsideration?
    if (_bClientWaiting) {

        // Warn client
        sem_post(&_clientWaitSemaphore);
    }

    ALOGD("-------------------------------------------------------------------------------------------------------");
    ALOGD("%s: End of Routing Reconsideration", __FUNCTION__);
    ALOGD("-------------------------------------------------------------------------------------------------------");
}

bool CAudioRouteManager::isStarted() const
{
    return _pParameterMgrPlatformConnector && _pParameterMgrPlatformConnector->isStarted();
}

void CAudioRouteManager::executeRoutingStage(int iRouteStage)
{
    //
    // Set the routing stage criteria - Do not apply the configuration now...
    //
    if (_pParameterMgrPlatformConnector->isStarted()) {

        // Warn PFW
        _apSelectedCriteria[ESelectedRoutingStage]->setCriterionState(iRouteStage);
    }

    switch(iRouteStage) {

    case EMute:
        muteRoutingStage();
        break;
    case EDisable:
        disableRoutingStage();
        break;
    case EConfigure:
        configureRoutingStage();
        break;
    case EEnable:
        enableRoutingStage();
        break;
    case EUnmute:
        unmuteRoutingStage();
        break;
    default:
        break;
    }
}

//
// From ALSA Stream In / Out via AudiohardwareALSA
//
status_t CAudioRouteManager::setStreamParameters(ALSAStreamOps* pStream, const String8 &keyValuePairs, int iMode)
{
    AutoW lock(mLock);

    AudioParameter param = AudioParameter(keyValuePairs);
    status_t status;
    bool bForOutput = pStream->isOut();

    /// Stream Flags
    int iFlags;
    String8 key = String8(AudioParameter::keyStreamFlags);
    status = param.getInt(key, iFlags);
    if (status == NO_ERROR) {

        // Remove parameter
        param.remove(key);

        setStreamFlags(pStream, iFlags);
    }

    /// Devices (ie routing key)
    int devices;
    key = String8(AudioParameter::keyRouting);
    status = param.getInt(key, devices);

    if (status == NO_ERROR) {

        // Remove parameter
        param.remove(key);

        // Ask the route manager to update the stream devices
        setDevices(pStream, devices);

        // Retrieve if provide, the input source for an input stream
        if (!bForOutput) {

            key = String8(AudioParameter::keyInputSource);

            /// Get concerned input source
            int inputSource;
            status = param.getInt(key, inputSource);

            if (status == NO_ERROR) {

                // Remove parameter
                param.remove(key);

                // Found the input source
                setInputSource(pStream, inputSource);
            }
            if (devices == 0) {

                // When this function is called with a null device, considers it as
                // an unrouting request, restore source to default within route manager
                setInputSource(pStream, AUDIO_SOURCE_DEFAULT);
            }
        } else {

            // For output streams, latch Android Mode
            _pPlatformState->setMode(iMode);
        }
    }

    /// Process pending platform changes
    if (_pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EOutputDevicesChange | CAudioPlatformState::EInputDevicesChange |
                                                 CAudioPlatformState::EStreamEvent | CAudioPlatformState::EInputSourceChange |
                                                 CAudioPlatformState::EAndroidModeChange | CAudioPlatformState::EHwModeChange |
                                                 CAudioPlatformState::EFmModeChange | CAudioPlatformState::EFmHwModeChange)) {

        ALOGD("-------------------------------------------------------------------------------------------------------");
        ALOGD("%s: Reconsider Routing due to %s stream parameter change",
              __FUNCTION__,
              pStream->isOut()? "output": "input");
        ALOGD("-------------------------------------------------------------------------------------------------------");
        // Reconsider the routing now
        status = reconsiderRouting();
        if (status != NO_ERROR) {

            // Just log!
            ALOGE("alsa route error: %d", status);
        }

    } else {

        ALOGD("-------------------------------------------------------------------------------------------------------");
        ALOGD("%s: Nothing to do, identical Platform State, bailing out", __FUNCTION__);
        ALOGD("-------------------------------------------------------------------------------------------------------");
    }

    // No more?
    if (param.size()) {

        // Just log!
        ALOGW("Unhandled argument.");
    }

    return status;
}

//
// From ALSA Stream In / Out via ALSAStreamOps
//
status_t CAudioRouteManager::startStream(ALSAStreamOps *pStream)
{
    AutoW lock(mLock);

    if (pStream->isStarted()){

        // bailing out
        return NO_ERROR;
    }
    pStream->setStarted(true);

    ALOGD("-------------------------------------------------------------------------------------------------------");
    ALOGD("%s: Reconsider Routing due to %s stream start event",
          __FUNCTION__,
          pStream->isOut()? "output" : "input");
    ALOGD("-------------------------------------------------------------------------------------------------------");
    //
    // SYNCHRONOUSLY RECONSIDERATION of the routing in case of stream start
    //
    return reconsiderRouting();
}

//
// From ALSA Stream In / Out via ALSAStreamOps
//
status_t CAudioRouteManager::stopStream(ALSAStreamOps* pStream)
{
    AutoW lock(mLock);

    if (!pStream->isStarted()){

        // bailing out
        return NO_ERROR;
    }
    pStream->setStarted(false);

    ALOGD("-------------------------------------------------------------------------------------------------------");
    ALOGD("%s: Reconsider Routing due to %s stream stop event",
          __FUNCTION__,
          pStream->isOut()? "output" : "input");
    ALOGD("-------------------------------------------------------------------------------------------------------");
    //
    // SYNCHRONOUSLY RECONSIDERATION of the routing in case of stream start
    //
    return reconsiderRouting();
}

//
// Called from AudioHardwareALSA
//
status_t CAudioRouteManager::setParameters(const String8& keyValuePairs)
{
    AutoW lock(mLock);

    AudioParameter param = AudioParameter(keyValuePairs);
    status_t status;
    String8 strRst;

    String8 key = String8("restarting");
    status = param.get(key, strRst);
    if (status == NO_ERROR) {

        if (strRst == "true") {

            // Restore the audio parameters when mediaserver is restarted in case of crash.
            ALOGI("Restore audio parameters as mediaserver is restarted param=%s", _pAudioParameterHandler->getParameters().string());
            doSetParameters(_pAudioParameterHandler->getParameters());
        }
        param.remove(key);
    } else {

        // Set the audio parameters
        status = doSetParameters(keyValuePairs);
        if (status == NO_ERROR) {

            ALOGD("%s: saving %s", __FUNCTION__, keyValuePairs.string());
            // Save the audio parameters for recovering audio parameters in case of crash.
            _pAudioParameterHandler->saveParameters(keyValuePairs);
        } else {

            return status;
        }
    }
    return NO_ERROR;
}


status_t CAudioRouteManager::doSetParameters(const String8& keyValuePairs)
{
    AutoW lock(mLock);

    ALOGD("%s: key value pair %s", __FUNCTION__, keyValuePairs.string());

    AudioParameter param = AudioParameter(keyValuePairs);
    status_t status;

    //
    // Search Stream Flags
    //
    int iFlags;
    String8 key = String8(AudioParameter::keyStreamFlags);
    status = param.getInt(key, iFlags);
    if (status == NO_ERROR) {

        // Remove parameter
        param.remove(key);

        _pPlatformState->setDirectStreamEvent(iFlags);
    }

    //
    // Search TTY mode
    //
    String8 strTtyDevice;
    int iTtyDirection = 0;
    key = String8(AUDIO_PARAMETER_KEY_TTY_MODE);

    // Get concerned devices
    status = param.get(key, strTtyDevice);

    if (status == NO_ERROR) {

        if(strTtyDevice == AUDIO_PARAMETER_VALUE_TTY_FULL) {
            ALOGV("tty full\n");
            iTtyDirection = TTY_DOWNLINK | TTY_UPLINK;
        }
        else if(strTtyDevice == AUDIO_PARAMETER_VALUE_TTY_HCO) {
            ALOGV("tty hco\n");
            iTtyDirection = TTY_UPLINK;
        }
        else if(strTtyDevice == AUDIO_PARAMETER_VALUE_TTY_VCO) {
            ALOGV("tty vco\n");
            iTtyDirection = TTY_DOWNLINK;
        }
        else if (strTtyDevice == AUDIO_PARAMETER_VALUE_TTY_OFF) {
            ALOGV("tty off\n");
            iTtyDirection = 0;

        }

        setTtyDirection(iTtyDirection);

        // Remove parameter
        param.remove(key);
    }

    //
    // Search BT STATE parameter
    //
    String8 strBtState;
    key = String8(AUDIO_PARAMETER_KEY_BLUETOOTH_STATE);

    // Get concerned devices
    status = param.get(key, strBtState);
    if(status == NO_ERROR)
    {
        bool bIsBtEnabled = false;
        if (strBtState == AUDIO_PARAMETER_VALUE_BLUETOOTH_STATE_ON) {

            LOGV("bt enabled\n");
            bIsBtEnabled = true;
        }
        else {

            LOGV("bt disabled\n");
            bIsBtEnabled = false;
        }
        setBtEnable(bIsBtEnabled);
        // Remove parameter
        param.remove(key);
    }

    // Search BT NREC parameter
    // Because BT_NREC feature only supported on Bluetooth HFP support device,
    // this request should be ignored on non supported devices like tablet to avoid "non acoustic" audience profile by default.
    if(_bBluetoothHFPSupported) {

        String8 strBTnRecSetting;
        key = String8(AUDIO_PARAMETER_KEY_BT_NREC);

        // Get BT NREC setting value
        status = param.get(key, strBTnRecSetting);
        bool isBtNRecAvailable = false;

        if (status == NO_ERROR) {
            if(strBTnRecSetting == AUDIO_PARAMETER_VALUE_ON) {
                LOGV("BT NREC on, headset is without noise reduction and echo cancellation algorithms");
                isBtNRecAvailable = false;
            }
            else if(strBTnRecSetting == AUDIO_PARAMETER_VALUE_OFF) {
                LOGV("BT NREC off, headset is with noise reduction and echo cancellation algorithms");
                isBtNRecAvailable = true;
                // We reconsider routing as BT_NREC_ON intent is sent first, then setStreamParameters and finally
                // BT_NREC_OFF when the SCO link is enabled. But only BT_NREC_ON setting is applied in that
                // context, resulting in loading the wrong Audience profile for BT SCO. This is where reconsiderRouting
                // becomes necessary, to be aligned with BT_NREC_OFF to process the correct Audience profile.
            }

            setBtNrEc(isBtNRecAvailable);


            // Remove parameter
            param.remove(key);
        }
    }

    // Search HAC setting
    String8 strHACSetting;
    bool bIsHACModeSet = false;

    key = String8(AUDIO_PARAMETER_KEY_HAC_SETTING);

    // Get HAC setting value
    status = param.get(key, strHACSetting);

    if (status == NO_ERROR) {
        if(strHACSetting == AUDIO_PARAMETER_VALUE_HAC_ON) {
            LOGV("HAC setting is turned on, enable output on HAC device");
            bIsHACModeSet = true;
        }
        else if(strHACSetting == AUDIO_PARAMETER_VALUE_HAC_OFF) {
            LOGV("HAC setting is turned off");
            bIsHACModeSet = false;
        }

        setHacMode(bIsHACModeSet);

        // Remove parameter
        param.remove(key);
    }

    // Reconsider the routing now
    if (_pPlatformState->hasPlatformStateChanged()) {

        //
        // ASYNCHRONOUSLY RECONSIDERATION of the routing in case of a parameter change
        //
        ALOGD("-------------------------------------------------------------------------------------------------------");
        ALOGD("%s: Reconsider Routing due to External parameter change",
              __FUNCTION__);
        ALOGD("-------------------------------------------------------------------------------------------------------");
        reconsiderRouting(false);
    }

    // Not a problem if a key does not exist, its value will
    // simply not be read and used, thus return NO_ERROR
    return NO_ERROR;
}

//
// Update devices attribute of the stream
//
void CAudioRouteManager::setDevices(ALSAStreamOps* pStream, uint32_t devices)
{
    AutoW lock(mLock);

    bool bIsOut = pStream->isOut();

    if (!bIsOut) {

        devices = REMOVE_32_BITS_MSB(devices);
    }

    // Update Platform state: in/out devices
    _pPlatformState->setDevices(devices, bIsOut);

    ALOGD("%s: set device = %s to %s stream", __FUNCTION__,
         bIsOut ?\
             print_criteria(devices, EOutputDeviceCriteriaType).c_str() :\
             print_criteria(devices, EInputDeviceCriteriaType).c_str(),
        bIsOut ? "output": "input");

    // set the new device for this stream
    pStream->setNewDevice(devices);
}

//
// From AudioInStreams: update the active input source
// Assumption: only one active input source at one time.
// Does it make sense to keep it in the platform state???
//
void CAudioRouteManager::setInputSource(ALSAStreamOps* pStream, int iInputSource)
{
    AutoW lock(mLock);

    pStream->setInputSource(iInputSource);

    ALOGD("%s: inputSource = %s", __FUNCTION__,
         print_criteria(iInputSource, EInputSourceCriteriaType).c_str());
    _pPlatformState->setInputSource(iInputSource);
}

void CAudioRouteManager::setStreamFlags(android_audio_legacy::ALSAStreamOps *pStream, uint32_t uiFlags)
{
    uint32_t uiPreviousFlags = (uint32_t)pStream->getFlags();
    pStream->setFlags((audio_output_flags_t)uiFlags);

    ALOGD("%s: output flags = 0x%X (Prev Flags=0x%X)", __FUNCTION__, uiFlags, uiPreviousFlags);
}

void CAudioRouteManager::createsRoutes()
{
    for (uint32_t i = 0; i < CAudioPlatformHardware::getNbRoutes(); i++) {

        CAudioRoute* pRoute = CAudioPlatformHardware::createAudioRoute(i, _pPlatformState);

        if (!pRoute) {

            ALOGE("%s: could not find routeIndex %d", __FUNCTION__, i);
            return ;
        }
        ALOGD("%s: add %s (Index=%d) to route manager", __FUNCTION__, pRoute->getName().c_str(), i);

        _routeList.push_back(pRoute);

        // Add ports to the route
        uint32_t uiPortsUsed = CAudioPlatformHardware::getPortsUsedByRoute(i);

        ALOGD("%s: uiPortsUsed=0x%X",  __FUNCTION__, uiPortsUsed);

        assert (popCount(uiPortsUsed) <= 2);

        for (uint32_t uiPort = 0; uiPort < CAudioPlatformHardware::getNbPorts(); uiPort++) {

            uint32_t uiPortId = CAudioPlatformHardware::getPortId(uiPort);
            if (uiPortsUsed & uiPortId) {

                pRoute->addPort(findPortById(uiPortId));
            }
        }
    }

}

void CAudioRouteManager::createAudioHardwarePlatform()
{
    AutoW lock(mLock);

    ALOGD("%s", __FUNCTION__);

    for (uint32_t i = 0; i < CAudioPlatformHardware::getNbPorts(); i++) {

        addPort(i);
    }

    for (uint32_t i = 0; i < CAudioPlatformHardware::getNbPortGroups(); i++) {

        addPortGroup(i);
    }

    createsRoutes();
}


void CAudioRouteManager::addPort(uint32_t uiPortIndex)
{
    ALOGD("%s", __FUNCTION__);

    CAudioPort* pPort = new CAudioPort(uiPortIndex);

    if (!pPort) {

        return ;
    }
    _portList.push_back(pPort);
}

void CAudioRouteManager::addPortGroup(uint32_t uiPortGroupIndex)
{
    ALOGD("%s", __FUNCTION__);

    CAudioPortGroup* pAudioPortGroup = new CAudioPortGroup(uiPortGroupIndex);

    if (!pAudioPortGroup) {

        return ;
    }

    _portGroupList.push_back(pAudioPortGroup);

    for (uint32_t i = 0; i < CAudioPlatformHardware::getNbPorts(); i++) {

        if (CAudioPlatformHardware::getPortsUsedByPortGroup(uiPortGroupIndex) & CAudioPlatformHardware::getPortId(i)) {

            pAudioPortGroup->addPortToGroup(findPortById(CAudioPlatformHardware::getPortId(i)));
        }
    }
}

//
// This functionr resets the availability of all routes:
// ie it resets both condemnation and borrowed flags.
//
void CAudioRouteManager::resetAvailability()
{
    ALOGD("%s", __FUNCTION__);

    CAudioRoute *aRoute =  NULL;
    RouteListIterator it;

    // Find the applicable route for this route request
    for (it = _routeList.begin(); it != _routeList.end(); ++it) {

        aRoute = *it;
        aRoute->resetAvailability();
    }

    // Reset Port availability
    PortListIterator itport;

    // Find the applicable route for this route request
    for (itport = _portList.begin(); itport != _portList.end(); ++itport) {

        CAudioPort* aPort = *itport;
        aPort->resetAvailability();
    }
}

//
// Add a stream to route manager
//
void CAudioRouteManager::addStream(ALSAStreamOps* pStream)
{
    AutoW lock(mLock);

    bool isOut = pStream->isOut();

    ALOGD("%s: add %s stream to route manager", __FUNCTION__, isOut? "output" : "input");

    // Add Stream Out to the list
    _streamsList[isOut].push_back(pStream);
}

//
// Remove a stream from route manager
//
void CAudioRouteManager::removeStream(ALSAStreamOps* pStream)
{
    AutoW lock(mLock);

    ALOGD("%s", __FUNCTION__);

    ALSAStreamOpsListIterator it;

    bool isOut = pStream->isOut();

    for (it = _streamsList[isOut].begin(); it != _streamsList[isOut].end(); ++it) {

        ALSAStreamOps* pOps = *it;

        if (pOps == pStream) {

            // Remove element
            _streamsList[isOut].erase(it);

            // Done
            break;
        }
    }
}

//
// Returns true if the routing scheme has changed, false otherwise
//
bool CAudioRouteManager::virtuallyConnectRoutes()
{
    ALOGD("---------------------------------------------------------------------------------------");
    ALOGD("\t\t %s", __FUNCTION__);
    ALOGD("---------------------------------------------------------------------------------------");

    // Return true if any changes observed routes (input or output direction)
    return virtuallyConnectRoutes(OUTPUT) | virtuallyConnectRoutes(INPUT);
}

//
// This function virtually connects all streams to their
// new route according to:
//     -applicability of the route
//     -availability of the route
//
// Returns true if previous enabled route is different from current enabled route
//              or if any route needs to be reconfigured.
//         false otherwise (the list of enabled route did not change, no route
//              needs to be reconfigured)
//
bool CAudioRouteManager::virtuallyConnectRoutes(bool bIsOut)
{
    ALOGD("%s for %s", __FUNCTION__,
          bIsOut? "output" : "input");

    // Save Enabled routes bit field
    _uiPreviousEnabledRoutes[bIsOut] = _uiEnabledRoutes[bIsOut];

    // Reset Enabled Routes
    _uiEnabledRoutes[bIsOut] = 0;

    // Reset Need reconfiguration Routes
    _uiNeedToReconfigureRoutes[bIsOut] = 0;

    // Go through the list of routes
    RouteListIterator it;

    // Find the applicable route for this route request
    for (it = _routeList.begin(); it != _routeList.end(); ++it) {

        CAudioRoute *pRoute =  *it;

        virtuallyConnectRoute(pRoute, bIsOut);

    }
    if ((_uiPreviousEnabledRoutes[bIsOut] == _uiEnabledRoutes[bIsOut]) && !_uiNeedToReconfigureRoutes[bIsOut]) {

        return false;
    }
    return true;
}

void CAudioRouteManager::virtuallyConnectRoute(CAudioRoute* pRoute, bool bIsOut)
{
    uint32_t uiDevices = _pPlatformState->getDevices(bIsOut);
    int iMode = _pPlatformState->getHwMode();

    //
    // First check if the route has slaves routes to evaluate
    // slave routes first
    //
    uint32_t uiSlaveRoutes = pRoute->getSlaveRoute();
    if (uiSlaveRoutes) {

        // Go through the list of routes
        RouteListIterator it;
        int iOneSlaveAtLeastEnabled = 0;

        // Find the applicable route for this route request
        for (it = _routeList.begin(); it != _routeList.end(); ++it) {

            CAudioRoute *pSlaveRoute =  *it;
            uint32_t uiRouteId = pSlaveRoute->getRouteId();

            if (uiSlaveRoutes & uiRouteId) {

                virtuallyConnectRoute(pSlaveRoute, bIsOut);
                iOneSlaveAtLeastEnabled += pSlaveRoute->willBeBorrowed(bIsOut);
            }
        }
        if (!iOneSlaveAtLeastEnabled) {

            return ;
        }
    }

    if (pRoute->getRouteType() == CAudioRoute::EExternalRoute || pRoute->getRouteType() == CAudioRoute::ECompressedStreamRoute) {

        if (pRoute->isApplicable(uiDevices, iMode, bIsOut)) {

            ALOGD("%s: route %s is applicable [mode = %s devices= %s]", __FUNCTION__,
                  pRoute->getName().c_str(),
                  print_criteria(iMode, EModeCriteriaType).c_str(),
                  bIsOut ?\
                      print_criteria(uiDevices, EOutputDeviceCriteriaType).c_str() :\
                      print_criteria(uiDevices, EInputDeviceCriteriaType).c_str());
            // Route is not condemned -> its port are now borrowed by this ext route
            // It will automatically condemn all mutual exclusive port of
            // the port used by this route
            pRoute->setBorrowed(bIsOut);
            // Add route to enabled route bit field
            _uiEnabledRoutes[bIsOut] |= pRoute->getRouteId();
        }

    } else if (pRoute->getRouteType() == CAudioRoute::EStreamRoute) {

        ALSAStreamOps* pStreamOps;

        // Check if a stream route is applicable for this stream
        pStreamOps = findApplicableStreamForRoute(bIsOut, pRoute);

        if (pStreamOps) {

            CAudioStreamRoute* pStreamRoute = static_cast<CAudioStreamRoute*>(pRoute);
            pStreamRoute->setStream(pStreamOps);

            pRoute->setBorrowed(bIsOut);

            // Add route to enabled route bit field
            _uiEnabledRoutes[bIsOut] |= pRoute->getRouteId();
        }
    }

    if (pRoute->needReconfiguration(bIsOut)) {

        // Add route to NeedToReconfigureRoute bit field
        _uiNeedToReconfigureRoutes[bIsOut] |= pRoute->getRouteId();
    }
}

ALSAStreamOps* CAudioRouteManager::findApplicableStreamForRoute(bool bIsOut, CAudioRoute* pRoute)
{
    ALSAStreamOpsListIterator it;

    for (it = _streamsList[bIsOut].begin(); it != _streamsList[bIsOut].end(); ++it) {

        ALSAStreamOps* pOps = *it;
        assert(isOut = pOps->isOut());

        // Check stream state
        if ((pRoute->getRouteType() == CAudioRoute::EStreamRoute) && !pOps->isStarted()) {

            // Stream is not started (standby or stopped)
            // Do not affect a route to this stream (route has been resetted in the virtual disconnect)
            continue;
        }

        // Check if the route is applicable
        // Applicability will also check if this route is already borrowed or not.
        uint32_t uiFlags = (bIsOut? pOps->getFlags() : (1 << pOps->getInputSource()));
        if (pRoute->isApplicable(pOps->getNewDevice(), _pPlatformState->getHwMode(), bIsOut, uiFlags)) {

            ALOGD("%s: route %s is applicable", __FUNCTION__, pRoute->getName().c_str());

            // Got it! Bailing out
            return pOps;
        }
    }
    return NULL;
}

void CAudioRouteManager::muteRoutingStage()
{
    // Mute Routes that
    //      -are disabled
    //      -need reconfiguration
    // Routing order criterion = Mute
    muteRoutes(INPUT);
    muteRoutes(OUTPUT);

    if (_pParameterMgrPlatformConnector->isStarted()) {

        std::string strError;
        if (!_pParameterMgrPlatformConnector->applyConfigurations(strError)) {

            ALOGE("%s", strError.c_str());
        }
    }
}

void CAudioRouteManager::muteRoutes(bool bIsOut)
{
    ALOGD("%s for %s:", __FUNCTION__,
          bIsOut? "output" : "input");

    //
    // CurrentEnabledRoute: Eclipse the route that need to reconfigure
    //                      so that the PFW mutes the route that need reconfiguration
    // The logic should be explained as:
    //      RoutesToMute = (PreviousEnabledRoutes[bIsOut] & ~EnabledRoutes[bIsOut]) |
    //                      NeedToReconfigureRoutes[bIsOut];
    //
    uint32_t uiEnabledRoutes = _uiEnabledRoutes[bIsOut] & ~_uiNeedToReconfigureRoutes[bIsOut];

    ALOGD("%s \t -Previously enabled routes in %s = %s", __FUNCTION__,
             bIsOut? "Output" : "Input",
             print_criteria(_uiPreviousEnabledRoutes[bIsOut], ERouteCriteriaType).c_str());

    ALOGD("%s \t -Enabled routes [eclipsing route that need reconfiguration] in %s = %s", __FUNCTION__,
         bIsOut? "Output" : "Input",
         print_criteria(uiEnabledRoutes, ERouteCriteriaType).c_str());

    ALOGD("%s \t --------------------------------------------------------------------------", __FUNCTION__);
    ALOGD("%s \t Expected Routes to be muted in %s = %s", __FUNCTION__,
         bIsOut? "Output" : "Input",
         print_criteria(((_uiPreviousEnabledRoutes[bIsOut] & ~_uiEnabledRoutes[bIsOut]) | _uiNeedToReconfigureRoutes[bIsOut]),
                        ERouteCriteriaType).c_str());

    if (_pParameterMgrPlatformConnector->isStarted()) {

        // Warn PFW
         selectedPreviousRoute(bIsOut)->setCriterionState(_uiPreviousEnabledRoutes[bIsOut]);
         selectedRoute(bIsOut)->setCriterionState(uiEnabledRoutes);
    }
}

void CAudioRouteManager::unmuteRoutingStage()
{
    // Unmute Routes that
    //      -were not enabled but will be enabled
    //      -were previously enabled, will be enabled but need reconfiguration
    // Routing order criterion = Unmute
    unmuteRoutes(INPUT);
    unmuteRoutes(OUTPUT);

    if (_pParameterMgrPlatformConnector->isStarted()) {

        std::string strError;
        if (!_pParameterMgrPlatformConnector->applyConfigurations(strError)) {

            ALOGE("%s", strError.c_str());
        }
    }
}
void CAudioRouteManager::unmuteRoutes(bool bIsOut)
{
    ALOGD("%s for %s:", __FUNCTION__,
          bIsOut? "output" : "input");

    // Unmute Routes that
    //      -were not enabled but will be enabled
    //      -were previously enabled, will be enabled but need reconfiguration
    // Routing order criterion = Unmute
    // From a logic point of view:
    //          RoutesToUnmute = (EnabledRoutes[bIsOut] & ~PreviousEnabledRoutes[bIsOut]) |
    //                               NeedToReconfigureRoutes[bIsOut];

    ALOGD("%s \t -Previously enabled routes in %s = %s", __FUNCTION__,
         bIsOut? "Output" : "Input",
         print_criteria(_uiPreviousEnabledRoutes[bIsOut], ERouteCriteriaType).c_str());

    ALOGD("%s \t -Enabled routes  in %s = %s", __FUNCTION__,
         bIsOut? "Output" : "Input",
         print_criteria(_uiEnabledRoutes[bIsOut], ERouteCriteriaType).c_str());

    ALOGD("%s \t --------------------------------------------------------------------------", __FUNCTION__);
    ALOGD("%s \t Expected Routes to be unmuted in %s = %s", __FUNCTION__,
         bIsOut? "Output" : "Input",
         print_criteria(((_uiEnabledRoutes[bIsOut] & ~_uiPreviousEnabledRoutes[bIsOut]) |_uiNeedToReconfigureRoutes[bIsOut]),
                        ERouteCriteriaType).c_str());

    if (_pParameterMgrPlatformConnector->isStarted()) {

        // Warn PFW
        selectedPreviousRoute(bIsOut)->setCriterionState(_uiPreviousEnabledRoutes[bIsOut]);
    }
}

void CAudioRouteManager::configureRoutingStage()
{
    // PFW: Routing criterion = configure
    // Change here the devices, the mode, ... all the criteria
    // required for the routing
    configureRoutes(OUTPUT);
    configureRoutes(INPUT);

    if (_pParameterMgrPlatformConnector->isStarted()) {

        // Warn PFW
        _apSelectedCriteria[ESelectedMode]->setCriterionState(_pPlatformState->getHwMode());
        _apSelectedCriteria[ESelectedFmMode]->setCriterionState(_pPlatformState->getFmRxHwMode());
        _apSelectedCriteria[ESelectedTtyDirection]->setCriterionState(_pPlatformState->getTtyDirection());
        _apSelectedCriteria[ESelectedBtHeadsetNrEc]->setCriterionState(_pPlatformState->isBtHeadsetNrEcEnabled());
        _apSelectedCriteria[ESelectedBand]->setCriterionState(_pPlatformState->getBandType());
        _apSelectedCriteria[ESelectedHacMode]->setCriterionState(_pPlatformState->isHacEnabled());

        std::string strError;
        if (!_pParameterMgrPlatformConnector->applyConfigurations(strError)) {

            ALOGE("%s", strError.c_str());
        }
    }
}

void CAudioRouteManager::configureRoutes(bool bIsOut)
{
    ALOGD("%s for %s:", __FUNCTION__,
          bIsOut? "output" : "input");

    // Configure Routes that
    //      -were not enabled but will be enabled
    //      -were previously enabled, will be enabled but need reconfiguration
    // Routing order criterion = Configure
    // From a logic point of view:
    //      RoutesToConfigure = (EnabledRoutes[bIsOut] & ~PreviousEnabledRoutes[bIsOut]) |
    //                                NeedToReconfigureRoutes[bIsOut];

    ALOGD("%s:          -Routes to be configured in %s = %s", __FUNCTION__,
         bIsOut? "Output" : "Input",
         print_criteria(((_uiEnabledRoutes[bIsOut] & ~_uiPreviousEnabledRoutes[bIsOut]) | _uiNeedToReconfigureRoutes[bIsOut]),
                        ERouteCriteriaType).c_str());

    // PFW: Routing criterion = configure
    // Change here the devices, the mode, ... all the criteria
    // required for the routing
    if (_pParameterMgrPlatformConnector->isStarted()) {

        // Warn PFW
        selectedDevice(bIsOut)->setCriterionState(_pPlatformState->getDevices(bIsOut));
        if (!bIsOut) {

            _apSelectedCriteria[ESelectedInputSource]->setCriterionState(_pPlatformState->getInputSource());
        }
    }
}

void CAudioRouteManager::disableRoutingStage()
{
    // Disable Routes that
    //      -are disabled
    //      -need reconfiguration
    // Routing order criterion = Disable
    // starting from input streams
    disableRoutes(INPUT);
    disableRoutes(OUTPUT);

    //
    // PFW: disable route stage:
    // CurrentEnabledRoutes reflects the reality: do not disable route that need reconfiguration only
    //
    if (_pParameterMgrPlatformConnector->isStarted()) {

        std::string strError;
        if (!_pParameterMgrPlatformConnector->applyConfigurations(strError)) {

            ALOGE("%s", strError.c_str());
        }
    }
}

void CAudioRouteManager::disableRoutes(bool bIsOut)
{
    ALOGD("%s for %s:", __FUNCTION__,
          bIsOut? "output" : "input");

    // Disable Routes that
    //      -are disabled
    //      -need reconfiguration
    // Routing order criterion = Disable
    // From a logic point of view:
    //          RoutesToDisable = (PreviousEnabledRoutes[bIsOut] & ~EnabledRoutes[bIsOut]) |
    //                              NeedToReconfigureRoutes[bIsOut];

    ALOGD("%s \t -Routes to be disabled(unrouted) in %s = %s",  __FUNCTION__,
         bIsOut? "Output" : "Input",
         print_criteria((_uiPreviousEnabledRoutes[bIsOut] & ~_uiEnabledRoutes[bIsOut]) | _uiNeedToReconfigureRoutes[bIsOut],
                        ERouteCriteriaType).c_str());

    CAudioRoute *aRoute =  NULL;
    RouteListIterator it;

    //
    // Performs the unrouting on the routes
    //
    for (it = _routeList.begin(); it != _routeList.end(); ++it) {

        aRoute = *it;

        //
        // Disable Routes that
        //      -are disabled
        //      -need reconfiguration
        //
        if (aRoute->currentlyBorrowed(bIsOut) && (aRoute->needReconfiguration(bIsOut) || !aRoute->willBeBorrowed(bIsOut))) {

            aRoute->unRoute(bIsOut);
        }
    }

    //
    // PFW: disable route stage:
    // CurrentEnabledRoutes reflects the reality: do not disable route that need reconfiguration only
    //
    if (_pParameterMgrPlatformConnector->isStarted()) {

        selectedRoute(bIsOut)->setCriterionState(_uiEnabledRoutes[bIsOut]);
    }
}

void CAudioRouteManager::enableRoutingStage()
{
    //
    // Enable routes through PFW
    //
    if (_pParameterMgrPlatformConnector->isStarted()) {

        std::string strError;
        if (!_pParameterMgrPlatformConnector->applyConfigurations(strError)) {

            ALOGE("%s", strError.c_str());
        }
    }

    // Connect all streams that need to be connected (starting from output streams)
    enableRoutes(OUTPUT);
    enableRoutes(INPUT);
}

void CAudioRouteManager::enableRoutes(bool bIsOut)
{
    ALOGD("%s for %s:", __FUNCTION__,
          bIsOut? "output" : "input");

    CAudioRoute *aRoute =  NULL;
    RouteListIterator it;

    //
    // Performs the routing on the routes:
    // Enable Routes that
    //      -were not enabled but will be enabled
    //      -were previously enabled, will be enabled but need reconfiguration
    //
    // From logic point of view:
    //          RoutesToEnable = (EnabledRoutes[bIsOut] & ~PreviousEnabledRoutes[bIsOut]) |
    //                                NeedToReconfigureRoutes[bIsOut];

    ALOGD("%s \t -Routes to be enabled(routed) in %s = %s", __FUNCTION__,
         bIsOut? "Output" : "Input",
         print_criteria(((_uiEnabledRoutes[bIsOut] & ~_uiPreviousEnabledRoutes[bIsOut]) | _uiNeedToReconfigureRoutes[bIsOut]),
                        ERouteCriteriaType).c_str());

    for (it = _routeList.begin(); it != _routeList.end(); ++it) {

        aRoute = *it;

        // If the route is external and was set borrowed -> it needs to be routed
        if (aRoute->needReconfiguration(bIsOut) || (!aRoute->currentlyBorrowed(bIsOut) && aRoute->willBeBorrowed(bIsOut))) {

            if (aRoute->route(bIsOut) != NO_ERROR) {

                // Just logging
                ALOGE("\t error while routing %s", aRoute->getName().c_str());
            }
        }
    }
}

CAudioRoute* CAudioRouteManager::findRouteById(uint32_t uiRouteId)
{
    ALOGD("%s", __FUNCTION__);

    CAudioRoute* pFoundRoute =  NULL;
    RouteListIterator it;

    // Find the applicable route for this route request
    for (it = _routeList.begin(); it != _routeList.end(); ++it) {

        CAudioRoute* pRoute = *it;
        if (uiRouteId == pRoute->getRouteId()) {

            pFoundRoute = pRoute;
            break;
        }
    }
    return pFoundRoute;
}

CAudioPort* CAudioRouteManager::findPortById(uint32_t uiPortId)
{
    ALOGD("%s", __FUNCTION__);

    CAudioPort* pFoundPort =  NULL;
    PortListIterator it;

    // Find the applicable route for this route request
    for (it = _portList.begin(); it != _portList.end(); ++it) {

        CAudioPort* pPort = *it;
        if(uiPortId == pPort->getPortId()) {

            pFoundPort = pPort;
            break;
        }
    }
    return pFoundPort;
}

void CAudioRouteManager::startATManager()
{
    // Starts the ModemAudioManager
    if(_pModemAudioManager->start()) {

        ALOGE("%s: could not start ModemAudioManager", __FUNCTION__);
        return ;
    }
    ALOGE("%s: success", __FUNCTION__);
}

//
// From IModemStatusNotifier
// Need to hold AudioHardwareALSA::mLock
//
void CAudioRouteManager::onModemAudioStatusChanged()
{
    AutoW lock(mLock);

    ALOGD("%s: IN", __FUNCTION__);

    // Check if Modem Audio Path is available
    // According to network, some can receive XCALLSTAT, some can receive XPROGRESS
    // so compute both information
    bool isAudioPathAvail = _pModemAudioManager->isModemAudioAvailable();

    // Modem Call State has changed?
    if(_bModemCallActive != isAudioPathAvail){

        // ModemCallActive has changed, keep track
        _bModemCallActive = isAudioPathAvail;

        // Update the platform state
        _pPlatformState->setModemAudioAvailable(_bModemCallActive);

        // Reconsider the routing now only if the platform state has changed
        if (_pPlatformState->hasPlatformStateChanged()) {

            ALOGD("-------------------------------------------------------------------------------------------------------");
            ALOGD("%s: Reconsider Routing due to Modem Audio Status change",
                  __FUNCTION__);
            ALOGD("-------------------------------------------------------------------------------------------------------");
            status_t status = reconsiderRouting();
            if (status != NO_ERROR) {

                // Just log!
                ALOGE("alsa route error: %d", status);
            }
        }
    }
    ALOGD("%s: OUT", __FUNCTION__);
}

//
// From IModemStatusNotifier
// Called on Modem State change reported by ModemAudioManager
// Need to hold AudioHardwareALSA::mLock
//
void  CAudioRouteManager::onModemStateChanged()
{
    AutoW lock(mLock);

    ALOGD("%s: IN: ModemStatus", __FUNCTION__);

    _bModemAvailable = _pModemAudioManager->isModemAlive();

    // Update the platform state
    _pPlatformState->setModemAlive(_bModemAvailable);

    // Reset the modem audio status
    _pPlatformState->setModemAudioAvailable(false);

    // Reconsider the routing now only if the platform state has changed
    if (_pPlatformState->hasPlatformStateChanged()) {

        ALOGD("-------------------------------------------------------------------------------------------------------");
        ALOGD("%s: Reconsider Routing due to Modem State change",
              __FUNCTION__);
        ALOGD("-------------------------------------------------------------------------------------------------------");
        status_t status = reconsiderRouting();
        if (status != NO_ERROR) {

            // Just log!
            ALOGE("alsa route error: %d", status);
        }
    }
    ALOGD("%s: OUT", __FUNCTION__);
}

//
// From IModemStatusNotifier
// Called on Modem Audio PCM change report
//
void  CAudioRouteManager::onModemAudioPCMChanged() {

    MODEM_CODEC modemCodec;
    CAudioPlatformState::BandType_t band;

    AutoW lock(mLock);
    ALOGD("%s: in", __FUNCTION__);

    modemCodec = _pModemAudioManager->getModemCodec();
    if (modemCodec == CODEC_TYPE_WB_AMR_SPEECH)
        band = CAudioPlatformState::EWideBand;
    else
        band = CAudioPlatformState::ENarrowBand;

    _pPlatformState->setBandType(band);

    ALOGD("-------------------------------------------------------------------------------------------------------");
    ALOGD("%s: Reconsider Routing due to Modem Band change",
          __FUNCTION__);
    ALOGD("-------------------------------------------------------------------------------------------------------");
    status_t status = reconsiderRouting();
    if (status != NO_ERROR) {

        // Just log!
        ALOGE("%s error: %d", __FUNCTION__, status);
    }

    ALOGD("%s: out", __FUNCTION__);
}

//
// Worker thread context
// Event processing
//
bool CAudioRouteManager::onEvent(int iFd)
{
    return false;
}

//
// Worker thread context
//
bool CAudioRouteManager::onError(int iFd)
{
    return false;
}

//
// Worker thread context
//
bool CAudioRouteManager::onHangup(int iFd)
{
    return false;
}

//
// Worker thread context
//
void CAudioRouteManager::onTimeout()
{
    ALOGD("%s", __FUNCTION__);
}

//
// Worker thread context
//
void CAudioRouteManager::onPollError()
{

}

//
// Worker thread context
//
bool CAudioRouteManager::onProcess(uint16_t uiEvent)
{
    //
    // Take the lock only in case of asynchronous request
    // as already handled by client thread if synchronous request
    //
    if (!_bClientWaiting) {

        mLock.writeLock();
    }

    doReconsiderRouting();

    if (!_bClientWaiting) {

        mLock.unlock();
    }
    return false;
}

void CAudioRouteManager::lock()
{
    mLock.readLock();
}

void CAudioRouteManager::unlock()
{
    mLock.unlock();
}

// Used to fill types for PFW
ISelectionCriterionTypeInterface* CAudioRouteManager::createAndFillSelectionCriterionType(CriteriaType eCriteriaType) const
{
    uint32_t uiIndex;

    const SSelectionCriterionTypeValuePair* pSelectionCriterionTypeValuePairs = _asCriteriaType[eCriteriaType]._pCriterionTypeValuePairs;
    uint32_t uiNbEntries = _asCriteriaType[eCriteriaType]._uiNbValuePairs;
    bool bIsInclusive = _asCriteriaType[eCriteriaType]._bIsInclusive;

    ISelectionCriterionTypeInterface* pSelectionCriterionType = _pParameterMgrPlatformConnector->createSelectionCriterionType(bIsInclusive);

    for (uiIndex = 0; uiIndex < uiNbEntries; uiIndex++) {

        const SSelectionCriterionTypeValuePair* pValuePair = &pSelectionCriterionTypeValuePairs[uiIndex];
        pSelectionCriterionType->addValuePair(pValuePair->iNumerical, pValuePair->pcLiteral);
    }
    return pSelectionCriterionType;
}

uint32_t CAudioRouteManager::getIntegerParameterValue(const string& strParameterPath, uint32_t uiDefaultValue) const
{
    ALOGD("%s in", __FUNCTION__);

    if (!_pParameterMgrPlatformConnector->isStarted()) {

        return uiDefaultValue;
    }

    string strError;
    // Get handle
    CParameterHandle* pParameterHandle = _pParameterMgrPlatformConnector->createParameterHandle(strParameterPath, strError);

    if (!pParameterHandle) {

        strError = strParameterPath.c_str();
        strError += " not found!";

        ALOGE("Unable to get parameter handle: %s", strError.c_str());

        ALOGD("%s returning %d", __FUNCTION__, uiDefaultValue);

        return uiDefaultValue;
    }

    // Retrieve value
    uint32_t uiValue;

    if (!pParameterHandle->getAsInteger(uiValue, strError)) {

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

status_t CAudioRouteManager::setIntegerParameterValue(const string& strParameterPath, uint32_t uiValue)
{
    ALOGD("%s in", __FUNCTION__);

    if (!_pParameterMgrPlatformConnector->isStarted()) {

        return INVALID_OPERATION;
    }

    string strError;
    // Get handle
    CParameterHandle* pParameterHandle = _pParameterMgrPlatformConnector->createParameterHandle(strParameterPath, strError);

    if (!pParameterHandle) {

        strError = strParameterPath.c_str();
        strError += " not found!";

        ALOGE("%s: Unable to set parameter handle: %s", __FUNCTION__, strError.c_str());

        return NAME_NOT_FOUND;
    }

    // set value
    if (!pParameterHandle->setAsInteger(uiValue, strError)) {

        ALOGE("%s: Unable to set value: %s, from parameter path: %s", __FUNCTION__, strError.c_str(), strParameterPath.c_str());

        // Remove handle
        delete pParameterHandle;

        return NAME_NOT_FOUND;
    }

    // Remove handle
    delete pParameterHandle;

    ALOGD("%s returning %d", __FUNCTION__, uiValue);

    return NO_ERROR;
}

status_t CAudioRouteManager::setIntegerArrayParameterValue(const string& strParameterPath, std::vector<uint32_t>& uiArray) const
{
    ALOGD("%s in", __FUNCTION__);

    if (!_pParameterMgrPlatformConnector->isStarted()) {

        return INVALID_OPERATION;
    }

    string strError;
    // Get handle
    CParameterHandle* pParameterHandle = _pParameterMgrPlatformConnector->createParameterHandle(strParameterPath, strError);

    if (!pParameterHandle) {

        strError = strParameterPath.c_str();
        strError += " not found!";

        ALOGE("Unable to get parameter handle: %s", strError.c_str());

        return NAME_NOT_FOUND;
    }

    // set value
    if (!pParameterHandle->setAsIntegerArray(uiArray, strError)) {

        ALOGE("Unable to set value: %s, from parameter path: %s", strError.c_str(), strParameterPath.c_str());

        // Remove handle
        delete pParameterHandle;

        return INVALID_OPERATION;
    }

    // Remove handle
    delete pParameterHandle;

    return NO_ERROR;
}

status_t CAudioRouteManager::setVoiceVolume(int gain)
{
    return setIntegerParameterValue(gpcVoiceVolume, gain);
}

status_t CAudioRouteManager::setFmRxVolume(float volume)
{
    uint32_t headsetVolume = 0;
    uint32_t speakerVolume = 0;
    vector<uint32_t> pfwVolumeArray;
    //computed values (in float): {002172, 0.004660, 0.010000, 0.014877, 0.023646, 0.037584, 0.055912, 0.088869, 0.141254, 0.189453, 0.266840, 0.375838, 0.504081, 0.709987, 1.000000}
    //FM_RX_STREAM_MAX_VOLUME levels of volumes - must be aligned with MAX_STREAM_VOLUME of STREAM_FM_RX in AudioService.java
    float volumeLevels[FM_RX_STREAM_MAX_VOLUME]={0.001, 0.003, 0.005, 0.011, 0.015, 0.024, 0.04, 0.06, 0.09, 0.15, 0.2, 0.3, 0.4, 0.6, 0.8};
    uint32_t integerVolume = 0;

    AutoW lock(mLock);

    // Update volume only if FM is ON and analog
    if ((_pPlatformState->getFmRxHwMode() == AudioSystem::MODE_FM_ON) && (_bFmIsAnalog)) {

        while ((integerVolume < FM_RX_STREAM_MAX_VOLUME) && (volume > volumeLevels[integerVolume]))
        {
            integerVolume++;
        }

        // Framework forces speaker use
        if (_pPlatformState->getDevices(OUTPUT) == AudioSystem::DEVICE_OUT_SPEAKER) {
            headsetVolume = (uint32_t) (0);
            speakerVolume = (uint32_t) (integerVolume * _uiFmRxSpeakerMaxVolumeValue / FM_RX_STREAM_MAX_VOLUME);
        }
        // Else use wired accessory for FM
        else if ((_pPlatformState->getDevices(OUTPUT) & AudioSystem::DEVICE_OUT_WIRED_HEADSET) || (_pPlatformState->getDevices(OUTPUT) & AudioSystem::DEVICE_OUT_WIRED_HEADPHONE)) {
            headsetVolume = (uint32_t) (integerVolume * _uiFmRxHeadsetMaxVolumeValue / FM_RX_STREAM_MAX_VOLUME);
            speakerVolume = (uint32_t) 0;
        }

        ALOGV("%s: FM Rx volume applied on speaker %d and on headset %d", __FUNCTION__, speakerVolume, headsetVolume);
        //apply calculated volumes into audio codec
        //left and right channels of stereo headset - only one value for speaker (mono speaker)
        pfwVolumeArray.push_back(headsetVolume);
        pfwVolumeArray.push_back(headsetVolume);

        if((setIntegerArrayParameterValue(gapcLineInToHeadsetLineVolume, pfwVolumeArray) != NO_ERROR) ||
           (setIntegerParameterValue(gapcLineInToSpeakerLineVolume, (uint32_t)speakerVolume) != NO_ERROR) ||
           (setIntegerParameterValue(gapcLineInToEarSpeakerLineVolume, (uint32_t)speakerVolume)!= NO_ERROR)) {
            return INVALID_OPERATION;
        }
    }

    return NO_ERROR;
}


}       // namespace android
