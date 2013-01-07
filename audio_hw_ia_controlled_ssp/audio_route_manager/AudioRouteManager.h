/* RouteManager.h
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

#pragma once

#include <list>
#include <vector>
#include <semaphore.h>
#include <utils/threads.h>
#include <hardware_legacy/AudioHardwareBase.h>

#include "AudioRoute.h"
#include "ModemAudioManager.h"
#include "AudioPlatformState.h"


#define NOT_SET (-1)

class CEventThread;


class CParameterMgrPlatformConnector;
class ISelectionCriterionTypeInterface;
class ISelectionCriterionInterface;

namespace android_audio_legacy
{
using android::RWLock;
using android::Mutex;

class CParameterMgrPlatformConnectorLogger;
class AudioHardwareALSA;
class ALSAStreamOps;
class CAudioRoute;
class CAudioPortGroup;
class CAudioPort;
class CAudioStreamRoute;
class CAudioParameterHandler;

class CAudioRouteManager : public IModemStatusNotifier, public IEventListener
{
    // Criteria Types
    enum CriteriaType {
        EModeCriteriaType = 0,
        EFmModeCriteriaType,
        ETtyDirectionCriteriaType,
        ERoutingStageCriteriaType,
        ERouteCriteriaType,
        EInputDeviceCriteriaType,
        EOutputDeviceCriteriaType,
        EInputSourceCriteriaType,
        EBandRingingCriteriaType,
        EBandCriteriaType,
        EBtHeadsetNrEcCriteriaType,
        EHacModeCriteriaType,

        ENbCriteriaType
    };

    static inline uint32_t popCount(uint32_t u)
    {
        u = ((u&0x55555555) + ((u>>1)&0x55555555));
        u = ((u&0x33333333) + ((u>>2)&0x33333333));
        u = ((u&0x0f0f0f0f) + ((u>>4)&0x0f0f0f0f));
        u = ((u&0x00ff00ff) + ((u>>8)&0x00ff00ff));
        u = ( u&0x0000ffff) + (u>>16);
        return u;
    }

    enum RoutingStage {
        EMute = 0,
        EDisable,
        EConfigure,
        EEnable,
        EUnmute,

        ENbRoutingStage
    };

    typedef list<CAudioRoute*>::iterator RouteListIterator;
    typedef list<CAudioRoute*>::const_iterator RouteListConstIterator;
    typedef list<CAudioPort*>::iterator PortListIterator;
    typedef list<CAudioPort*>::iterator PortListConstIterator;
    typedef list<CAudioPortGroup*>::iterator PortGroupListIterator;
    typedef list<CAudioPortGroup*>::iterator PortGroupListConstIterator;


    typedef list<ALSAStreamOps*>::iterator ALSAStreamOpsListIterator;
    typedef list<ALSAStreamOps*>::const_iterator ALSAStreamOpsListConstIterator;

public:
    // PFW type value pairs type
    struct SSelectionCriterionTypeValuePair
    {
        int iNumerical;
        const char* pcLiteral;
    };

    CAudioRouteManager(AudioHardwareALSA* pParent);
    virtual           ~CAudioRouteManager();

    status_t setStreamParameters(android_audio_legacy::ALSAStreamOps* pStream, const String8 &keyValuePairsSET, int iMode);

    status_t startStream(ALSAStreamOps* pStream);

    status_t stopStream(ALSAStreamOps* pStream);

    status_t setParameters(const String8& keyValuePairs);

    // Add a new group to route manager
    void addPort(uint32_t uiPortIndex);

    // Add a new group to route manager
    void addPortGroup(uint32_t uiPortGroupIndex);

    // Add a stream to route manager
    void addStream(ALSAStreamOps* pStream);

    // Remove a stream from route manager
    void removeStream(ALSAStreamOps* pStream);

    // Set FM mode
    void setFmRxMode(int fmMode);

    // Start route manager service
    status_t start();

    bool isStarted() const;

    void lock();

    void unlock();

    status_t setVoiceVolume(int gain);

    status_t setFmRxVolume(float volume);

protected:
    CAudioRoute* findApplicableRoute(uint32_t devices, uint32_t uiFlags, int mode, bool bForOutput, CAudioRoute::RouteType type);

private:
    CAudioRouteManager(const CAudioRouteManager &);
    CAudioRouteManager& operator = (const CAudioRouteManager &);

    void setDevices(ALSAStreamOps* pStream, uint32_t devices);

    void setInputSource(android_audio_legacy::ALSAStreamOps *pStream, int iInputSource);

    void setStreamFlags(android_audio_legacy::ALSAStreamOps *pStream, uint32_t uiFlags);

    // Set TTY mode
    void setTtyDirection(int iTtyDirection);

    // Set HAC mode
    void setHacMode(bool bEnabled);

    // Set BT_NREC
    void setBtNrEc(bool bIsAcousticSupportedOnBT);

    // Set BT Enabled
    void setBtEnable(bool bIsBTEnabled);

    status_t reconsiderRouting(bool bIsSynchronous = true);

    // Routing within worker thread context
    void doReconsiderRouting();

    // Virtually connect routes
    bool virtuallyConnectRoutes();
    bool virtuallyConnectRoutes(bool bIsOut);
    void virtuallyConnectRoute(CAudioRoute* pRoute, bool bIsOut);

    // Route stage dispatcher
    void executeRoutingStage(int iRouteStage);

    // Mute the routes
    void muteRoutingStage();
    void muteRoutes(bool bIsOut);

    // Unmute the routes
    void unmuteRoutingStage();
    void unmuteRoutes(bool bIsOut);

    // Configure the routes
    void configureRoutingStage();
    void configureRoutes(bool bIsOut);

    // Disable the routes
    void disableRoutingStage();
    void disableRoutes(bool bIsOut);

    // Enable the routes
    void enableRoutingStage();
    void enableRoutes(bool bIsOut);

    // unsigned integer parameter value retrieval
    uint32_t getIntegerParameterValue(const string& strParameterPath, uint32_t uiDefaultValue) const;

    // unsigned integer parameter value set
    status_t setIntegerParameterValue(const string& strParameterPath, uint32_t uiValue);

    // unsigned integer parameter value setter
    status_t setIntegerArrayParameterValue(const string& strParameterPath, std::vector<uint32_t>& uiArray) const;

    // For a given streamroute, find an applicable in/out stream
    ALSAStreamOps* findApplicableStreamForRoute(bool bIsOut, android_audio_legacy::CAudioRoute *pRoute);

    // Retrieve route pointer from its name
    CAudioRoute* findRouteById(uint32_t uiRouteId);

    // Retrieve port pointer from its name
    CAudioPort* findPortById(uint32_t uiPortId);

    // Reset availability of all routes
    void resetAvailability();

    // Start the AT Manager
    void startATManager();

    /* from ModemStatusNotifier: notified on modem status changes */
    virtual void onModemAudioStatusChanged();
    virtual void onModemStateChanged();
    virtual void onModemAudioPCMChanged();

    // Inherited from IEventListener: Event processing
    virtual bool onEvent(int iFd);
    virtual bool onError(int iFd);
    virtual bool onHangup(int iFd);
    virtual void onTimeout();
    virtual void onPollError();
    virtual bool onProcess(uint16_t uiEvent);

    status_t doSetParameters(const String8& keyValuePairs);

    void createsRoutes();

    void createAudioHardwarePlatform();

    const string print_criteria(int32_t iValue, CriteriaType eCriteriaType) const;

    // Used to fill types for PFW
    ISelectionCriterionTypeInterface* createAndFillSelectionCriterionType(CriteriaType eCriteriaType) const;

    static const char* const gapcVoicePortEnable[2];

    static const char* gpcVoiceVolume;

    static const uint32_t ENABLED;
    static const uint32_t DISABLED;

    static const uint32_t DEFAULT_FM_RX_VOL_MAX;
    static const uint32_t FM_RX_STREAM_MAX_VOLUME;

    enum FM_RX_DEVICE {
        FM_RX_SPEAKER,
        FM_RX_HEADSET,

        FM_RX_NB_DEVICE
    };
    static const char* const gapcLineInToHeadsetLineVolume;
    static const char* const gapcLineInToSpeakerLineVolume;
    static const char* const gapcLineInToEarSpeakerLineVolume;
    static const char* const gapcDefaultFmRxMaxVolume[FM_RX_NB_DEVICE];

    static const char* const mModemEmbeddedPropName;
    static const bool mModemEmbeddedDefaultValue;
    static const char* const mFmSupportedPropName;
    static const bool mFmSupportedDefaultValue;
    static const char* const mFmIsAnalogPropName;
    static const bool mFmIsAnalogDefaultValue;
    static const char* const mBluetoothHFPSupportedPropName;
    static const bool mBluetoothHFPSupportedDefaultValue;
    static const char* const mPFWConfigurationFileNamePropertyName;

    // Indicate if platform supports FM Radio
    bool _bFmSupported;

    // Indicate if FM module supports RX Analog
    bool _bFmIsAnalog;

    bool _bBluetoothHFPSupported;

    //max values of FM RX Volume
    uint32_t _uiFmRxSpeakerMaxVolumeValue;
    uint32_t _uiFmRxHeadsetMaxVolumeValue;

    // The connector
    CParameterMgrPlatformConnector* _pParameterMgrPlatformConnector;
    // Logger
    CParameterMgrPlatformConnectorLogger* _pParameterMgrPlatformConnectorLogger;

    // Mode type
    static const SSelectionCriterionTypeValuePair _stModeValuePairs[];
    // Band type
    static const SSelectionCriterionTypeValuePair _stBandTypeValuePairs[];
    // FM Mode type
    static const SSelectionCriterionTypeValuePair _stFmModeValuePairs[];
    // TTY Mode
    static const SSelectionCriterionTypeValuePair _stTTYDirectionValuePairs[];
    // Band Ringing
    static const SSelectionCriterionTypeValuePair _stBandRingingValuePairs[];
    // Routing Mode
    static const SSelectionCriterionTypeValuePair _stRoutingStageValuePairs[];
    // Audio Source
    static const SSelectionCriterionTypeValuePair _stAudioSourceValuePairs[];
    // BT Headset NrEc
    static const SSelectionCriterionTypeValuePair _stBtHeadsetNrEcValuePairs[];
    // HAC mode
    static const SSelectionCriterionTypeValuePair _stHACModeValuePairs[];
    // Route
    // Selected Input Device type
    static const SSelectionCriterionTypeValuePair _stInputDeviceValuePairs[];
    // Selected Output Device type
    static const SSelectionCriterionTypeValuePair _stOutputDeviceValuePairs[];
    // Selected Route type
    SSelectionCriterionTypeValuePair* _stSelectedRouteValuePairs;

    struct SSelectionCriterionTypeInterface
    {
        CriteriaType eCriteriaType;
        const SSelectionCriterionTypeValuePair* _pCriterionTypeValuePairs;
        uint32_t _uiNbValuePairs;
        bool _bIsInclusive;
    };

    static const SSelectionCriterionTypeInterface _asCriteriaType[ENbCriteriaType];

    ISelectionCriterionTypeInterface* _apCriteriaTypeInterface[ENbCriteriaType];

    // Criteria
    enum Criteria {
        ESelectedMode = 0,
        ESelectedFmMode,
        ESelectedTtyDirection,
        ESelectedRoutingStage,
        EPreviousInputRoute,
        EPreviousOutputRoute,
        ESelectedInputRoute,
        ESelectedOutputRoute,
        ESelectedInputDevice,
        ESelectedOutputDevice,
        ESelectedInputSource,
        ESelectedBandRinging,
        ESelectedBand,
        ESelectedBtHeadsetNrEc,
        ESelectedHacMode,

        ENbCriteria
    };
    struct CriteriaInterface {
        const char* pcName;
        CriteriaType eCriteriaType;
    };
    static const CriteriaInterface _apCriteriaInterface[ENbCriteria];

    ISelectionCriterionInterface* _apSelectedCriteria[ENbCriteria];

    inline ISelectionCriterionInterface* selectedPreviousRoute(bool bIsOut) {

        Criteria eCriteria = (bIsOut? EPreviousOutputRoute : EPreviousInputRoute);
        return _apSelectedCriteria[eCriteria];
    }

    inline ISelectionCriterionInterface* selectedRoute(bool bIsOut) {

        Criteria eCriteria = (bIsOut? ESelectedOutputRoute : ESelectedInputRoute);
        return _apSelectedCriteria[eCriteria];
    }

    inline ISelectionCriterionInterface* selectedDevice(bool bIsOut) {

        Criteria eCriteria = (bIsOut? ESelectedOutputDevice : ESelectedInputDevice);
        return _apSelectedCriteria[eCriteria];
    }

    // Input/Output Streams list
    list<ALSAStreamOps*> _streamsList[2];

    // List of route
    list<CAudioRoute*> _routeList;

    // List of port
    list<CAudioPort*> _portList;

    // List of port group
    list<CAudioPortGroup*> _portGroupList;

    // Audio AT Manager
    CModemAudioManager* _pModemAudioManager;

    // Platform state pointer
    CAudioPlatformState* _pPlatformState;

    // Modem Call state
    bool _bModemCallActive;

    // Modem State
    bool _bModemAvailable;

    // Worker Thread
    CEventThread* _pEventThread;

    // Answer wait semaphore
    sem_t _clientWaitSemaphore;

    // Started service flag
    bool _bIsStarted;

    // Waiting Client flag
    bool _bClientWaiting;

    // Routing timeout
    static const uint32_t _uiTimeoutSec;

    // Routing lock protection
    RWLock mLock;

    // Bitfield of route that needs reconfiguration, it includes route
    // that were enabled and need to be disabled
    uint32_t _uiNeedToReconfigureRoutes[2];

    //  Bitfield of enabled route
    uint32_t _uiEnabledRoutes[2];

    //  Bitfield of previously enabled route
    uint32_t _uiPreviousEnabledRoutes[2];

protected:
    friend class AudioHardwareALSA;

    AudioHardwareALSA *     _pParent;

    // For backup and restore audio parameters
    CAudioParameterHandler* _pAudioParameterHandler;
};
// ----------------------------------------------------------------------------

};        // namespace android

