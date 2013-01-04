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

/// May add a new route, include header here...
#define SAMPLE_RATE_8000                (8000)
#define SAMPLE_RATE_48000               (48000)
#define VOICE_CAPTURE_PERIOD_SIZE       (320) // 20ms @ 16k, mono
#define PLAYBACK_44100_PERIOD_SIZE      (1024) //(PLAYBACK_44100_PERIOD_TIME_US * 44100 / USEC_PER_SEC)
#define PLAYBACK_48000_PERIOD_SIZE      (1152) //(24000*2 * 48000 / USEC_PER_SEC)
#define CAPTURE_48000_PERIOD_SIZE       (1152) //(CAPTURE_48000_PERIOD_SIZE * 48000 / USEC_PER_SEC)
#define NB_RING_BUFFER_NORMAL           (2)
#define NB_RING_BUFFER_INCALL           (4)


static const char* MEDIA_CARD_NAME = "cloverviewaudio";
#define MEDIA_PLAYBACK_DEVICE_ID        (0)
#define MEDIA_CAPTURE_DEVICE_ID         (0)


static const char* VOICE_MIXING_CARD_NAME = "IntelALSAIFX";
#define VOICE_MIXING_DEVICE_ID          (0)
#define VOICE_RECORD_DEVICE_ID          (0)

static const char* VOICE_CARD_NAME = "IntelALSASSP";
#define VOICE_HWCODEC_DOWNLINK_DEVICE_ID    (2)
#define VOICE_HWCODEC_UPLINK_DEVICE_ID      (2)
#define VOICE_BT_DOWNLINK_DEVICE_ID         (0)
#define VOICE_BT_UPLINK_DEVICE_ID           (0)

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
   /* start_threshold : */PLAYBACK_48000_PERIOD_SIZE - 1,
   /* stop_threshold  : */PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
   /* silence_threshold : */0,
   /* avail_min       : */PLAYBACK_48000_PERIOD_SIZE,
};

static const pcm_config pcm_config_media_capture = {
   /* channels        : */2,
   /* rate            : */SAMPLE_RATE_48000,
   /* period_size     : */PLAYBACK_48000_PERIOD_SIZE,
   /* period_count    : */NB_RING_BUFFER_NORMAL,
   /* format          : */PCM_FORMAT_S16_LE,
   /* start_threshold : */1,
   /* stop_threshold  : */PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
   /* silence_threshold : */0,
   /* avail_min       : */PLAYBACK_48000_PERIOD_SIZE,
};



static const pcm_config pcm_config_voice_codec_downlink = {
   /* channels        : */2,
   /* rate            : */SAMPLE_RATE_48000,
   /* period_size     : */VOICE_CAPTURE_PERIOD_SIZE,
   /* period_count    : */NB_RING_BUFFER_INCALL,
   /* format          : */PCM_FORMAT_S16_LE,
   /* start_threshold : */PLAYBACK_48000_PERIOD_SIZE,
   /* stop_threshold  : */PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
   /* silence_threshold : */0,
   /* avail_min       : */PLAYBACK_48000_PERIOD_SIZE,
};

static const pcm_config pcm_config_voice_codec_uplink = {
   /* channels        : */2,
   /* rate            : */SAMPLE_RATE_48000,
   /* period_size     : */VOICE_CAPTURE_PERIOD_SIZE,
   /* period_count    : */NB_RING_BUFFER_INCALL,
   /* format          : */PCM_FORMAT_S16_LE,
   /* start_threshold : */1,
   /* stop_threshold  : */PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
   /* silence_threshold : */0,
   /* avail_min       : */PLAYBACK_48000_PERIOD_SIZE,
};

static const pcm_config pcm_config_voice_bt_downlink = {
   /* channels        : */1,
   /* rate            : */SAMPLE_RATE_8000,
   /* period_size     : */VOICE_CAPTURE_PERIOD_SIZE,
   /* period_count    : */NB_RING_BUFFER_INCALL,
   /* format          : */PCM_FORMAT_S16_LE,
   /* start_threshold : */PLAYBACK_48000_PERIOD_SIZE,
   /* stop_threshold  : */PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
   /* silence_threshold : */0,
   /* avail_min       : */PLAYBACK_48000_PERIOD_SIZE,
};

static const pcm_config pcm_config_voice_bt_uplink = {
   /* channels        : */1,
   /* rate            : */SAMPLE_RATE_8000,
   /* period_size     : */VOICE_CAPTURE_PERIOD_SIZE,
   /* period_count    : */NB_RING_BUFFER_INCALL,
   /* format          : */PCM_FORMAT_S16_LE,
   /* start_threshold : */0,
   /* stop_threshold  : */0,
   /* silence_threshold : */0,
   /* avail_min       : */0,
};

static const pcm_config pcm_config_voice_hwcodec_downlink = {
   /* channels        : */2,
   /* rate            : */SAMPLE_RATE_48000,
   /* period_size     : */PLAYBACK_48000_PERIOD_SIZE / 2,
   /* period_count    : */4,
   /* format          : */PCM_FORMAT_S16_LE,
   /* start_threshold : */(PLAYBACK_48000_PERIOD_SIZE / 2),
   /* stop_threshold  : */PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
   /* silence_threshold : */0,
   /* avail_min       : */PLAYBACK_48000_PERIOD_SIZE / 2,
};

static const pcm_config pcm_config_voice_hwcodec_uplink = {
   /* channels        : */2,
   /* rate            : */SAMPLE_RATE_48000,
   /* period_size     : */PLAYBACK_48000_PERIOD_SIZE / 2,
   /* period_count    : */4,
   /* format          : */PCM_FORMAT_S16_LE,
   /* start_threshold : */0,
   /* stop_threshold  : */0,
   /* silence_threshold : */0,
   /* avail_min       : */0,
};

static const pcm_config pcm_config_voice_mixing_playback = {
   /* channels        : */2,
   /* rate            : */SAMPLE_RATE_48000,
   /* period_size     : */PLAYBACK_48000_PERIOD_SIZE,
   /* period_count    : */NB_RING_BUFFER_NORMAL,
   /* format          : */PCM_FORMAT_S16_LE,
   /* start_threshold : */PLAYBACK_48000_PERIOD_SIZE - 1,
   /* stop_threshold  : */PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
   /* silence_threshold : */0,
   /* avail_min       : */PLAYBACK_48000_PERIOD_SIZE,
};

static const pcm_config pcm_config_voice_mixing_capture = {
   /* channels        : */2,
   /* rate            : */SAMPLE_RATE_48000,
   /* period_size     : */PLAYBACK_48000_PERIOD_SIZE / 2,
   /* period_count    : */4,
   /* format          : */PCM_FORMAT_S16_LE,
   /* start_threshold : */0,
   /* stop_threshold  : */0,
   /* silence_threshold : */0,
   /* avail_min       : */0,
};

typedef enum {
    LPE_I2S2_PORT,
    LPE_I2S3_PORT,
    IA_I2S0_PORT,
    IA_I2S1_PORT,
    MODEM_I2S1_PORT,
    MODEM_I2S2_PORT,
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
    {1 << MODEM_I2S1_PORT, "MODEM_I2S1_PORT"},
    {1 << MODEM_I2S2_PORT, "MODEM_I2S2_PORT"},
    {1 << BT_I2S_PORT, "BT_I2S_PORT"},
    {1 << FM_I2S_PORT, "FM_I2S_PORT"},
    {1 << HWCODEC_ASP_PORT, "HWCODEC_ASP_PORT"},
    {1 << HWCODEC_VSP_PORT, "HWCODEC_VSP_PORT"},
    {1 << HWCODEC_AUX_PORT, "HWCODEC_AUX_PORT"}
};

// Port Group and associated port
// Group0: "HwCodecMediaPort", "HwCodecVoicePort"
// Group1: "BTPort", "HwCodecVoicePort"
// Group2: "IACommPort", "ModemCsvPort"
#define NB_PORT_GROUP   3
#define NB_PORT_PGR0    2
#define NB_PORT_PGR1    2
#define NB_PORT_PGR2    2

const uint32_t CAudioPlatformHardware::_uiNbPortGroups = NB_PORT_GROUP;

const int CAudioPlatformHardware::_gstrNbPortByGroup[CAudioPlatformHardware::_uiNbPortGroups] = {
    NB_PORT_PGR0,
    NB_PORT_PGR1,
    NB_PORT_PGR2
};

const CAudioPlatformHardware::s_port_group_t CAudioPlatformHardware::_astrPortGroups[CAudioPlatformHardware::_uiNbPortGroups] = {
    {"GROUP0", CAudioPlatformHardware::_gstrNbPortByGroup[0], (1 << HWCODEC_ASP_PORT) | (1 << HWCODEC_VSP_PORT) },
    {"GROUP1", CAudioPlatformHardware::_gstrNbPortByGroup[1], (1 << HWCODEC_VSP_PORT) | (1 << BT_I2S_PORT)},
    {"GROUP2", CAudioPlatformHardware::_gstrNbPortByGroup[2], (1 << IA_I2S1_PORT) | (1 << MODEM_I2S1_PORT)}
};

enum AudioRouteId {
    Media_Offset,
    CompressedMedia_Offset,
    ModemMix_Offset,
    HwCodecComm_Offset,
    BtComm_Offset,
    HwCodecMedia_Offset,
    HwCodecCSV_Offset,
    BtCSV_Offset,
    HwCodecFm_Offset,

    NB_ROUTE
};

const CAudioRouteManager::SSelectionCriterionTypeValuePair CAudioPlatformHardware::_stRouteValuePairs[NB_ROUTE] = {
    { 1 << Media_Offset , "Media" },
    { 1 << CompressedMedia_Offset , "CompressedMedia" },
    { 1 << ModemMix_Offset , "ModemMix" },
    { 1 << HwCodecComm_Offset , "HwCodecComm" },
    { 1 << BtComm_Offset , "BtComm" },
    { 1 << HwCodecMedia_Offset , "HwCodecMedia" },
    { 1 << HwCodecCSV_Offset , "HwCodecCSV" },
    { 1 << BtCSV_Offset , "BtCSV" },
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
        (1 << LPE_I2S3_PORT) | (1 << HWCODEC_ASP_PORT),
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
        (1 << LPE_I2S3_PORT) | (1 << HWCODEC_ASP_PORT),
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
    //
    {
        "ModemMix",
        1 << ModemMix_Offset,
        CAudioRoute::EStreamRoute,
        (1 << IA_I2S0_PORT) | (1 << MODEM_I2S2_PORT),
        {
            AudioSystem::DEVICE_IN_ALL,
            DEVICE_OUT_MM_ALL | DEVICE_OUT_BLUETOOTH_SCO_ALL
        },
        {
            (1 << AUDIO_SOURCE_VOICE_UPLINK) | (1 << AUDIO_SOURCE_VOICE_DOWNLINK) | (1 << AUDIO_SOURCE_VOICE_CALL),
            AUDIO_OUTPUT_FLAG_PRIMARY
        },
        {
            (1 << AudioSystem::MODE_IN_CALL),
            (1 << AudioSystem::MODE_IN_CALL)
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
    // VOICE (VoIP) on HWCodec Route
    //
    {
        "HwCodecComm",
        1 << HwCodecComm_Offset,
        CAudioRoute::EStreamRoute,
        (1 << IA_I2S1_PORT) | (1 << HWCODEC_VSP_PORT),
        {
            DEVICE_IN_BUILTIN_ALL,
            DEVICE_OUT_MM_ALL
        },
        {
            (1 << AUDIO_SOURCE_DEFAULT) | (1 << AUDIO_SOURCE_MIC) | (1 << AUDIO_SOURCE_VOICE_COMMUNICATION),
            AUDIO_OUTPUT_FLAG_PRIMARY
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
            VOICE_HWCODEC_UPLINK_DEVICE_ID,
            VOICE_HWCODEC_DOWNLINK_DEVICE_ID,
        },
        {
            pcm_config_voice_hwcodec_uplink,
            pcm_config_voice_hwcodec_downlink,
        },
        NOT_APPLICABLE
    },
    //
    // VOICE (VoIP) on BT Route, or when BT_SCO is forced in normal mode
    //
    {
        "BtComm",
        1 << BtComm_Offset,
        CAudioRoute::EStreamRoute,
        (1 << IA_I2S1_PORT) | (1 << BT_I2S_PORT),
        {
            DEVICE_IN_BLUETOOTH_SCO_ALL,
            DEVICE_OUT_BLUETOOTH_SCO_ALL
        },
        {
            (1 << AUDIO_SOURCE_DEFAULT) | (1 << AUDIO_SOURCE_MIC) | (1 << AUDIO_SOURCE_VOICE_COMMUNICATION)| (1 << AUDIO_SOURCE_VOICE_RECOGNITION),
            AUDIO_OUTPUT_FLAG_PRIMARY
        },
        {
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_IN_COMMUNICATION),
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_IN_COMMUNICATION)
        },
        {
            CAudioPlatformState::EBtEnable | CAudioPlatformState::ESharedI2SState | CAudioPlatformState::EModemState,
            CAudioPlatformState::EBtEnable | CAudioPlatformState::ESharedI2SState | CAudioPlatformState::EModemState,
        },
        VOICE_CARD_NAME,
        {
            VOICE_BT_DOWNLINK_DEVICE_ID,
            VOICE_BT_UPLINK_DEVICE_ID
        },
        {
            pcm_config_voice_bt_uplink,
            pcm_config_voice_bt_downlink,
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
    //
    {
        "HwCodecMedia",
        1 << HwCodecMedia_Offset,
        CAudioRoute::EExternalRoute,
        NOT_APPLICABLE,
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
    // HwCodec CSV route
    //
    {
        "HwCodecCSV",
        1 << HwCodecCSV_Offset,
        CAudioRoute::EExternalRoute,
        (1 << MODEM_I2S1_PORT) | (1 << HWCODEC_VSP_PORT),
        {
            NOT_APPLICABLE,     // Why? because there are no input stream for the CSV UL!!!
            DEVICE_OUT_MM_ALL
        },
        {
            NOT_APPLICABLE,
            AUDIO_OUTPUT_FLAG_NONE
        },
        {
            NOT_APPLICABLE,
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
    // BT CSV route
    //
    {
        "BtCSV",
        1 << BtCSV_Offset,
        CAudioRoute::EExternalRoute,
        (1 << MODEM_I2S1_PORT) | (1 << BT_I2S_PORT),
        {
            NOT_APPLICABLE,     // Why? because there are no input stream for the BT CSV UL!!!
            DEVICE_OUT_BLUETOOTH_SCO_ALL
        },
        {
            NOT_APPLICABLE,
            AUDIO_OUTPUT_FLAG_NONE
        },
        {
            NOT_APPLICABLE,
            (1 << AudioSystem::MODE_IN_CALL)
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
        (1 << FM_I2S_PORT) | (1 << LPE_I2S2_PORT),
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

        if (_pPlatformState->hasDirectStreams()) {

            return false;
        }
        return CAudioStreamRoute::isApplicable(uidevices, iMode, bIsOut, uiFlags);
    }
};


class CAudioStreamRouteHwCodecComm : public CAudioStreamRoute
{
public:
    CAudioStreamRouteHwCodecComm(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioStreamRoute(uiRouteIndex, pPlatformState) {
    }

    bool needReconfiguration(bool bIsOut) const
    {
        // The route needs reconfiguration except if:
        //      - still borrowed by the same stream
        //      - HAC mode has not changed
        //      - TTY direction has not changed
        if ((CAudioRoute::needReconfiguration(bIsOut) &&
                    _pPlatformState->hasPlatformStateChanged(
                        CAudioPlatformState::EHacModeChange |
                        CAudioPlatformState::ETtyDirectionChange)) ||
                 CAudioStreamRoute::needReconfiguration(bIsOut)) {

            return true;
        }
        return false;
    }

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t uiFlags = 0) const {

        if (!_pPlatformState->isSharedI2SBusAvailable()) {

            return false;
        }
        return CAudioStreamRoute::isApplicable(uidevices, iMode, bIsOut, uiFlags);
    }
};

class CAudioStreamRouteBtComm : public CAudioStreamRoute
{
public:
    CAudioStreamRouteBtComm(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioStreamRoute(uiRouteIndex, pPlatformState) {
    }

    bool needReconfiguration(bool bIsOut) const
    {
        // The route needs reconfiguration except if:
        //      - still borrowed by the same stream
        //      - bluetooth noise reduction and echo cancelation has not changed
        if ((CAudioRoute::needReconfiguration(bIsOut) &&
                    _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EBtHeadsetNrEcChange)) ||
                 CAudioStreamRoute::needReconfiguration(bIsOut)) {

            return true;
        }
        return false;
    }

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t uiFlags = 0) const {

        // BT module must be off and as the BT is on the shared I2S bus
        // the modem must be alive as well to use this route
        if (!_pPlatformState->isSharedI2SBusAvailable() || !_pPlatformState->isBtEnabled()) {

            return false;
        }
        return CAudioStreamRoute::isApplicable(uidevices, iMode, bIsOut, uiFlags);
    }
};

class CAudioStreamRouteModemMix : public CAudioStreamRoute
{
public:
    CAudioStreamRouteModemMix(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioStreamRoute(uiRouteIndex, pPlatformState) {
    }

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t uiFlags = 0) const {

        // BT module must be off and as the BT is on the shared I2S bus
        // the modem must be alive as well to use this route
        if (!_pPlatformState->isModemAudioAvailable()) {

            return false;
        }
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

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t uiFlags = 0) const {

        // BT module must be off and as the BT is on the shared I2S bus
        // the modem must be alive as well to use this route
        if (!_pPlatformState->isModemAudioAvailable()) {

            return false;
        }
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

    virtual bool needReconfiguration(bool bIsOut) const
    {
        if (CAudioExternalRoute::needReconfiguration(bIsOut) && bIsOut && _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EOutputDevicesChange)) {
            return true;
        }
        return false;
    }

#else
    // Route order - for external until Manager under PFW
    virtual status_t route(bool bIsOut) {

        return NO_ERROR;
    }

    // UnRoute order - for external until Manager under PFW
    virtual void unRoute(bool bIsOut) {
    }
#endif
};

class CAudioExternalRouteBtCSV : public CAudioExternalRoute
{
public:
    CAudioExternalRouteBtCSV(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioExternalRoute(uiRouteIndex, pPlatformState) {
    }

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t uiFlags = 0) const {

        // BT module must be off and as the BT is on the shared I2S bus
        // the modem must be alive as well to use this route
        if (!_pPlatformState->isModemAudioAvailable() || !_pPlatformState->isBtEnabled()) {

            return false;
        }
        if (!bIsOut) {

            // Input has no meaning except if this route is borrowed in output
            return willBeBorrowed(OUTPUT);
        }
        return CAudioExternalRoute::isApplicable(uidevices, iMode, bIsOut);
    }
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

    case Media_Offset:
        return new CAudioStreamRouteMedia(uiRouteId, pPlatformState);
        break;

    case CompressedMedia_Offset:
        return new CAudioCompressedStreamRoute(uiRouteId, pPlatformState);
        break;

    case HwCodecFm_Offset:
        return new CAudioExternalRouteHwCodecFm(uiRouteId, pPlatformState);
        break;

    case ModemMix_Offset:
        return new CAudioStreamRouteModemMix(uiRouteId, pPlatformState);
        break;

    case HwCodecComm_Offset:
        return new CAudioStreamRouteHwCodecComm(uiRouteId, pPlatformState);
        break;

    case BtComm_Offset:
        return new CAudioStreamRouteBtComm(uiRouteId, pPlatformState);
        break;

    case HwCodecMedia_Offset:
        return new CAudioExternalRoute(uiRouteId, pPlatformState);
        break;

    case HwCodecCSV_Offset:
        return new CAudioExternalRouteHwCodecCSV(uiRouteId, pPlatformState);
        break;

    case BtCSV_Offset:
        return new CAudioExternalRouteBtCSV(uiRouteId, pPlatformState);
        break;

    default:
        assert(0);
    }
    return NULL;
}

}        // namespace android

