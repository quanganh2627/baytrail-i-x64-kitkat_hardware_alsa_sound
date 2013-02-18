/*
 ** Copyright 2013 Intel Corporation
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
#include <utils/threads.h>
#include <hardware_legacy/AudioHardwareBase.h>

#include "AudioRoute.h"
#include "SyncSemaphoreList.h"
#include "Utils.h"
#include "ModemAudioManagerObserver.h"
#include "AudioPlatformState.h"
#include "EventListener.h"


class CEventThread;


class CParameterMgrPlatformConnector;
class ISelectionCriterionTypeInterface;
class ISelectionCriterionInterface;
struct IModemAudioManagerInterface;
struct echo_reference_itfe;

namespace android_audio_legacy
{
using android::RWLock;
using android::Mutex;

class CParameterMgrPlatformConnectorLogger;
class AudioHardwareALSA;
class AudioStreamInALSA;
class AudioStreamOutALSA;
class ALSAStreamOps;
class AudioStreamInALSA;
class CAudioRoute;
class CAudioPortGroup;
class CAudioPort;
class CAudioStreamRoute;
class CAudioParameterHandler;
class CAudioPlatformState;

class CAudioRouteManager : private IModemAudioManagerObserver, public IEventListener
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
        EScreenStateCriteriaType,
        EContextAwarenessCriteriaType,

        ENbCriteriaTypes
    };

    enum EventType {
        EUpdateModemAudioBand,
        EUpdateModemState,
        EUpdateModemAudioStatus,
        EUpdateRouting
    };

    /*
     * Routing stage bits description.
     * It is used to feed the criteria of the Parameter Framework.
     * This criterion is inclusive and is used to encode
     * 5 steps of the routing (Muting, Disabling, Configuring, Enabling, Unmuting)
     * Muting -> EFlow
     * Disabling -> EPath
     * Configuring -> EConfigure
     * Enabling -> EConfigure | EPath
     * Unmuting -> EConfigure | EPath | EFlow
     */
    enum RoutingStage {
        EFlow = (1 << 0),       /**< It refers to umute/unmute steps.   */
        EPath = (1 << 1),       /**< It refers to enable/disable steps  */
        EConfigure = (1 << 2)   /**< It refers to configure step        */
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

    status_t startStream(bool bIsStreamOut);

    status_t stopStream(bool bIsStreamOut);

    status_t setParameters(const String8& keyValuePairs);

    // Add a new group to route manager
    void addPort(uint32_t uiPortIndex);

    // Add a new group to route manager
    void addPortGroup(uint32_t uiPortGroupIndex);

    // Add a stream to route manager
    void addStream(ALSAStreamOps* pStream);

    // Remove a stream from route manager
    void removeStream(ALSAStreamOps* pStream);

    // Start route manager service
    status_t start();

    bool isStarted() const;

    void lock();

    void unlock();

    status_t setVoiceVolume(int gain);

    /**
     * Called by the stream in to request to add an effect.
     * It appends the effect to the stream list of requested effects
     * and add the effect only if the stream is already attached to the route.
     *
     * @param[in] pStream input stream pointer.
     * @param[in] effect structure of the effect to add.
     *
     * @return status_t OK upon succes, error code otherwise.
     */
    status_t addAudioEffectRequest(AudioStreamInALSA* pStream, effect_handle_t effect);

    /**
     * Called by an input stream in to request to remove an effect.
     * It removes the effect from the stream list of requested effects
     * and add the effect only if the stream is still attached to the route.
     *
     * @param[in] pStream input stream pointer.
     * @param[in] effect structure of the effect to add.
     *
     * @return status_t OK upon succes, error code otherwise.
     */
    status_t removeAudioEffectRequest(AudioStreamInALSA *pStream, effect_handle_t effect);

    /**
     * Called by an input stream in to add an effect.
     * When calling this function, the stream must be already attached to an audio route.
     *
     * @param[in] pStream input stream pointer.
     * @param[in] effect structure of the effect to add.
     *
     * @return status_t OK upon succes, error code otherwise.
     */
    status_t addAudioEffect(AudioStreamInALSA* pStream, effect_handle_t effect);

    /**
     * Called by an input stream in to remove an effect.
     * When calling this function, the stream must be still attached to an audio route..
     *
     * @param[in] pStream input stream pointer.
     * @param[in] effect structure of the effect to add.
     *
     * @return status_t OK upon succes, error code otherwise.
     */
    status_t removeAudioEffect(AudioStreamInALSA *pStream, effect_handle_t effect);

    /**
     * Reset the Echo Reference.
     * The purpose of this function is
     * - to stop the processing (i.e. writing of playback frames as echo reference for
     * for AEC effect) in AudioSteamOutALSA
     * - reset locally stored echo reference
     * @param[in] reference: pointer to echo reference to reset
     */
    void resetEchoReference(struct echo_reference_itfe* reference);

    /**
     * Get an Echo Reference for AEC.
     * The purpose of this function is
     *     - create echo_reference_itfe using input stream and output stream parameters
     *     - add echo_reference_itfs to AudioSteamOutALSA which will use it for
     *         providing playback frames as echo reference for AEC effect
     *     - store locally the created reference
     *     - return created echo_reference_itfe to caller (i.e. AudioSteamInALSA)
     * Note: created echo_reference_itfe is used as backlink between playback which
     *         provides reference of output data and record which applies AEC effect
     * @param[in] format: input stream format
     * @param[in] channel_count: input stream channels count
     * @param[in] sampling_rate: input stream sampling rate
     * @return NULL is creation of echo_reference_itfe failed overwise,
     *         pointer to created echo_reference_itfe
     */
    struct echo_reference_itfe* getEchoReference(int format,
                                                 uint32_t channel_count,
                                                 uint32_t sampling_rate);

private:
    CAudioRouteManager(const CAudioRouteManager &);
    CAudioRouteManager& operator = (const CAudioRouteManager &);

    void setDevices(ALSAStreamOps* pStream, uint32_t devices);

    void setInputSource(AudioStreamInALSA* pStreamIn, int iInputSource);

    void setOutputFlags(AudioStreamOutALSA* pStreamOut, uint32_t uiFlags);

    void reconsiderRouting(bool bIsSynchronous = true);

    // Routing within worker thread context
    void doReconsiderRouting();

    // Virtually connect routes
    bool prepareRouting();
    bool prepareRouting(bool bIsOut);
    void prepareRoute(CAudioRoute* pRoute, bool bIsOut);

    // Route stage dispatcher
    void executeRouting();

    // Mute the routes
    void executeMuteStage();
    void muteRoutes(bool bIsOut);

    // Unmute the routes
    void executeUnmuteStage();

    // Configure the routes
    void executeConfigureStage();
    void configureRoutes(bool bIsOut);

    // Disable the routes
    void executeDisableStage();
    void disableRoutes(bool bIsOut);

    // Enable the routes
    void executeEnableStage();
    void enableRoutes(bool bIsOut);

    // unsigned integer parameter value retrieval
    uint32_t getIntegerParameterValue(const string& strParameterPath, uint32_t uiDefaultValue) const;

    // unsigned integer parameter value set
    status_t setIntegerParameterValue(const string& strParameterPath, uint32_t uiValue);

    // unsigned integer parameter value setter
    status_t setIntegerArrayParameterValue(const string& strParameterPath, vector<uint32_t>& uiArray) const;

    // For a given streamroute, find an applicable in/out stream
    ALSAStreamOps* findApplicableStreamForRoute(bool bIsOut, const CAudioRoute* pRoute);

    // Retrieve route pointer from its name
    CAudioRoute* findRouteById(uint32_t uiRouteId);

    // Retrieve port pointer from its name
    CAudioPort* findPortById(uint32_t uiPortId);

    // Reset availability of all routes
    void resetAvailability();

    // Start the AT Manager
    void startModemAudioManager();

    bool isAecEffect(const effect_uuid_t *uuid);
    status_t getAudioEffectUuidFromHandle(effect_handle_t effect, effect_uuid_t* uuid);
    status_t doAddAudioEffect(AudioStreamInALSA* pStream, effect_handle_t effect);
    status_t doRemoveAudioEffect(AudioStreamInALSA *pStream, effect_handle_t effect);

    /// Inherited from IModemAudioManagerObserver
    virtual void onModemAudioStatusChanged();
    virtual void onModemStateChanged();
    virtual void onModemAudioBandChanged();

    // Inherited from IEventListener: Event processing
    virtual bool onEvent(int iFd);
    virtual bool onError(int iFd);
    virtual bool onHangup(int iFd);
    virtual void onAlarm();
    virtual void onPollError();
    virtual bool onProcess(uint16_t uiEvent);

    status_t doSetParameters(const String8& keyValuePairs);

    void createsRoutes();

    void createAudioHardwarePlatform();

    // Used to fill types for PFW
    ISelectionCriterionTypeInterface* createAndFillSelectionCriterionType(CriteriaType eCriteriaType) const;

    static const char* const gpcVoiceVolume;

    static const char* const LINE_IN_TO_HEADSET_LINE_VOLUME;
    static const char* const LINE_IN_TO_SPEAKER_LINE_VOLUME;
    static const char* const LINE_IN_TO_EAR_SPEAKER_LINE_VOLUME;

    static const char* const BLUETOOTH_HFP_SUPPORTED_PROP_NAME;
    static const bool BLUETOOTH_HFP_SUPPORTED_DEFAULT_VALUE;
    static const char* const PFW_CONF_FILE_NAME_PROP_NAME;
    static const char* const PFW_CONF_FILE_DEFAULT_NAME;
    static const char* const ROUTING_LOCKED_PROP_NAME;

    static const char* const gapcLineInToHeadsetLineVolume;
    static const char* const gapcLineInToSpeakerLineVolume;
    static const char* const gapcLineInToEarSpeakerLineVolume;

    static const char* const MODEM_LIB_PROP_NAME;

    static const uint32_t VOIP_RATE_FOR_NARROW_BAND_PROCESSING;

    bool _bBluetoothHFPSupported;

    // The connector
    CParameterMgrPlatformConnector* _pParameterMgrPlatformConnector;
    // Logger
    CParameterMgrPlatformConnectorLogger* _pParameterMgrPlatformConnectorLogger;

    // Mode type
    static const SSelectionCriterionTypeValuePair MODE_VALUE_PAIRS[];
    // Band type
    static const SSelectionCriterionTypeValuePair BAND_TYPE_VALUE_PAIRS[];
    // FM Mode type
    static const SSelectionCriterionTypeValuePair FM_MODE_VALUE_PAIRS[];
    // TTY Mode
    static const SSelectionCriterionTypeValuePair TTY_DIRECTION_VALUE_PAIRS[];
    // Band Ringing
    static const SSelectionCriterionTypeValuePair BAND_RINGING_VALUE_PAIRS[];
    // Routing Mode
    static const SSelectionCriterionTypeValuePair ROUTING_STAGE_VALUE_PAIRS[];
    // Audio Source
    static const SSelectionCriterionTypeValuePair AUDIO_SOURCE_VALUE_PAIRS[];
    // BT Headset NrEc
    static const SSelectionCriterionTypeValuePair BT_HEADSET_NREC_VALUE_PAIRS[];
    // HAC mode
    static const SSelectionCriterionTypeValuePair HAC_MODE_VALUE_PAIRS[];
    // Screen State
    static const SSelectionCriterionTypeValuePair SCREEN_STATE_VALUE_PAIRS[];
    // Route
    // Selected Input Device type
    static const SSelectionCriterionTypeValuePair INPUT_DEVICE_VALUE_PAIRS[];
    // Selected Output Device type
    static const SSelectionCriterionTypeValuePair OUTPUT_DEVICE_VALUE_PAIRS[];

    struct SSelectionCriterionTypeInterface
    {
        CriteriaType eCriteriaType;
        const SSelectionCriterionTypeValuePair* _pCriterionTypeValuePairs;
        uint32_t _uiNbValuePairs;
        bool _bIsInclusive;
    };

    static const SSelectionCriterionTypeInterface ARRAY_CRITERIA_TYPES[];

    ISelectionCriterionTypeInterface* _apCriteriaTypeInterface[ENbCriteriaTypes];

    // Criteria
    enum Criteria {
        ESelectedMode = 0,
        ESelectedFmMode,
        ESelectedTtyDirection,
        ESelectedRoutingStage,
        EClosingCaptureRoutes,
        EClosingPlaybackRoutes,
        EOpenedCaptureRoutes,
        EOpenedPlaybackRoutes,
        ESelectedInputDevice,
        ESelectedOutputDevice,
        ESelectedInputSource,
        ESelectedBandRinging,
        ESelectedBand,
        ESelectedBtHeadsetNrEc,
        ESelectedHacMode,
        ESelectedScreenState,

        ENbCriteria
    };
    struct CriteriaInterface {
        const char* pcName;
        CriteriaType eCriteriaType;
    };
    static const CriteriaInterface ARRAY_CRITERIA_INTERFACE[ENbCriteria];

    ISelectionCriterionInterface* _apSelectedCriteria[ENbCriteria];

    inline ISelectionCriterionInterface* selectedClosingRoutes(bool bIsOut) {

        Criteria eCriteria = (bIsOut? EClosingPlaybackRoutes : EClosingCaptureRoutes);
        return _apSelectedCriteria[eCriteria];
    }

    inline ISelectionCriterionInterface* selectedOpenedRoutes(bool bIsOut) {

        Criteria eCriteria = (bIsOut ? EOpenedPlaybackRoutes : EOpenedCaptureRoutes);
        return _apSelectedCriteria[eCriteria];
    }

    inline ISelectionCriterionInterface* selectedDevice(bool bIsOut) {

        Criteria eCriteria = (bIsOut ? ESelectedOutputDevice : ESelectedInputDevice);
        return _apSelectedCriteria[eCriteria];
    }

    // Input/Output Streams list
    list<ALSAStreamOps*> _streamsList[CUtils::ENbDirections];

    // List of route
    list<CAudioRoute*> _routeList;

    // List of port
    list<CAudioPort*> _portList;

    // List of port group
    list<CAudioPortGroup*> _portGroupList;

    // Audio AT Manager interface
    IModemAudioManagerInterface* _pModemAudioManagerInterface;

    // Platform state pointer
    CAudioPlatformState* _pPlatformState;

    // Worker Thread
    CEventThread* _pEventThread;

    // Client wait semaphore list
    CSyncSemaphoreList _clientWaitSemaphoreList;

    // Started service flag
    bool _bIsStarted;

    /*
     * Routing Protection Required.
     * This allows to handle platform with strong locking strategy
     * (streams may switch to one route to another dynamically)
     */
    bool _bRoutingLocked;

    // Routing timeout
    static const uint32_t _uiTimeoutSec;

    // Routing lock protection
    RWLock _lock;

    struct {

        // Bitfield of route that needs reconfiguration, it includes route
        // that were enabled and need to be disabled
        uint32_t uiNeedReconfig;
        // Bitfield of enabled route
        uint32_t uiEnabled;
        // Bitfield of previously enabled route
        uint32_t uiPrevEnabled;
    } _stRoutes[CUtils::ENbDirections];

protected:
    friend class AudioHardwareALSA;

    AudioHardwareALSA *     _pParent;

    // For backup and restore audio parameters
    CAudioParameterHandler* _pAudioParameterHandler;

    struct echo_reference_itfe* _pEchoReference;
};
};        // namespace android

