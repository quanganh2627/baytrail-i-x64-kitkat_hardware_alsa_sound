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
#include <semaphore.h>
#include <utils/threads.h>
#include <hardware_legacy/AudioHardwareBase.h>

#include "AudioRoute.h"
#include "ModemAudioManager.h"
#include "AudioPlatformHardware.h"
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
class CAudioStreamRoute;

class CAudioRouteManager : public IModemStatusNotifier, public IEventListener
{
    enum RoutingStage {
        EMute,
        EDisable,
        EConfigure,
        EEnable,
        EUnmute
    };
    typedef list<CAudioRoute*>::iterator RouteListIterator;
    typedef list<CAudioRoute*>::const_iterator RouteListConstIterator;

    typedef list<ALSAStreamOps*>::iterator ALSAStreamOpsListIterator;
    typedef list<ALSAStreamOps*>::const_iterator ALSAStreamOpsListConstIterator;

public:
    CAudioRouteManager(AudioHardwareALSA *pParent);
    virtual           ~CAudioRouteManager();

    void addRoute(uint32_t routeId);

    status_t setStreamParameters(android_audio_legacy::ALSAStreamOps *pStream, const String8 &keyValuePairsSET, int iMode);

    status_t startStream(ALSAStreamOps* pStream);

    status_t stopStream(ALSAStreamOps* pStream);

    status_t setParameters(const String8& keyValuePairs);

    // Add a stream to route manager
    void addStream(ALSAStreamOps* pStream);

    // Remove a stream from route manager
    void removeStream(ALSAStreamOps* pStream);

    // Set FM mode
    void setFmMode(int fmMode);

    // Start route manager service
    void start();

    void lock();

    void unlock();

    CParameterMgrPlatformConnector* getParameterMgrPlatformConnector() const { return _pParameterMgrPlatformConnector; }

    // unsigned integer parameter value retrieval
    uint32_t getIntegerParameterValue(const string& strParameterPath, bool bSigned, uint32_t uiDefaultValue) const;

    // unsigned integer parameter value set
    status_t setIntegerParameterValue(const string& strParameterPath, bool bSigned, uint32_t uiDefaultValue);

    bool hasActiveStream(bool bIsOut);

protected:
    CAudioRoute* findApplicableRoute(uint32_t devices, uint32_t inputSource, int mode, bool bForOutput, CAudioRoute::RouteType type);

private:
    CAudioRouteManager(const CAudioRouteManager &);
    CAudioRouteManager& operator = (const CAudioRouteManager &);

    void setDevices(ALSAStreamOps* pStream, uint32_t devices);

    void setInputSource(uint32_t uiInputSource);

    // Set TTY mode
    void setTtyMode(ETty iTtyMode);

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
    void virtuallyConnectRoutes();
    void virtuallyConnectRoutes(bool bIsOut);

    // Mute the routes
    void muteRoutes();
    void muteRoutes(bool bIsOut);

    // Unmute the routes
    void unmuteRoutes();
    void unmuteRoutes(bool bIsOut);

    // Configure the routes
    void configureRoutes();
    void configureRoutes(bool bIsOut);

    // Disable the routes
    void disableRoutes();
    void disableRoutes(bool bIsOut);

    // Enable the routes
    void enableRoutes();
    void enableRoutes(bool bIsOut);

    // For a given streamroute, find an applicable in/out stream
    ALSAStreamOps* findApplicableStreamForRoute(bool bIsOut, CAudioStreamRoute* pStreamRoute);

    // Retrieve route pointer from its name
    CAudioRoute* findRouteByName(const string& strName);

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
    virtual void onProcess();

private:
    // PFW type value pairs type
    struct SSelectionCriterionTypeValuePair
    {
        int iNumerical;
        const char* pcLiteral;
    };
    const string print_criteria(int32_t uiValue, const SSelectionCriterionTypeValuePair* sPair, uint32_t uiNbPairs, bool bIsInclusive = false) const;

    // Used to fill types for PFW
    void fillSelectionCriterionType(ISelectionCriterionTypeInterface* pSelectionCriterionType, const SSelectionCriterionTypeValuePair* pSelectionCriterionTypeValuePairs, uint32_t uiNbEntries) const;

    // Mode type
    static const SSelectionCriterionTypeValuePair _stModeValuePairs[];
    static const uint32_t _uiNbModeValuePairs;

    // FM Mode type
    static const SSelectionCriterionTypeValuePair _stFmModeValuePairs[];
    static const uint32_t _uiNbFmModeValuePairs;
    // TTY Mode
    static const SSelectionCriterionTypeValuePair _stTTYModeValuePairs[];
    static const uint32_t _uiNbTTYModeValuePairs;

    // Band Ringing
    static const SSelectionCriterionTypeValuePair _stBandRingingValuePairs[];
    static const uint32_t _uiNbBandRingingValuePairs;

    // Routing Mode
    static const SSelectionCriterionTypeValuePair _stRoutingModeValuePairs[];
    static const uint32_t _uiNbRoutingModeValuePairs;

    // Audio Source
    static const SSelectionCriterionTypeValuePair _stAudioSourceValuePairs[];
    static const uint32_t _uiNbAudioSourceValuePairs;

    // Route
    // Selected Input Device type
    static const SSelectionCriterionTypeValuePair _stSelectedInputDeviceValuePairs[];
    static const uint32_t _uiNbSelectedInputDeviceValuePairs;
    // Selected Output Device type
    static const SSelectionCriterionTypeValuePair _stSelectedOutputDeviceValuePairs[];
    static const uint32_t _uiNbSelectedOutputDeviceValuePairs;
    // Selected Route type
    static const SSelectionCriterionTypeValuePair _stSelectedRouteValuePairs[];
    static const uint32_t _uiNbSelectedRouteValuePairs;

    static const char* const gapcVoicePortEnable[2];

    static const uint32_t ENABLED;
    static const uint32_t DISABLED;

    // The connector
    CParameterMgrPlatformConnector* _pParameterMgrPlatformConnector;
    // Logger
    CParameterMgrPlatformConnectorLogger* _pParameterMgrPlatformConnectorLogger;
    // Criteria Types
    ISelectionCriterionTypeInterface* _pModeType;
    ISelectionCriterionTypeInterface* _pFmModeType;
    ISelectionCriterionTypeInterface* _pmTTYModeType;

    ISelectionCriterionTypeInterface* _pRoutingModeType;

    ISelectionCriterionTypeInterface* _pRouteType;
    ISelectionCriterionTypeInterface* _pInputDeviceType;
    ISelectionCriterionTypeInterface* _pOutputDeviceType;
    ISelectionCriterionTypeInterface* _pAudioSourceType;

    ISelectionCriterionTypeInterface* _pBandRingingType;
    // Criteria
    ISelectionCriterionInterface* _pPreviousEnabledRoutes[2];
    ISelectionCriterionInterface* _pCurrentEnabledRoutes[2];
    ISelectionCriterionInterface* _pSelectedRouteStage;
    ISelectionCriterionInterface* _pSelectedMode;
    ISelectionCriterionInterface* _pSelectedFmMode;
    ISelectionCriterionInterface* _pSelectedTTYMode;
    ISelectionCriterionInterface* _pSelectedDevice[2];
    ISelectionCriterionInterface* _pSelectedAudioSource;

    ISelectionCriterionInterface* _pSelectedBandRinging;

    // Input/Output Streams list
    list<ALSAStreamOps*> _streamsList[2];

    // List of route
    list<CAudioRoute*> _routeList;

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

    // Client mutex
    pthread_mutex_t _clientMutex;

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

};
// ----------------------------------------------------------------------------

};        // namespace android

