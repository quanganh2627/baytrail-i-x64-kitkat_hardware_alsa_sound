/* AudioPlatformHardware_victoriabay.cpp
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

#include "AudioPlatformHardware.h"

#include <tinyalsa/asoundlib.h>

#ifdef VB_HAL_AUDIO_TEMP
extern "C" {
#include "amhal.h"
}
#endif

#define PLABACK_PERIOD_TIME_MS  ((int)24)
#define CAPTURE_PERIOD_TIME_MS  ((int)24)
#define VOICE_PERIOD_TIME_MS    ((int)20)

#define NB_RING_BUFFER_NORMAL   ((int)2)
#define NB_RING_BUFFER_INCALL   ((int)4)

#define LONG_PERIOD_FACTOR      ((int)2)
#define MSEC_PER_SEC            ((int)1000)

/// May add a new route, include header here...
#define SAMPLE_RATE_16000               (16000)
#define SAMPLE_RATE_48000               (48000)

#define VOICE_16000_PERIOD_SIZE         ((int)VOICE_PERIOD_TIME_MS * SAMPLE_RATE_16000 / MSEC_PER_SEC)
#define VOICE_48000_PERIOD_SIZE         ((int)VOICE_PERIOD_TIME_MS * SAMPLE_RATE_48000 / MSEC_PER_SEC)

#define PLAYBACK_48000_PERIOD_SIZE      ((int)PLABACK_PERIOD_TIME_MS * SAMPLE_RATE_48000 * LONG_PERIOD_FACTOR / MSEC_PER_SEC)
#define CAPTURE_48000_PERIOD_SIZE       ((int)CAPTURE_PERIOD_TIME_MS * SAMPLE_RATE_48000 * LONG_PERIOD_FACTOR / MSEC_PER_SEC)


static const char* MEDIA_CARD_NAME = "cloverviewaudio";
#define MEDIA_PLAYBACK_DEVICE_ID        (0)
#define MEDIA_CAPTURE_DEVICE_ID         (0)


static const char* VOICE_MIXING_CARD_NAME = "IntelALSAIFX";
#define VOICE_MIXING_DEVICE_ID          (0)
#define VOICE_RECORD_DEVICE_ID          (0)

static const char* VOICE_CARD_NAME = "IntelALSASSP";
#define VOICE_DOWNLINK_DEVICE_ID    (2)
#define VOICE_UPLINK_DEVICE_ID      (2)

namespace android_audio_legacy
{
// For playback, configure ALSA to start the transfer when the
// first period is full.
// For recording, configure ALSA to start the transfer on the
// first frame.
static const pcm_config pcm_config_media_playback = {
   /* channels        : */2,
   /* rate            : */SAMPLE_RATE_48000,
   /* period_size     : */PLAYBACK_48000_PERIOD_SIZE,
   /* period_count    : */NB_RING_BUFFER_NORMAL,
   /* format          : */PCM_FORMAT_S16_LE,
   /* start_threshold : */(PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL) - 1,
   /* stop_threshold  : */PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
   /* silence_threshold : */0,
   /* avail_min       : */PLAYBACK_48000_PERIOD_SIZE,
};

static const pcm_config pcm_config_media_capture = {
   /* channels        : */2,
   /* rate            : */SAMPLE_RATE_48000,
   /* period_size     : */CAPTURE_48000_PERIOD_SIZE,
   /* period_count    : */NB_RING_BUFFER_NORMAL,
   /* format          : */PCM_FORMAT_S16_LE,
   /* start_threshold : */1,
   /* stop_threshold  : */CAPTURE_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
   /* silence_threshold : */0,
   /* avail_min       : */CAPTURE_48000_PERIOD_SIZE,
};

static const pcm_config pcm_config_voice_downlink = {
   /* channels        : */1,
   /* rate            : */SAMPLE_RATE_16000,
   /* period_size     : */VOICE_16000_PERIOD_SIZE,
   /* period_count    : */NB_RING_BUFFER_INCALL,
   /* format          : */PCM_FORMAT_S16_LE,
   /* start_threshold : */VOICE_16000_PERIOD_SIZE - 1,
   /* stop_threshold  : */VOICE_16000_PERIOD_SIZE * NB_RING_BUFFER_INCALL,
   /* silence_threshold : */0,
   /* avail_min       : */VOICE_16000_PERIOD_SIZE,
};

static const pcm_config pcm_config_voice_uplink = {
   /* channels        : */1,
   /* rate            : */SAMPLE_RATE_16000,
   /* period_size     : */VOICE_16000_PERIOD_SIZE,
   /* period_count    : */NB_RING_BUFFER_INCALL,
   /* format          : */PCM_FORMAT_S16_LE,
   /* start_threshold : */1,
   /* stop_threshold  : */VOICE_16000_PERIOD_SIZE * NB_RING_BUFFER_INCALL,
   /* silence_threshold : */0,
   /* avail_min       : */VOICE_16000_PERIOD_SIZE,
};

static const pcm_config pcm_config_voice_mixing_playback = {
   /* channels        : */2,
   /* rate            : */SAMPLE_RATE_48000,
   /* period_size     : */VOICE_48000_PERIOD_SIZE,
   /* period_count    : */NB_RING_BUFFER_INCALL,
   /* format          : */PCM_FORMAT_S16_LE,
   /* start_threshold : */VOICE_48000_PERIOD_SIZE - 1,
   /* stop_threshold  : */VOICE_48000_PERIOD_SIZE * NB_RING_BUFFER_INCALL,
   /* silence_threshold : */0,
   /* avail_min       : */NB_RING_BUFFER_INCALL,
};

static const pcm_config pcm_config_voice_mixing_capture = {
   /* channels        : */2,
   /* rate            : */SAMPLE_RATE_48000,
   /* period_size     : */PLAYBACK_48000_PERIOD_SIZE / 2,
   /* period_count    : */NB_RING_BUFFER_INCALL,
   /* format          : */PCM_FORMAT_S16_LE,
   /* start_threshold : */1,
   /* stop_threshold  : */VOICE_48000_PERIOD_SIZE * NB_RING_BUFFER_INCALL,
   /* silence_threshold : */0,
   /* avail_min       : */NB_RING_BUFFER_INCALL,
};

/// Port section

typedef enum {
    LPE_I2S2_PORT,
    LPE_I2S3_PORT,
    IA_I2S0_PORT,
    IA_I2S1_PORT,
    MODEM_I2S_PORT,
    BT_I2S_PORT,
    FM_I2S_PORT,
    HWCODEC_ASP_PORT,
    HWCODEC_VSP_PORT,
    HWCODEC_AUX_PORT,

    NB_AUDIO_PORT
} AudioPortId;

const uint32_t CAudioPlatformHardware::_uiNbPorts = NB_AUDIO_PORT;

const CAudioPlatformHardware::s_port_t CAudioPlatformHardware::_apcAudioPorts[CAudioPlatformHardware::_uiNbPorts] = {
    {1 << LPE_I2S2_PORT, "LPE_I2S2_PORT"},
    {1 << LPE_I2S3_PORT, "LPE_I2S3_PORT"},
    {1 << IA_I2S0_PORT, "IA_I2S0_PORT"},
    {1 << IA_I2S1_PORT, "IA_I2S1_PORT"},
    {1 << MODEM_I2S_PORT, "MODEM_I2S_PORT"},
    {1 << BT_I2S_PORT, "BT_I2S_PORT"},
    {1 << FM_I2S_PORT, "FM_I2S_PORT"},
    {1 << HWCODEC_ASP_PORT, "HWCODEC_ASP_PORT"},
    {1 << HWCODEC_VSP_PORT, "HWCODEC_VSP_PORT"},
    {1 << HWCODEC_AUX_PORT, "HWCODEC_AUX_PORT"},
};

// Port Group and associated port
#define NB_PORT_GROUP   2
#define NB_PORT_PGR0    2
#define NB_PORT_PGR1    2

const uint32_t CAudioPlatformHardware::_uiNbPortGroups = NB_PORT_GROUP;

const int CAudioPlatformHardware::_gstrNbPortByGroup[CAudioPlatformHardware::_uiNbPortGroups] = {
    NB_PORT_PGR0,
    NB_PORT_PGR1,
};

const CAudioPlatformHardware::s_port_group_t CAudioPlatformHardware::_astrPortGroups[CAudioPlatformHardware::_uiNbPortGroups] = {
    {"GROUP0", CAudioPlatformHardware::_gstrNbPortByGroup[0], (1 << IA_I2S1_PORT) | (1 << MODEM_I2S_PORT) },
    {"GROUP1", CAudioPlatformHardware::_gstrNbPortByGroup[1], (1 << BT_I2S_PORT) | (1 << FM_I2S_PORT)},
};

/// Route section

enum AudioRouteId {
    Media_Offset,           // Stream route
    CompressedMedia_Offset, // Compressed route
    ModemMix_Offset,        // Stream route
    HwCodecComm_Offset,     // Stream route
    HwCodecMedia_Offset,    // External route
    HwCodecCSV_Offset,      // External route
    HwCodecBt_Offset,       // External route
    HwCodecFm_Offset,       // External route

    NB_ROUTE
};

const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioPlatformHardware::_stRouteValuePairs[NB_ROUTE] = {
    { 1 << Media_Offset , "Media" },
    { 1 << CompressedMedia_Offset , "CompressedMedia" },
    { 1 << ModemMix_Offset , "ModemMix" },
    { 1 << HwCodecComm_Offset , "HwCodecComm" },
    { 1 << HwCodecMedia_Offset , "HwCodecMedia" },
    { 1 << HwCodecCSV_Offset , "HwCodecCSV" },
    { 1 << HwCodecBt_Offset , "HwCodecBt" },
    { 1 << HwCodecFm_Offset , "HwCodecFm" },
};

const uint32_t CAudioPlatformHardware::_uiNbRouteValuePairs = sizeof(CAudioPlatformHardware::_stRouteValuePairs)
        /sizeof(CAudioPlatformHardware::_stRouteValuePairs[0]);

const uint32_t CAudioPlatformHardware::_uiNbRoutes = NB_ROUTE;

//
// Route description structure
//
const CAudioPlatformHardware::s_route_t CAudioPlatformHardware::_astrAudioRoutes[CAudioPlatformHardware::_uiNbRoutes] = {
    ////////////////////////////////////////////////////////////////////////
    //
    // Streams routes
    //
    ////////////////////////////////////////////////////////////////////////
    //
    // MEDIA Route
    //
    {
        "Media",
        1 << Media_Offset,
        CAudioRoute::EStreamRoute,
        NOT_APPLICABLE,
        {
            DEVICE_IN_BUILTIN_ALL | AudioSystem::DEVICE_IN_FM_RECORD,
            DEVICE_OUT_MM_ALL
        },
        {
            (1 << AUDIO_SOURCE_DEFAULT) | (1 << AUDIO_SOURCE_MIC) | (1 << AUDIO_SOURCE_CAMCORDER) | (1 << AUDIO_SOURCE_VOICE_RECOGNITION),
            AUDIO_OUTPUT_FLAG_PRIMARY
        },
        {
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE),
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE)
        },
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
        },
        MEDIA_CARD_NAME,
        {
            MEDIA_CAPTURE_DEVICE_ID,
            MEDIA_PLAYBACK_DEVICE_ID
        },
        {
            pcm_config_media_capture,
            pcm_config_media_playback
        },
        NOT_APPLICABLE
    },
    //
    // Compressed Media Route
    //
    {
        "CompressedMedia",
        1 << CompressedMedia_Offset,
        CAudioRoute::ECompressedStreamRoute,
        NOT_APPLICABLE,
        {
            NOT_APPLICABLE,
            DEVICE_OUT_MM_ALL
        },
        {
            NOT_APPLICABLE,
            AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD
        },
        {
            NOT_APPLICABLE,
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE)
        },
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
        },
        NOT_APPLICABLE,
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
        },
        {
            pcm_config_not_applicable,
            pcm_config_not_applicable
        },
        NOT_APPLICABLE
    },
    //
    // Voice Mix route (IE mixing during CSV, VoIP or VoLTE call)
    // Once VoIP DL uses a Direct Output, it will be used to mix streams during VoIP calls
    // so update applicability accordingly...
    //
    {
        "ModemMix",
        1 << ModemMix_Offset,
        CAudioRoute::EStreamRoute,
        (1 << IA_I2S0_PORT),
        {
            AudioSystem::DEVICE_IN_ALL,
            DEVICE_OUT_MM_ALL | DEVICE_OUT_BLUETOOTH_SCO_ALL
        },
        {
            (1 << AUDIO_SOURCE_VOICE_UPLINK) | (1 << AUDIO_SOURCE_VOICE_DOWNLINK) | (1 << AUDIO_SOURCE_VOICE_CALL), // TBD IN COMMUNICATION
            AUDIO_OUTPUT_FLAG_PRIMARY
        },
        {
            (1 << AudioSystem::MODE_IN_CALL), // TBD: IN COMM as well?
            (1 << AudioSystem::MODE_IN_CALL) // TBD: IN COMM as well?
        },
        {
            CAudioPlatformState::EModemAudioStatus | CAudioPlatformState::EModemState,
            CAudioPlatformState::EModemAudioStatus | CAudioPlatformState::EModemState,
        },
        VOICE_MIXING_CARD_NAME,
        {
            VOICE_RECORD_DEVICE_ID,
            VOICE_MIXING_DEVICE_ID,
        },
        {
            pcm_config_voice_mixing_capture,
            pcm_config_voice_mixing_playback,
        },
        NOT_APPLICABLE
    },
    //
    // VOICE (VoIP) (on any output device) Route
    // May be accessed by Direct Output streams
    //
    {
        "HwCodecComm",
        1 << HwCodecComm_Offset,
        CAudioRoute::EStreamRoute,
        (1 << IA_I2S1_PORT) | (1 << HWCODEC_VSP_PORT),
        {
            DEVICE_IN_BUILTIN_ALL | DEVICE_IN_BLUETOOTH_SCO_ALL,
            DEVICE_OUT_MM_ALL | DEVICE_OUT_BLUETOOTH_SCO_ALL
        },
        {
            (1 << AUDIO_SOURCE_DEFAULT) | (1 << AUDIO_SOURCE_MIC) | (1 << AUDIO_SOURCE_VOICE_COMMUNICATION)| (1 << AUDIO_SOURCE_VOICE_RECOGNITION),
            AUDIO_OUTPUT_FLAG_PRIMARY // May use DIRECT flag later
        },
        {
            (1 << AudioSystem::MODE_IN_COMMUNICATION),
            (1 << AudioSystem::MODE_IN_COMMUNICATION)
        },
        {
            CAudioPlatformState::ESharedI2SState | CAudioPlatformState::EModemState,
            CAudioPlatformState::ESharedI2SState | CAudioPlatformState::EModemState
        },
        VOICE_CARD_NAME,
        {
            VOICE_UPLINK_DEVICE_ID,
            VOICE_DOWNLINK_DEVICE_ID,
        },
        {
            pcm_config_voice_uplink,
            pcm_config_voice_downlink,
        },
        NOT_APPLICABLE
    },
    ////////////////////////////////////////////////////////////////////////
    //
    // External routes
    //
    ////////////////////////////////////////////////////////////////////////
    //
    // HwCodec Media route
    // TBD: used for BT as well???
    //
    {
        "HwCodecMedia",
        1 << HwCodecMedia_Offset,
        CAudioRoute::EExternalRoute,
        (1 << LPE_I2S3_PORT) | (1 << HWCODEC_ASP_PORT),
        {
            DEVICE_IN_BUILTIN_ALL,
            DEVICE_OUT_MM_ALL
        },
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
        },
        {
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE),
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE)
        },
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
        },
        NOT_APPLICABLE,
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
        },
        {
            pcm_config_not_applicable,
            pcm_config_not_applicable
        },
        (1 << CompressedMedia_Offset) | (1 << Media_Offset)
    },
    //
    // CSV external route (on any output device)
    //
    {
        "HwCodecCSV",
        1 << HwCodecCSV_Offset,
        CAudioRoute::EExternalRoute,
        (1 << MODEM_I2S_PORT) | (1 << HWCODEC_VSP_PORT),
        {
            DEVICE_IN_BUILTIN_ALL | DEVICE_IN_BLUETOOTH_SCO_ALL, // be carefull, no input device selected in CSV by the policy!!!
            DEVICE_OUT_MM_ALL | DEVICE_OUT_BLUETOOTH_SCO_ALL
        },
        {
            NOT_APPLICABLE,
            AUDIO_OUTPUT_FLAG_NONE
        },
        {
            (1 << AudioSystem::MODE_IN_CALL),
            (1 << AudioSystem::MODE_IN_CALL)
        },
        {
            CAudioPlatformState::ESharedI2SState | CAudioPlatformState::EModemState | CAudioPlatformState::EModemAudioStatus,
            CAudioPlatformState::ESharedI2SState | CAudioPlatformState::EModemState | CAudioPlatformState::EModemAudioStatus
        },
        NOT_APPLICABLE,
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
        },
        {
            pcm_config_not_applicable,
            pcm_config_not_applicable
        },
        NOT_APPLICABLE
    },
    //
    // BT External route
    //
    {
        "HwCodecBt",
        1 << HwCodecBt_Offset,
        CAudioRoute::EExternalRoute,
        (1 << HWCODEC_AUX_PORT) | (1 << BT_I2S_PORT),
        {
            DEVICE_IN_BLUETOOTH_SCO_ALL,    // Be carefull, no input selected in case of CSV!!!
            DEVICE_OUT_BLUETOOTH_SCO_ALL
        },
        {
            NOT_APPLICABLE,
            AUDIO_OUTPUT_FLAG_NONE
        },
        {
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE) | (1 << AudioSystem::MODE_IN_CALL) | (1 << AudioSystem::MODE_IN_COMMUNICATION),
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE) | (1 << AudioSystem::MODE_IN_CALL) | (1 << AudioSystem::MODE_IN_COMMUNICATION)
        },
        {
            CAudioPlatformState::EBtEnable | CAudioPlatformState::ESharedI2SState | CAudioPlatformState::EModemState | CAudioPlatformState::EModemAudioStatus,
            CAudioPlatformState::EBtEnable | CAudioPlatformState::ESharedI2SState | CAudioPlatformState::EModemState | CAudioPlatformState::EModemAudioStatus
        },
        NOT_APPLICABLE,
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
        },
        {
            pcm_config_not_applicable,
            pcm_config_not_applicable
        },
        NOT_APPLICABLE
    },
    //
    // FM route
    //
    {
        "HwCodecFm",
        1 << HwCodecFm_Offset,
        CAudioRoute::EExternalRoute,
        (1 << HWCODEC_AUX_PORT) | (1 << FM_I2S_PORT),
        {
            NOT_APPLICABLE,
            DEVICE_OUT_MM_ALL
        },
        {
            AUDIO_SOURCE_MIC,
            AUDIO_OUTPUT_FLAG_NONE
        },
        {
            (1 << AudioSystem::MODE_NORMAL),
            (1 << AudioSystem::MODE_NORMAL)
        },
        {
            NOT_APPLICABLE,
            CAudioPlatformState::EFmHwMode
        },
        NOT_APPLICABLE,
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
        },
        {
            pcm_config_not_applicable,
            pcm_config_not_applicable
        },
        NOT_APPLICABLE
    }
};

//
// Specific Applicability Rules
//
class CAudioStreamRouteMedia : public CAudioStreamRoute
{
public:
    CAudioStreamRouteMedia(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioStreamRoute(uiRouteIndex, pPlatformState) {
    }

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t uiFlags = 0) const {

        return CAudioStreamRoute::isApplicable(uidevices, iMode, bIsOut, uiFlags);
    }
};


class CAudioStreamRouteHwCodecComm : public CAudioStreamRoute
{
public:
    CAudioStreamRouteHwCodecComm(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioStreamRoute(uiRouteIndex, pPlatformState) {
    }

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t uiFlags = 0) const {

        if (!_pPlatformState->isSharedI2SBusAvailable()) {

            return false;
        }
        return CAudioStreamRoute::isApplicable(uidevices, iMode, bIsOut, uiFlags);
    }

    virtual bool needReconfiguration(bool bIsOut) const
    {
        // The route needs reconfiguration except if:
        //      - still borrowed by the same stream
        //      - HAC mode has not changed
        //      - TTY direction has not changed
        //      - BT NrEc has not changed
        if ((CAudioRoute::needReconfiguration(bIsOut) &&
                _pPlatformState->hasPlatformStateChanged(
                     CAudioPlatformState::EHacModeChange |
                     CAudioPlatformState::ETtyDirectionChange |
                     CAudioPlatformState::EBtHeadsetNrEcChange)) ||
                CAudioStreamRoute::needReconfiguration(bIsOut)) {

            return true;
        }
        return false;
    }
};

class CAudioExternalRouteHwCodecBt : public CAudioExternalRoute
{
public:
    CAudioExternalRouteHwCodecBt(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioExternalRoute(uiRouteIndex, pPlatformState) {
    }

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t uiFlags = 0) const {

        // For Victoria Bay platform, set Bluetooth always enabled when a BT route
        // is needed. This change will be reverted as soon as the Bluedroid stack
        // will be integrated as the key telling the audio manager that the Bluetooth
        // is on comes from the BT service.
#ifdef VB_HAL_AUDIO_TEMP
        _pPlatformState->setBtEnabled(true);
#endif

        // BT module must be on and as the BT is on the shared I2S bus
        // the share bus must be available
        if (!_pPlatformState->isBtEnabled() || !_pPlatformState->isSharedI2SBusAvailable()) {

            return false;
        }
        if (!bIsOut && (iMode == AudioSystem::MODE_IN_CALL)) {

            // In Voice CALL, the audio policy does not give any input device
            // So, Input has no meaning except if this route is borrowed in output
            return willBeBorrowed(OUTPUT);
        }
        return CAudioExternalRoute::isApplicable(uidevices, iMode, bIsOut);
    }
};

class CAudioStreamRouteModemMix : public CAudioStreamRoute
{
public:
    CAudioStreamRouteModemMix(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioStreamRoute(uiRouteIndex, pPlatformState) {
    }

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t uiFlags = 0) const {

        return CAudioStreamRoute::isApplicable(uidevices, iMode, bIsOut, uiFlags);
    }
};

class CAudioExternalRouteHwCodecCSV : public CAudioExternalRoute
{
public:
    CAudioExternalRouteHwCodecCSV(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioExternalRoute(uiRouteIndex, pPlatformState) {

    }

  ~CAudioExternalRouteHwCodecCSV() {
#ifdef VB_HAL_AUDIO_TEMP
        amhal_close(_ctxtPtr);
#endif
  }

    //
    // This route is applicable in CALL mode, whatever the output device selected
    //
    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t uiFlags = 0) const {

        if (!bIsOut) {

            // Input has no meaning except if this route is borrowed in output
            return willBeBorrowed(OUTPUT);
        }
        return CAudioExternalRoute::isApplicable(uidevices, iMode, bIsOut);
    }

#ifdef VB_HAL_AUDIO_TEMP
    void * _ctxtPtr;
    int amhal_err;
    amhal_command_t command;

    // Route order - for external until Manager under PFW
    virtual status_t route(bool bIsOut) {

        static bool bFirstCall = true;

        // Open amhal at first call
        if (bFirstCall) {
            _ctxtPtr = amhal_open(NULL);

            if (!_ctxtPtr) {
                amhal_close(_ctxtPtr);
                ALOGE("can not open amhal, closing it!!!");
            }
            bFirstCall = false;
            //wait for amhal to be opened to be started before continuing
            // workaround: audio HAL is not registered to amhal callbacks
            usleep(500000);
        }

        if (bIsOut) {
            // Start Modem DSP
            command = AMHAL_SPUV_START;

            ALOGE("Send AMHAL_SPUV_START");

            amhal_err = amhal_sendto(_ctxtPtr, command, NULL);

            // Enable PCM for speech
            command = AMHAL_PCM_START;

            ALOGE("Send AMHAL_PCM_START");

            amhal_err = amhal_sendto(_ctxtPtr, command, NULL);

            ALOGE("Sleep for a while to guarantee audio started before opening codec");
            //wait for a while for modem to send data and provide clock before opening VSP
            // workaround: audio HAL is not registered to amhal callbacks
            usleep(500000);
        }

        return CAudioExternalRoute::route(bIsOut);
    }

    // UnRoute order - for external until Manager under PFW
    virtual void unRoute(bool bIsOut) {
        if (bIsOut) {
            command = AMHAL_PCM_STOP;

            ALOGE("Send AMHAL_PCM_STOP");

            amhal_err = amhal_sendto(_ctxtPtr, command, NULL);

            // the same is the same for stop
            command = AMHAL_SPUV_STOP;

            ALOGE("Send AMHAL_SPUV_STOP");

            amhal_err = amhal_sendto(_ctxtPtr, command, NULL);

        }
        CAudioExternalRoute::unRoute(bIsOut);
    }
#endif
};

class CAudioExternalRouteHwCodecFm : public CAudioExternalRoute
{
public:
    CAudioExternalRouteHwCodecFm(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioExternalRoute(uiRouteIndex, pPlatformState)
    {
    }

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t uiFlags) const
    {
        (void)uiFlags;

        // BT module must be off and as the BT is on the shared I2S bus
        // the modem must be alive as well to use this route
        if (!_pPlatformState->getFmRxHwMode()) {

            return false;
        }
        return CAudioExternalRoute::isApplicable(uidevices, iMode, bIsOut);
    }
};

//
// Once all deriavated class exception has been removed
// replace this function by a generic route creator according to the route type
//
CAudioRoute* CAudioPlatformHardware::createAudioRoute(uint32_t uiRouteId, CAudioPlatformState* pPlatformState)
{
    assert(pPlatformState);

    switch(uiRouteId) {

    /// Stream Routes
    case Media_Offset:
        return new CAudioStreamRouteMedia(uiRouteId, pPlatformState);

    case HwCodecComm_Offset:
        return new CAudioStreamRouteHwCodecComm(uiRouteId, pPlatformState);

    case ModemMix_Offset:
        return new CAudioStreamRouteModemMix(uiRouteId, pPlatformState);


    /// Compressed Routes
    case CompressedMedia_Offset:
        return new CAudioCompressedStreamRoute(uiRouteId, pPlatformState);

    /// External routes
    case HwCodecCSV_Offset:
        return new CAudioExternalRouteHwCodecCSV(uiRouteId, pPlatformState);

    case HwCodecMedia_Offset:
        return new CAudioExternalRoute(uiRouteId, pPlatformState);

    case HwCodecBt_Offset:
        return new CAudioExternalRouteHwCodecBt(uiRouteId, pPlatformState);

    case HwCodecFm_Offset:
        return new CAudioExternalRouteHwCodecFm(uiRouteId, pPlatformState);

    default:
        assert(0);
    }
    return NULL;
}

}        // namespace android

