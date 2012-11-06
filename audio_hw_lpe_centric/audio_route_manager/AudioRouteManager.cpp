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

#include <AudioHardwareALSA.h>
#include <ALSAStreamOps.h>
#include <AudioStreamInALSA.h>
#include "EventThread.h"
#include "AudioRouteManager.h"
#include "AudioRouteFactory.h"
#include "AudioRoute.h"
#include "AudioStreamRoute.h"
#include "AudioPlatformState.h"
#include "ParameterMgrPlatformConnector.h"
#include "SelectionCriterionInterface.h"
#include "ModemAudioManagerInstance.h"

typedef RWLock::AutoRLock AutoR;
typedef RWLock::AutoWLock AutoW;

namespace android_audio_legacy
{

const uint32_t CAudioRouteManager::_uiTimeoutSec = 2;

// Defines path to parameters in PFW XML config files
const char* const CAudioRouteManager::gapcVoicePortEnable [2] = {
    "/Audio/MSIC/SOUND_CARD/VOICE_PORT/CONFIG/PLAYBACK_ENABLED", // Type = unsigned integer
    "/Audio/MSIC/SOUND_CARD/VOICE_PORT/CONFIG/CAPTURE_ENABLED" // Type = unsigned integer
};

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
        LOGD("%s",strLog.c_str());
    }
};

// Mode type
const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioRouteManager::_stModeValuePairs[] = {
    { AudioSystem::MODE_NORMAL, "Normal" },
    { AudioSystem::MODE_RINGTONE, "RingTone" },
    { AudioSystem::MODE_IN_CALL, "InCsvCall" },
    { AudioSystem::MODE_IN_COMMUNICATION, "InVoipCall" }
};
const uint32_t CAudioRouteManager::_uiNbModeValuePairs = sizeof(CAudioRouteManager::_stModeValuePairs)/sizeof(CAudioRouteManager::_stModeValuePairs[0]);

// Selected Input Device type
const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioRouteManager::_stSelectedInputDeviceValuePairs[] = {
    { AudioSystem::DEVICE_IN_COMMUNICATION, "Communication" },
    { AudioSystem::DEVICE_IN_AMBIENT, "Ambient" },
    { AudioSystem::DEVICE_IN_BUILTIN_MIC, "Main" },
    { AudioSystem::DEVICE_IN_BLUETOOTH_SCO_HEADSET, "SCO_Headset" },
    { AudioSystem::DEVICE_IN_WIRED_HEADSET, "Headset" },
    { AudioSystem::DEVICE_IN_AUX_DIGITAL, "AuxDigital" },
    { AudioSystem::DEVICE_IN_VOICE_CALL, "VoiceCall" },
    { AudioSystem::DEVICE_IN_BACK_MIC, "Back" }
};
const uint32_t CAudioRouteManager::_uiNbSelectedInputDeviceValuePairs = sizeof(CAudioRouteManager::_stSelectedInputDeviceValuePairs)/sizeof(CAudioRouteManager::_stSelectedInputDeviceValuePairs[0]);

// Selected Output Device type
const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioRouteManager::_stSelectedOutputDeviceValuePairs[] = {
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
const uint32_t CAudioRouteManager::_uiNbSelectedOutputDeviceValuePairs = sizeof(CAudioRouteManager::_stSelectedOutputDeviceValuePairs)/sizeof(CAudioRouteManager::_stSelectedOutputDeviceValuePairs[0]);

// Routing mode
const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioRouteManager::_stRoutingModeValuePairs[] = {
    { CAudioRouteManager::EMute , "Mute" },
    { CAudioRouteManager::EDisable , "Disable" },
    { CAudioRouteManager::EConfigure , "Configure" },
    { CAudioRouteManager::EEnable , "Enable" },
    { CAudioRouteManager::EUnmute , "Unmute" }
};
const uint32_t CAudioRouteManager::_uiNbRoutingModeValuePairs = sizeof(CAudioRouteManager::_stRoutingModeValuePairs)/sizeof(CAudioRouteManager::_stRoutingModeValuePairs[0]);

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
const uint32_t CAudioRouteManager::_uiNbAudioSourceValuePairs = sizeof(CAudioRouteManager::_stAudioSourceValuePairs)/sizeof(CAudioRouteManager::_stAudioSourceValuePairs[0]);

// TTY mode
const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioRouteManager::_stTTYModeValuePairs[] = {
    { 1 , "TTY_HCO" },
    { 2 , "TTY_VCO" }
};
const uint32_t CAudioRouteManager::_uiNbTTYModeValuePairs = sizeof(CAudioRouteManager::_stTTYModeValuePairs)/sizeof(CAudioRouteManager::_stTTYModeValuePairs[0]);

// FM Mode
const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioRouteManager::_stFmModeValuePairs[] = {
    { 1 , "Off" },
    { 2 , "On" }
};
const uint32_t CAudioRouteManager::_uiNbFmModeValuePairs = sizeof(CAudioRouteManager::_stFmModeValuePairs)/sizeof(CAudioRouteManager::_stFmModeValuePairs[0]);

// Band ringing
const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioRouteManager::_stBandRingingValuePairs[] = {
    { 1 , "NetworkGenerated" },
    { 2 , "PhoneGenerated" }
};
const uint32_t CAudioRouteManager::_uiNbBandRingingValuePairs = sizeof(CAudioRouteManager::_stBandRingingValuePairs)/sizeof(CAudioRouteManager::_stBandRingingValuePairs[0]);


// HAC: TBD


// Routes
const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioRouteManager::_stSelectedRouteValuePairs[] = {
    { 1 << MEDIA , "Media" },
//    { 1 << VOICE , "Voice" },
    { 1 << HWCODEC_OIA , "HwCodec0IA" },
    { 1 << HWCODEC_1IA , "HwCodec1IA" },
    { 1 << MODEM_IA , "ModemIA" },
    { 1 << BT_IA, "BtIA" },
    { 1 << FM_IA, "FMIA" }
};
const uint32_t CAudioRouteManager::_uiNbSelectedRouteValuePairs = sizeof(CAudioRouteManager::_stSelectedRouteValuePairs)/sizeof(CAudioRouteManager::_stSelectedRouteValuePairs[0]);

const string CAudioRouteManager::print_criteria(int32_t uiValue, const SSelectionCriterionTypeValuePair* sPair, uint32_t uiNbPairs, bool bIsInclusive) const{

    string strPrint = "{";
    bool bFirst = true;
    bool bFound = false;

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
    _pParameterMgrPlatformConnector(new CParameterMgrPlatformConnector("/etc/parameter-framework/ParameterFrameworkConfiguration.xml")),
    _pParameterMgrPlatformConnectorLogger(new CParameterMgrPlatformConnectorLogger),
    _pModemAudioManager(CModemAudioManagerInstance::create(this)),
    _pPlatformState(new CAudioPlatformState(this)),
    _bModemCallActive(false),
    _bModemAvailable(false),
    _pEventThread(new CEventThread(this)),
    _bIsStarted(false),
    _bClientWaiting(false),
    _pParent(pParent)
{
    _uiNeedToReconfigureRoutes[INPUT] = 0;
    _uiNeedToReconfigureRoutes[OUTPUT] = 0;

    _uiEnabledRoutes[INPUT] = 0;
    _uiEnabledRoutes[OUTPUT] = 0;

    _uiPreviousEnabledRoutes[INPUT] = 0;
    _uiPreviousEnabledRoutes[OUTPUT] = 0;

    // Logger
    _pParameterMgrPlatformConnector->setLogger(_pParameterMgrPlatformConnectorLogger);

    /// Criteria Types
    // Mode
    _pModeType = _pParameterMgrPlatformConnector->createSelectionCriterionType();
    fillSelectionCriterionType(_pModeType, _stModeValuePairs, _uiNbModeValuePairs);

    // FM Mode
    _pFmModeType = _pParameterMgrPlatformConnector->createSelectionCriterionType();
    fillSelectionCriterionType(_pFmModeType, _stFmModeValuePairs, _uiNbFmModeValuePairs);

    // TTY Mode
    _pmTTYModeType = _pParameterMgrPlatformConnector->createSelectionCriterionType(true);
    fillSelectionCriterionType(_pmTTYModeType, _stTTYModeValuePairs, _uiNbTTYModeValuePairs);

    // Routing Mode
    _pRoutingModeType = _pParameterMgrPlatformConnector->createSelectionCriterionType();
    fillSelectionCriterionType(_pRoutingModeType, _stRoutingModeValuePairs, _uiNbRoutingModeValuePairs);

    // Route
    _pRouteType = _pParameterMgrPlatformConnector->createSelectionCriterionType(true);
    fillSelectionCriterionType(_pRouteType, _stSelectedRouteValuePairs, _uiNbSelectedRouteValuePairs);

    // InputDevice
    _pInputDeviceType = _pParameterMgrPlatformConnector->createSelectionCriterionType(true);
    fillSelectionCriterionType(_pInputDeviceType, _stSelectedInputDeviceValuePairs, _uiNbSelectedInputDeviceValuePairs);

    // OutputDevice
    _pOutputDeviceType = _pParameterMgrPlatformConnector->createSelectionCriterionType(true);
    fillSelectionCriterionType(_pOutputDeviceType, _stSelectedOutputDeviceValuePairs, _uiNbSelectedOutputDeviceValuePairs);

    // AudioSource
    _pAudioSourceType = _pParameterMgrPlatformConnector->createSelectionCriterionType();
    fillSelectionCriterionType(_pAudioSourceType, _stAudioSourceValuePairs, _uiNbAudioSourceValuePairs);

    // Band Ringing
    _pBandRingingType = _pParameterMgrPlatformConnector->createSelectionCriterionType();
    fillSelectionCriterionType(_pBandRingingType, _stBandRingingValuePairs, _uiNbBandRingingValuePairs);

    /// Criteria
    _pSelectedMode = _pParameterMgrPlatformConnector->createSelectionCriterion("Mode", _pModeType);
    _pSelectedFmMode = _pParameterMgrPlatformConnector->createSelectionCriterion("FmMode", _pFmModeType);
    _pSelectedRouteStage = _pParameterMgrPlatformConnector->createSelectionCriterion("RoutageState", _pRoutingModeType);
    _pCurrentEnabledRoutes[INPUT] = _pParameterMgrPlatformConnector->createSelectionCriterion("CurrentRouteCapture", _pRouteType);
    _pCurrentEnabledRoutes[OUTPUT] = _pParameterMgrPlatformConnector->createSelectionCriterion("CurrentRoutePlayback", _pRouteType);
    _pPreviousEnabledRoutes[INPUT] = _pParameterMgrPlatformConnector->createSelectionCriterion("PreviousRouteCapture", _pRouteType);
    _pPreviousEnabledRoutes[OUTPUT] = _pParameterMgrPlatformConnector->createSelectionCriterion("PreviousRoutePlayback", _pRouteType);

    _pSelectedAudioSource = _pParameterMgrPlatformConnector->createSelectionCriterion("AudioSource", _pAudioSourceType);

    _pSelectedTTYMode = _pParameterMgrPlatformConnector->createSelectionCriterion("TTYMode", _pmTTYModeType);

    _pSelectedBandRinging = _pParameterMgrPlatformConnector->createSelectionCriterion("BandRinging", _pBandRingingType);

    _pSelectedDevice[INPUT] = _pParameterMgrPlatformConnector->createSelectionCriterion("SelectedInputDevice", _pInputDeviceType);
    _pSelectedDevice[OUTPUT] = _pParameterMgrPlatformConnector->createSelectionCriterion("SelectedOutputDevice", _pOutputDeviceType);

    // Client Mutex
    bzero(&_clientMutex, sizeof(_clientMutex));
    pthread_mutex_init(&_clientMutex, NULL);

    // Client wait semaphore
    bzero(&_clientWaitSemaphore, sizeof(_clientWaitSemaphore));
    sem_init(&_clientWaitSemaphore, 0, 0);

}

void CAudioRouteManager::start()
{
    assert(!_bIsStarted);

    _bIsStarted = true;

    // Start Event thread
    _pEventThread->start();

    // Start AT Manager
    startATManager();

    // Start PFW
    std::string strError;
    if (!_pParameterMgrPlatformConnector->start(strError)) {

        LOGE("parameter-framework start error: %s", strError.c_str());
    } else {

        LOGI("parameter-framework successfully started!");
    }
}

// From AudioHardwareALSA, set FM mode
// must be called with AudioHardwareALSA::mLock held
void CAudioRouteManager::setFmMode(int fmMode)
{
    // Update the platform state: fmmode
    _pPlatformState->setFmMode(fmMode);
}

// From AudioHardwareALSA, set TTY mode
void CAudioRouteManager::setTtyMode(ETty iTtyMode)
{
    _pPlatformState->setTtyMode(iTtyMode);
}

// From AudioHardwareALSA, set HAC mode
void CAudioRouteManager::setHacMode(bool bEnabled)
{
    _pPlatformState->setHacMode(bEnabled);
}

// From AudioHardwareALSA, set BT_NREC
void CAudioRouteManager::setBtNrEc(bool bIsAcousticSupportedOnBT)
{
    _pPlatformState->setBtNrEc(bIsAcousticSupportedOnBT);
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

    // Mutex
    pthread_mutex_destroy(&_clientMutex);

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
}

//
// Must be called from Locked context
//
status_t CAudioRouteManager::reconsiderRouting(bool bIsSynchronous)
{
    LOGD("%s", __FUNCTION__);

    assert(_bStarted && !_pEventThread->inThreadContext());

    status_t eStatus = NO_ERROR;

    // Block {
    pthread_mutex_lock(&_clientMutex);

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

    // } Block
    pthread_mutex_unlock(&_clientMutex);

    if (!bIsSynchronous) {

        return eStatus;
    }

    // Wait
    sem_wait(&_clientWaitSemaphore);

    // Block {
    pthread_mutex_lock(&_clientMutex);


    // Consume
    _bClientWaiting = false;

end:
    // } Block
    pthread_mutex_unlock(&_clientMutex);

    LOGD("%s: DONE", __FUNCTION__);
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
    LOGD("%s: following conditions:", __FUNCTION__);
    LOGD("%s:          -Modem Alive=%d", __FUNCTION__, _pPlatformState->isModemAlive());
    LOGD("%s:          -Modem Call Active=%d", __FUNCTION__, _pPlatformState->isModemAudioAvailable());
    LOGD("%s:          -Android Telephony Mode = %s", __FUNCTION__,
         print_criteria(_pPlatformState->getMode(), _stModeValuePairs, _uiNbModeValuePairs).c_str());
    LOGD("%s:          -RTE MGR HW Mode = %s", __FUNCTION__,
         print_criteria(_pPlatformState->getHwMode(), _stModeValuePairs, _uiNbModeValuePairs).c_str());
    LOGD("%s:          -Android FM Mode = %s", __FUNCTION__,
         print_criteria(_pPlatformState->getFmMode(), _stFmModeValuePairs, _uiNbFmModeValuePairs).c_str());
    LOGD("%s:          -Platform output device = %s", __FUNCTION__,
        print_criteria(_pPlatformState->getDevices(true), _stSelectedOutputDeviceValuePairs, _uiNbSelectedOutputDeviceValuePairs, true).c_str());
    LOGD("%s:          -Platform input device = %s", __FUNCTION__,
        print_criteria(_pPlatformState->getDevices(false), _stSelectedInputDeviceValuePairs, _uiNbSelectedInputDeviceValuePairs, true).c_str());
    LOGD("%s:          -Platform input source = %s", __FUNCTION__,
        print_criteria(_pPlatformState->getInputSource(), _stAudioSourceValuePairs, _uiNbAudioSourceValuePairs).c_str());




    // Reset availability of all route (All routes to be available)
    resetAvailability();

    // Parse all streams and affect route to it according to applicability of the route
    virtuallyConnectRoutes();

    LOGD("%s:          -Previously Enabled Route in Input = %s", __FUNCTION__,
         print_criteria(_uiPreviousEnabledRoutes[INPUT], _stSelectedRouteValuePairs, _uiNbSelectedRouteValuePairs, true).c_str());
    LOGD("%s:          -Previously Enabled Route in Output = %s", __FUNCTION__,
         print_criteria(_uiPreviousEnabledRoutes[OUTPUT], _stSelectedRouteValuePairs, _uiNbSelectedRouteValuePairs, true).c_str());
    LOGD("%s:          -To be Enabled Route in Input = %s", __FUNCTION__,
         print_criteria(_uiEnabledRoutes[INPUT], _stSelectedRouteValuePairs, _uiNbSelectedRouteValuePairs, true).c_str());
    LOGD("%s:          -To be Enabled Route in Output = %s", __FUNCTION__,
         print_criteria(_uiEnabledRoutes[OUTPUT], _stSelectedRouteValuePairs, _uiNbSelectedRouteValuePairs, true).c_str());
    LOGD("%s:          -Route that need reconfiguration in Input = %s", __FUNCTION__,
         print_criteria(_uiNeedToReconfigureRoutes[INPUT], _stSelectedRouteValuePairs, _uiNbSelectedRouteValuePairs, true).c_str());
    LOGD("%s:          -Route that need reconfiguration in Output = %s", __FUNCTION__,
         print_criteria(_uiNeedToReconfigureRoutes[OUTPUT], _stSelectedRouteValuePairs, _uiNbSelectedRouteValuePairs, true).c_str());

    // Mute Routes
    muteRoutes();

    // Disconnect all streams that need to be disconnected (ie, their route and or device and or mode changed)
    // starting from input streams
    disableRoutes();

    // Configure route through PFW
    configureRoutes();

    // Connect all streams that need to be connected (starting from output streams)
    enableRoutes();

    // Unmute Routes
    unmuteRoutes();

    // Clear Platform State flag
    _pPlatformState->clearPlatformState();

    // Is a client waiting for routing reconsideration?
    if (_bClientWaiting) {

        // Warn client
        sem_post(&_clientWaitSemaphore);
    }
}

//
// From ALSA Stream In / Out via AudiohardwareALSA
//
status_t CAudioRouteManager::setStreamParameters(ALSAStreamOps* pStream, const String8 &keyValuePairs, int iMode)
{
    AutoW lock(mLock);

    LOGD("%s", __FUNCTION__);
    AudioParameter param = AudioParameter(keyValuePairs);
    String8 key = String8(AudioParameter::keyRouting);
    status_t status;
    int devices;
    bool bForOutput = pStream->isOut();

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

    // Retrieve if provide, the input source for an input stream
    if (!bForOutput) {

        String8 keyIn = String8(AudioParameter::keyInputSource);

        // Get concerned input source
        int inputSource;
        status = param.getInt(keyIn, inputSource);

        if (status == NO_ERROR) {

            // Found the input source
            setInputSource(inputSource);
            // Remove parameter
            param.remove(keyIn);
        }
        if (devices == 0) {

            // When this function is called with a null device, considers it as
            // an unrouting request, restore source to default within route manager
            setInputSource(AUDIO_SOURCE_DEFAULT);
        }
    } else {

        // For output streams, latch Android Mode
        _pPlatformState->setMode(iMode);
    }

    ALOGW("AudioHardwareALSA::setStreamParameters() for %s devices: %s",
         bForOutput ? "output": "input",
         bForOutput ?\
             print_criteria(devices, _stSelectedInputDeviceValuePairs, _uiNbSelectedInputDeviceValuePairs).c_str() :\
             print_criteria(devices, _stSelectedOutputDeviceValuePairs, _uiNbSelectedOutputDeviceValuePairs).c_str());

    // Ask the route manager to update the stream devices
    setDevices(pStream, devices);

    //
    // ASYNCHRONOUSLY RECONSIDERATION of the routing now.
    // A change of device can be performed without locking the stream
    // on MRFLD.
    //
    status = reconsiderRouting(false);
    if (status != NO_ERROR) {

        // Just log!
        LOGE("alsa route error: %d", status);
    }

    // No more?
    if (param.size()) {

        // Just log!
        LOGW("Unhandled argument.");
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
    LOGD("%s: %s", __FUNCTION__, pStream->isOut()? "OUTPUT" : "INPUT");
    pStream->setStarted(true);

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
    LOGD("%s: %s", __FUNCTION__, pStream->isOut()? "OUTPUT" : "INPUT");
    pStream->setStarted(false);

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

    LOGD("%s: key value pair %s", __FUNCTION__, keyValuePairs.string());

    AudioParameter param = AudioParameter(keyValuePairs);
    status_t status;
    bool bReconsiderRouting = false;

    //
    // Search TTY mode
    //
    String8 strTtyDevice;
    ETty iTtyDevice = TTY_OFF;
    String8 key = String8(AUDIO_PARAMETER_KEY_TTY_MODE);

    // Get concerned devices
    status = param.get(key, strTtyDevice);

    if (status == NO_ERROR) {

        if(strTtyDevice == AUDIO_PARAMETER_VALUE_TTY_FULL) {
            LOGV("tty full\n");
            iTtyDevice = TTY_FULL;
        }
        else if(strTtyDevice == AUDIO_PARAMETER_VALUE_TTY_HCO) {
            LOGV("tty hco\n");
            iTtyDevice = TTY_HCO;
        }
        else if(strTtyDevice == AUDIO_PARAMETER_VALUE_TTY_VCO) {
            LOGV("tty vco\n");
            iTtyDevice = TTY_VCO;
        }
        else if (strTtyDevice == AUDIO_PARAMETER_VALUE_TTY_OFF) {
            LOGV("tty off\n");
            iTtyDevice = TTY_OFF;

        }

        setTtyMode(iTtyDevice);
        bReconsiderRouting = true;

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
            // We reconsider routing as VPC_BT_NREC_ON intent is sent first, then setStreamParameters and finally
            // VPC_BT_NREC_OFF when the SCO link is enabled. But only VPC_BT_NREC_ON setting is applied in that
            // context, resulting in loading the wrong Audience profile for BT SCO. This is where reconsiderRouting
            // becomes necessary, to be aligned with VPC_BT_NREC_OFF to process the correct Audience profile.
            bReconsiderRouting = true;
        }

        setBtNrEc(isBtNRecAvailable);


        // Remove parameter
        param.remove(key);
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
        bReconsiderRouting = true;

        // Remove parameter
        param.remove(key);
    }

    // Reconsider the routing now
    if (bReconsiderRouting) {

        //
        // ASYNCHRONOUSLY RECONSIDERATION of the routing in case of a parameter change
        //
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

    // Update Platform state: in/out devices
    _pPlatformState->setDevices(devices, bIsOut);

    ALOGD("%s: set device = %s to %s stream", __FUNCTION__,
         bIsOut ?\
             print_criteria(devices, _stSelectedInputDeviceValuePairs, _uiNbSelectedInputDeviceValuePairs).c_str() :\
             print_criteria(devices, _stSelectedOutputDeviceValuePairs, _uiNbSelectedOutputDeviceValuePairs).c_str(),
        bIsOut ? "output": "input");

    // set the new device for this stream
    pStream->setNewDevice(devices);

    // Set the mode to the stream (still necessary for ring buffer period according to the mode)
    pStream->setNewMode(_pPlatformState->getHwMode());
}

//
// From AudioInStreams: update the active input source
// Assumption: only one active input source at one time.
// Does it make sense to keep it in the platform state???
//
void CAudioRouteManager::setInputSource(uint32_t uiInputSource)
{
    AutoW lock(mLock);

    LOGD("%s: inputSource = %s", __FUNCTION__,
         print_criteria(_pPlatformState->getInputSource(), _stAudioSourceValuePairs, _uiNbAudioSourceValuePairs).c_str());
    _pPlatformState->setInputSource(uiInputSource);
}

void CAudioRouteManager::addRoute(uint32_t routeId)
{
    AutoW lock(mLock);

    LOGD("%s: add routeId %d to route manager", __FUNCTION__, routeId);

    CAudioRouteFactory* pRouteFactory = new CAudioRouteFactory();

    CAudioRoute* pRoute = pRouteFactory->getRoute(routeId, _pPlatformState);

    if (!pRoute) {

        LOGE("%s: could not find routeId %d", __FUNCTION__, routeId);
        return ;
    }

    _routeList.push_back(pRoute);

    delete pRouteFactory;
}

//
// This functionr resets the availability of all routes:
// ie it resets both condemnation and borrowed flags.
//
void CAudioRouteManager::resetAvailability()
{
    LOGD("%s", __FUNCTION__);

    CAudioRoute *aRoute =  NULL;
    CAudioStreamRoute* aStreamRoute = NULL;
    RouteListIterator it;

    // Find the applicable route for this route request
    for (it = _routeList.begin(); it != _routeList.end(); ++it) {

        aRoute = *it;
        aRoute->resetAvailability();
    }
}

//
// Add a stream to route manager
//
void CAudioRouteManager::addStream(ALSAStreamOps* pStream)
{
    AutoW lock(mLock);

    bool isOut = pStream->isOut();

    LOGD("%s: add %s stream to route manager", __FUNCTION__, isOut? "output" : "input");

    // Add Stream Out to the list
    _streamsList[isOut].push_back(pStream);
}

//
// Remove a stream from route manager
//
void CAudioRouteManager::removeStream(ALSAStreamOps* pStream)
{
    AutoW lock(mLock);

    LOGD("%s", __FUNCTION__);

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

void CAudioRouteManager::virtuallyConnectRoutes()
{
    virtuallyConnectRoutes(OUTPUT);
    virtuallyConnectRoutes(INPUT);
}

//
// This function virtually connects all streams to their
// new route according to:
//     -applicability of the route
//     -availability of the route
//
void CAudioRouteManager::virtuallyConnectRoutes(bool bIsOut)
{
    LOGD("%s", __FUNCTION__);

    uint32_t uiDevices = _pPlatformState->getDevices(bIsOut);
    int iMode = _pPlatformState->getHwMode();

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

        if (pRoute->getRouteType() == CAudioRoute::EExternalRoute) {

            if (pRoute->isApplicable(uiDevices, iMode, bIsOut)) {

                ALOGD("%s: route %s is applicable [mode = %s devices= %s]", __FUNCTION__,
                     pRoute->getName().c_str(),
                     print_criteria(iMode, _stModeValuePairs, _uiNbModeValuePairs).c_str(),
                     bIsOut ?\
                         print_criteria(uiDevices, _stSelectedInputDeviceValuePairs, _uiNbSelectedInputDeviceValuePairs, true).c_str() :\
                         print_criteria(uiDevices, _stSelectedOutputDeviceValuePairs, _uiNbSelectedOutputDeviceValuePairs, true).c_str());
                pRoute->setBorrowed(bIsOut);

                // Add route to enabled route bit field
                _uiEnabledRoutes[bIsOut] |= pRoute->getRouteId();

                continue;
            }

        } else if (pRoute->getRouteType() == CAudioRoute::EStreamRoute) {

            CAudioStreamRoute* pStreamRoute = NULL;
            ALSAStreamOps* pStreamOps;

            pStreamRoute = static_cast<CAudioStreamRoute*>(pRoute);

            // Check if an output stream is applicable for this stream
            pStreamOps = findApplicableStreamForRoute(bIsOut, pStreamRoute);

            if (pStreamOps) {

                pStreamRoute->setStream(pStreamOps);

                // Add route to enabled route bit field
                _uiEnabledRoutes[bIsOut] |= pRoute->getRouteId();
            }
        }

        if (pRoute->needReconfiguration(bIsOut)) {

            // Add route to NeedToReconfigureRoute bit field
            _uiNeedToReconfigureRoutes[bIsOut] |= pRoute->getRouteId();
        }
    }
}

ALSAStreamOps* CAudioRouteManager::findApplicableStreamForRoute(bool bIsOut, CAudioStreamRoute* pStreamRoute)
{
    ALSAStreamOpsListIterator it;

    for (it = _streamsList[bIsOut].begin(); it != _streamsList[bIsOut].end(); ++it) {

        ALSAStreamOps* pOps = *it;
        assert(isOut = pOps->isOut());

        // Check stream state
        if (!pOps->isStarted()) {

            // Stream is not started (standby or stopped)
            // Do not affect a route to this stream (route has been resetted in the virtual disconnect)
            continue;
        }

        // Check if the route is applicable
        // Applicability will also check if this route is already borrowed or not.
        if (pStreamRoute->isApplicable(pOps->getNewDevice(), _pPlatformState->getHwMode(), bIsOut)) {

            LOGD("%s: route %s is applicable", __FUNCTION__, pStreamRoute->getName().c_str());

            // Got it! Bailing out
            return pOps;
        }
    }
    return NULL;
}

void CAudioRouteManager::muteRoutes()
{
    // Mute Routes that
    //      -are disabled
    //      -need reconfiguration
    // Routing order criterion = Mute
    muteRoutes(INPUT);
    muteRoutes(OUTPUT);

    if (_pParameterMgrPlatformConnector->isStarted()) {

        // Warn PFW
        _pSelectedRouteStage->setCriterionState(EMute);

        std::string strError;
        if (!_pParameterMgrPlatformConnector->applyConfigurations(strError)) {

            LOGE("%s", strError.c_str());
        }
    }
}

void CAudioRouteManager::muteRoutes(bool bIsOut)
{

    uint32_t uiRoutesToMute = (_uiPreviousEnabledRoutes[bIsOut] & ~_uiEnabledRoutes[bIsOut]) |
            _uiNeedToReconfigureRoutes[bIsOut];

    //
    // CurrentEnabledRoute: Eclipse the route that need to reconfigure
    //                      so that the PFW mutes the route that need reconfiguration
    //
    uint32_t uiEnabledRoutes = _uiEnabledRoutes[bIsOut] & ~_uiNeedToReconfigureRoutes[bIsOut];

    LOGD("%s:          -Previously enabled routes in %s = %s", __FUNCTION__,
             bIsOut? "Output" : "Input",
             print_criteria(_uiPreviousEnabledRoutes[bIsOut], _stSelectedRouteValuePairs, _uiNbSelectedRouteValuePairs, true).c_str());

    LOGD("%s:          -Enabled routes [eclipsing route that need reconfiguration] in %s = %s", __FUNCTION__,
         bIsOut? "Output" : "Input",
         print_criteria(uiEnabledRoutes, _stSelectedRouteValuePairs, _uiNbSelectedRouteValuePairs, true).c_str());

    LOGD("%s:          --------------------------------------------------------------------------", __FUNCTION__);
    LOGD("%s:           Routes to be muted in %s = %s", __FUNCTION__,
         bIsOut? "Output" : "Input",
         print_criteria(uiRoutesToMute, _stSelectedRouteValuePairs, _uiNbSelectedRouteValuePairs, true).c_str());

    if (_pParameterMgrPlatformConnector->isStarted()) {

        // Warn PFW
        _pPreviousEnabledRoutes[bIsOut]->setCriterionState(_uiPreviousEnabledRoutes[bIsOut]);
        _pCurrentEnabledRoutes[bIsOut]->setCriterionState(uiEnabledRoutes);
    }
}

void CAudioRouteManager::unmuteRoutes()
{
    // Unmute Routes that
    //      -were not enabled but will be enabled
    //      -were previously enabled, will be enabled but need reconfiguration
    // Routing order criterion = Unmute
    unmuteRoutes(INPUT);
    unmuteRoutes(OUTPUT);

    if (_pParameterMgrPlatformConnector->isStarted()) {

        // Warn PFW
        _pSelectedRouteStage->setCriterionState(EUnmute);

        std::string strError;
        if (!_pParameterMgrPlatformConnector->applyConfigurations(strError)) {

            LOGE("%s", strError.c_str());
        }
    }
}
void CAudioRouteManager::unmuteRoutes(bool bIsOut)
{
    // Unmute Routes that
    //      -were not enabled but will be enabled
    //      -were previously enabled, will be enabled but need reconfiguration
    // Routing order criterion = Unmute

    uint32_t uiRoutesToUnmute = (_uiEnabledRoutes[bIsOut] & ~_uiPreviousEnabledRoutes[bIsOut]) |
                                    _uiNeedToReconfigureRoutes[bIsOut];

    //
    // PrevousEnabledRoute: eclipse the route that need to reconfigure so that
    // they can be unmuted
    //
//    uint32_t uiPreviousEnabledRoutes = _uiPreviousEnabledRoutes[bIsOut] & ~_uiNeedToReconfigureRoutes[bIsOut];

    LOGD("%s:          -Previously enabled routes [eclipsing route that need reconfiguration] in %s = %s", __FUNCTION__,
         bIsOut? "Output" : "Input",
         print_criteria(_uiPreviousEnabledRoutes[bIsOut], _stSelectedRouteValuePairs, _uiNbSelectedRouteValuePairs, true).c_str());

    LOGD("%s:          -Enabled routes  in %s = %s", __FUNCTION__,
         bIsOut? "Output" : "Input",
         print_criteria(_uiEnabledRoutes[bIsOut], _stSelectedRouteValuePairs, _uiNbSelectedRouteValuePairs, true).c_str());

    LOGD("%s:          --------------------------------------------------------------------------", __FUNCTION__);
    LOGD("%s:           Routes to be unmuted in %s = %s", __FUNCTION__,
         bIsOut? "Output" : "Input",
         print_criteria(uiRoutesToUnmute, _stSelectedRouteValuePairs, _uiNbSelectedRouteValuePairs, true).c_str());

    if (_pParameterMgrPlatformConnector->isStarted()) {

        // Warn PFW
        _pPreviousEnabledRoutes[bIsOut]->setCriterionState(_uiPreviousEnabledRoutes[bIsOut]);
    }
}

void CAudioRouteManager::configureRoutes()
{
    // RENAUD
    enableRoutes(OUTPUT);
    enableRoutes(INPUT);

    // PFW: Routing criterion = configure
    // Change here the devices, the mode, ... all the criteria
    // required for the routing
    configureRoutes(OUTPUT);
    configureRoutes(INPUT);

    if (_pParameterMgrPlatformConnector->isStarted()) {

        // Warn PFW
        _pSelectedMode->setCriterionState(_pPlatformState->getHwMode());
        _pSelectedTTYMode->setCriterionState(_pPlatformState->getTtyMode());
        _pSelectedRouteStage->setCriterionState(EConfigure);

        std::string strError;
        if (!_pParameterMgrPlatformConnector->applyConfigurations(strError)) {

            LOGE("%s", strError.c_str());
        }
    }
}

void CAudioRouteManager::configureRoutes(bool bIsOut)
{
    // Configure Routes that
    //      -were not enabled but will be enabled
    //      -were previously enabled, will be enabled but need reconfiguration
    // Routing order criterion = Configure
    uint32_t uiRoutesToConfigure = (_uiEnabledRoutes[bIsOut] & ~_uiPreviousEnabledRoutes[bIsOut]) |
                                    _uiNeedToReconfigureRoutes[bIsOut];

    LOGD("%s:          -Routes to be configured in %s = %s", __FUNCTION__,
         bIsOut? "Output" : "Input",
         print_criteria(uiRoutesToConfigure, _stSelectedRouteValuePairs, _uiNbSelectedRouteValuePairs, true).c_str());

    // PFW: Routing criterion = configure
    // Change here the devices, the mode, ... all the criteria
    // required for the routing
    if (_pParameterMgrPlatformConnector->isStarted()) {

        // Warn PFW
        _pSelectedDevice[bIsOut]->setCriterionState(_pPlatformState->getDevices(bIsOut));
        if (!bIsOut) {

            _pSelectedAudioSource->setCriterionState(_pPlatformState->getInputSource());
        }
    }
}

void CAudioRouteManager::disableRoutes()
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

        _pSelectedRouteStage->setCriterionState(EDisable);

        std::string strError;
        if (!_pParameterMgrPlatformConnector->applyConfigurations(strError)) {

            LOGE("%s", strError.c_str());
        }
    }
}

void CAudioRouteManager::disableRoutes(bool bIsOut)
{
    // Disable Routes that
    //      -are disabled
    //      -need reconfiguration
    // Routing order criterion = Disable
    uint32_t uiRoutesToDisable = (_uiPreviousEnabledRoutes[bIsOut] & ~_uiEnabledRoutes[bIsOut]) |
            _uiNeedToReconfigureRoutes[bIsOut];

    LOGD("%s:          -Routes to be disabled in %s = %s", __FUNCTION__,
         bIsOut? "Output" : "Input",
         print_criteria(uiRoutesToDisable, _stSelectedRouteValuePairs, _uiNbSelectedRouteValuePairs, true).c_str());

    CAudioRoute *aRoute =  NULL;
    RouteListIterator it;

    //
    // Performs the unrouting on the routes
    //
    for (it = _routeList.begin(); it != _routeList.end(); ++it) {

        aRoute = *it;

        //
        // Disable Routes that were used previously but are not used any more
        // (ie a stream was started on this route)
        //
        if (aRoute->currentlyBorrowed(bIsOut) && !aRoute->willBeBorrowed(bIsOut)) {

            LOGD("%s: route %s in %s to be unrouted", __FUNCTION__, aRoute->getName().c_str(), bIsOut? "PLAYBACK" : "CAPTURE");
            aRoute->unRoute(bIsOut);
        }
    }

    //
    // PFW: disable route stage:
    // CurrentEnabledRoutes reflects the reality: do not disable route that need reconfiguration only
    //
    if (_pParameterMgrPlatformConnector->isStarted()) {

        _pCurrentEnabledRoutes[bIsOut]->setCriterionState(_uiEnabledRoutes[bIsOut]);
    }
}

void CAudioRouteManager::enableRoutes()
{
    //
    // Enable routes through PFW
    //
    if (_pParameterMgrPlatformConnector->isStarted()) {

        // Update:
        // Routing stage criterion = Enable
        _pSelectedRouteStage->setCriterionState(EEnable);

        std::string strError;
        if (!_pParameterMgrPlatformConnector->applyConfigurations(strError)) {

            LOGE("%s", strError.c_str());
        }
    }

    // Connect all streams that need to be connected (starting from output streams)
//    enableRoutes(OUTPUT);
//    enableRoutes(INPUT);
}

void CAudioRouteManager::enableRoutes(bool bIsOut)
{
    CAudioRoute *aRoute =  NULL;
    RouteListIterator it;

    uint32_t uiRoutesToEnable = (_uiEnabledRoutes[bIsOut] & ~_uiPreviousEnabledRoutes[bIsOut]);

    LOGD("%s:          -Routes to be enabled in %s = %s", __FUNCTION__,
         bIsOut? "Output" : "Input",
         print_criteria(uiRoutesToEnable, _stSelectedRouteValuePairs, _uiNbSelectedRouteValuePairs, true).c_str());

    //
    // Performs the routing on the routes:
    // Enable Routes that were not enabled but will be enabled
    // (ie a stream will now be running on this route)
    //
    for (it = _routeList.begin(); it != _routeList.end(); ++it) {

        aRoute = *it;

        // If the route is external and was set borrowed -> it needs to be routed
        if (!aRoute->currentlyBorrowed(bIsOut) && aRoute->willBeBorrowed(bIsOut)) {

            LOGD("%s: route %s in %s to be routed", __FUNCTION__, aRoute->getName().c_str(), bIsOut? "PLAYBACK" : "CAPTURE");
            if (aRoute->route(bIsOut) != NO_ERROR) {

                // Just logging
                LOGE("%s: error while routing %s", __FUNCTION__, aRoute->getName().c_str());
            }
        }
    }
}

CAudioRoute* CAudioRouteManager::findRouteByName(const string& strName)
{
    LOGD("%s", __FUNCTION__);

    CAudioRoute* pFoundRoute =  NULL;
    RouteListIterator it;

    // Find the applicable route for this route request
    for (it = _routeList.begin(); it != _routeList.end(); ++it) {

        CAudioRoute* pRoute = *it;
        if (strName == pRoute->getName()) {

            pFoundRoute = pRoute;
            break;
        }
    }
    return pFoundRoute;
}

void CAudioRouteManager::startATManager()
{
    // Starts the ModemAudioManager
    if(_pModemAudioManager->start()) {

        LOGE("%s: could not start ModemAudioManager", __FUNCTION__);
        return ;
    }
    LOGE("%s: success", __FUNCTION__);
}

//
// From IModemStatusNotifier
// Need to hold AudioHardwareALSA::mLock
//
void CAudioRouteManager::onModemAudioStatusChanged()
{
    AutoW lock(mLock);

    LOGD("%s: IN", __FUNCTION__);

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

            status_t status = reconsiderRouting(false);
            if (status != NO_ERROR) {

                // Just log!
                LOGE("alsa route error: %d", status);
            }
        }
    }
    LOGD("%s: OUT", __FUNCTION__);
}

//
// From IModemStatusNotifier
// Called on Modem State change reported by STMD
// Need to hold AudioHardwareALSA::mLock
//
void  CAudioRouteManager::onModemStateChanged()
{
    AutoW lock(mLock);

    LOGD("%s: IN: ModemStatus", __FUNCTION__);

    _bModemAvailable = _pModemAudioManager->isModemAlive();

    // Update the platform state
    _pPlatformState->setModemAlive(_bModemAvailable);

    // Reset the modem audio status
    _pPlatformState->setModemAudioAvailable(false);

    // Reconsider the routing now only if the platform state has changed
    if (_pPlatformState->hasPlatformStateChanged()) {

        status_t status = reconsiderRouting(false);
        if (status != NO_ERROR) {

            // Just log!
            LOGE("alsa route error: %d", status);
        }
    }
    LOGD("%s: OUT", __FUNCTION__);
}

//
// From IModemStatusNotifier
// Called on Modem Audio PCM change report
//
void  CAudioRouteManager::onModemAudioPCMChanged() {

    MODEM_CODEC modemCodec;
    vpc_band_t band;

    AutoW lock(mLock);
    LOGD("%s: in", __FUNCTION__);

    modemCodec = _pModemAudioManager->getModemCodec();
    if (modemCodec == CODEC_TYPE_WB_AMR_SPEECH)
        band = VPC_BAND_WIDE;
    else
        band = VPC_BAND_NARROW;

    LOGD("%s: out", __FUNCTION__);
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
    LOGD("%s", __FUNCTION__);

    // Block {
    pthread_mutex_lock(&_clientMutex);


    // } Block
    pthread_mutex_unlock(&_clientMutex);
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
void CAudioRouteManager::onProcess()
{
    LOGD("%s", __FUNCTION__);

    // Block {
    pthread_mutex_lock(&_clientMutex);

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

    // } Block
    pthread_mutex_unlock(&_clientMutex);
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
void CAudioRouteManager::fillSelectionCriterionType(ISelectionCriterionTypeInterface* pSelectionCriterionType, const SSelectionCriterionTypeValuePair* pSelectionCriterionTypeValuePairs, uint32_t uiNbEntries) const
{
    uint32_t uiIndex;

    for (uiIndex = 0; uiIndex < uiNbEntries; uiIndex++) {

        const SSelectionCriterionTypeValuePair* pValuePair = &pSelectionCriterionTypeValuePairs[uiIndex];

        pSelectionCriterionType->addValuePair(pValuePair->iNumerical, pValuePair->pcLiteral);
    }
}

uint32_t CAudioRouteManager::getIntegerParameterValue(const string& strParameterPath, bool bSigned, uint32_t uiDefaultValue) const
{
    LOGD("%s in", __FUNCTION__);

    if (!_pParameterMgrPlatformConnector->isStarted()) {

        return uiDefaultValue;
    }

    string strError;
    // Get handle
    CParameterHandle* pParameterHandle = _pParameterMgrPlatformConnector->createParameterHandle(strParameterPath, strError);

    if (!pParameterHandle) {

        strError = strParameterPath.c_str();
        strError += " not found!";

        LOGE("Unable to get parameter handle: %s", strError.c_str());

        LOGD("%s returning %d", __FUNCTION__, uiDefaultValue);

        return uiDefaultValue;
    }

    // Retrieve value
    uint32_t uiValue;

    if ((!bSigned && !pParameterHandle->getAsInteger(uiValue, strError)) || (bSigned && !pParameterHandle->getAsSignedInteger((int32_t&)uiValue, strError))) {

        LOGE("Unable to get value: %s, from parameter path: %s", strError.c_str(), strParameterPath.c_str());

        LOGD("%s returning %d", __FUNCTION__, uiDefaultValue);

        // Remove handle
        delete pParameterHandle;

        return uiDefaultValue;
    }

    // Remove handle
    delete pParameterHandle;

    LOGD("%s returning %d", __FUNCTION__, uiValue);

    return uiValue;
}

status_t CAudioRouteManager::setIntegerParameterValue(const string& strParameterPath, bool bSigned, uint32_t uiValue)
{
    LOGD("%s in", __FUNCTION__);

    if (!_pParameterMgrPlatformConnector->isStarted()) {

        return NAME_NOT_FOUND;
    }

    string strError;
    // Get handle
    CParameterHandle* pParameterHandle = _pParameterMgrPlatformConnector->createParameterHandle(strParameterPath, strError);

    if (!pParameterHandle) {

        strError = strParameterPath.c_str();
        strError += " not found!";

        LOGE("%s: Unable to set parameter handle: %s", __FUNCTION__, strError.c_str());

        return NAME_NOT_FOUND;
    }

    // set value
    if ((!bSigned && !pParameterHandle->setAsInteger(uiValue, strError)) || (bSigned && !pParameterHandle->setAsSignedInteger((int32_t)uiValue, strError))) {

        LOGE("%s: Unable to set value: %s, from parameter path: %s", __FUNCTION__, strError.c_str(), strParameterPath.c_str());

        // Remove handle
        delete pParameterHandle;

        return NAME_NOT_FOUND;
    }

    // Remove handle
    delete pParameterHandle;

    LOGD("%s returning %d", __FUNCTION__, uiValue);

    return NO_ERROR;
}

//
// this function checks if at least one stream in bIsOut direction
// has been already started
//
bool CAudioRouteManager::hasActiveStream(bool bIsOut)
{
    ALSAStreamOpsListIterator it;

    for (it = _streamsList[bIsOut].begin(); it != _streamsList[bIsOut].end(); ++it) {

        ALSAStreamOps* pOps = *it;
        if (pOps->isStarted()) {

            return true;
        }
    }
    return false;
}

}       // namespace android
