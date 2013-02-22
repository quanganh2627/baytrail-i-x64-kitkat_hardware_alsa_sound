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

#include "AudioPlatformHardware.h"
#include "Property.h"

#include <tinyalsa/asoundlib.h>

#define PLAYBACK_PERIOD_TIME_MS         ((int)24)
#define VOICE_PERIOD_TIME_MS            ((int)20)

#define LONG_PERIOD_FACTOR              ((int)2)
#define SEC_PER_MSEC                    ((int)1000)

/// May add a new route, include header here...
#define NB_RING_BUFFER_NORMAL           ((int)2)
#define NB_RING_BUFFER_INCALL           ((int)4)

#define SAMPLE_RATE_8000                ((int)8000)
#define SAMPLE_RATE_48000               ((int)48000)

#define VOICE_8000_PERIOD_SIZE          ((int)VOICE_PERIOD_TIME_MS * SAMPLE_RATE_8000 / SEC_PER_MSEC)  // 20ms @ 8k
#define PLAYBACK_48000_PERIOD_SIZE      ((int)PLAYBACK_PERIOD_TIME_MS * LONG_PERIOD_FACTOR * SAMPLE_RATE_48000 / SEC_PER_MSEC) //(24000*2 * 48000 / USEC_PER_SEC)
#define CAPTURE_48000_PERIOD_SIZE       ((int)VOICE_PERIOD_TIME_MS * LONG_PERIOD_FACTOR * SAMPLE_RATE_48000 / SEC_PER_MSEC) //(20000*2 * 48000 / USEC_PER_SEC)
#define VOICE_48000_PERIOD_SIZE         ((int)VOICE_PERIOD_TIME_MS * SAMPLE_RATE_48000 / SEC_PER_MSEC)  //(20000 * 48000 / USEC_PER_SEC)



static const char* MEDIA_CARD_NAME = "cloverviewaudio";
#define MEDIA_PLAYBACK_DEVICE_ID        ((int)0)
#define MEDIA_CAPTURE_DEVICE_ID         ((int)0)


static const char* VOICE_MIXING_CARD_NAME = "cloverviewaudio";
#define VOICE_MIXING_DEVICE_ID          ((int)5)
#define VOICE_RECORD_DEVICE_ID          ((int)5)

static const char* VOICE_CARD_NAME = "cloverviewaudio";
#define VOICE_HWCODEC_DOWNLINK_DEVICE_ID    ((int)4)
#define VOICE_HWCODEC_UPLINK_DEVICE_ID      ((int)4)
#define VOICE_BT_DOWNLINK_DEVICE_ID         ((int)3)
#define VOICE_BT_UPLINK_DEVICE_ID           ((int)3)

using namespace std;

namespace android_audio_legacy
{
// For playback, configure ALSA to start the transfer when the
// first period is full.
// For recording, configure ALSA to start the transfer on the
// first frame.
static const pcm_config pcm_config_media_playback = {
   channels          : 2,
   rate              : SAMPLE_RATE_48000,
   period_size       : PLAYBACK_48000_PERIOD_SIZE,
   period_count      : NB_RING_BUFFER_NORMAL,
   format            : PCM_FORMAT_S16_LE,
   start_threshold   : PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL - 1,
   stop_threshold    : PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
   silence_threshold : 0,
   avail_min         : PLAYBACK_48000_PERIOD_SIZE,
};

static const pcm_config pcm_config_media_capture = {
    channels          : 2,
    rate              : SAMPLE_RATE_48000,
    period_size       : CAPTURE_48000_PERIOD_SIZE,
    period_count      : NB_RING_BUFFER_NORMAL,
    format            : PCM_FORMAT_S16_LE,
    start_threshold   : 1,
    stop_threshold    : CAPTURE_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
    silence_threshold : 0,
    avail_min         : CAPTURE_48000_PERIOD_SIZE,
};

static const pcm_config pcm_config_voice_bt_downlink = {
    channels          : 1,
    rate              : SAMPLE_RATE_8000,
    period_size       : VOICE_8000_PERIOD_SIZE,
    period_count      : NB_RING_BUFFER_INCALL,
    format            : PCM_FORMAT_S16_LE,
    start_threshold   : VOICE_8000_PERIOD_SIZE - 1,
    stop_threshold    : VOICE_8000_PERIOD_SIZE * NB_RING_BUFFER_INCALL,
    silence_threshold : 0,
    avail_min         : VOICE_8000_PERIOD_SIZE,
};

static const pcm_config pcm_config_voice_bt_uplink = {
    channels          : 1,
    rate              : SAMPLE_RATE_8000,
    period_size       : VOICE_8000_PERIOD_SIZE,
    period_count      : NB_RING_BUFFER_INCALL,
    format            : PCM_FORMAT_S16_LE,
    start_threshold   : 0,
    stop_threshold    : 0,
    silence_threshold : 0,
    avail_min         : 0,
};

static const pcm_config pcm_config_voice_hwcodec_downlink = {
    channels          : 2,
    rate              : SAMPLE_RATE_48000,
    period_size       : VOICE_48000_PERIOD_SIZE,
    period_count      : NB_RING_BUFFER_INCALL,
    format            : PCM_FORMAT_S16_LE,
    start_threshold   : VOICE_48000_PERIOD_SIZE - 1,
    stop_threshold    : VOICE_48000_PERIOD_SIZE * NB_RING_BUFFER_INCALL,
    silence_threshold : 0,
    avail_min         :  VOICE_48000_PERIOD_SIZE,
};

static const pcm_config pcm_config_voice_hwcodec_uplink = {
    channels          : 2,
    rate              : SAMPLE_RATE_48000,
    period_size       : VOICE_48000_PERIOD_SIZE,
    period_count      : NB_RING_BUFFER_INCALL,
    format            : PCM_FORMAT_S16_LE,
    start_threshold   : 1,
    stop_threshold    : VOICE_48000_PERIOD_SIZE * NB_RING_BUFFER_INCALL,
    silence_threshold : 0,
    avail_min         : VOICE_48000_PERIOD_SIZE,
};

static const pcm_config pcm_config_voice_mixing_playback = {
    channels          : 2,
    rate              : SAMPLE_RATE_48000,
    period_size       : VOICE_48000_PERIOD_SIZE,
    period_count      : NB_RING_BUFFER_INCALL,
    format            : PCM_FORMAT_S16_LE,
    start_threshold   : VOICE_48000_PERIOD_SIZE - 1,
    stop_threshold    : VOICE_48000_PERIOD_SIZE * NB_RING_BUFFER_INCALL,
    silence_threshold : 0,
    avail_min         : VOICE_48000_PERIOD_SIZE,
};


static const pcm_config pcm_config_voice_mixing_capture = {
    channels          : 2,
    rate              : SAMPLE_RATE_48000,
    period_size       : VOICE_48000_PERIOD_SIZE,
    period_count      : NB_RING_BUFFER_INCALL,
    format            : PCM_FORMAT_S16_LE,
    start_threshold   : 0,
    stop_threshold    : 0,
    silence_threshold : 0,
    avail_min         : 0,
};

const char* const CAudioPlatformHardware::_acPorts[] = {
    "LPE_I2S2_PORT",
    "LPE_I2S3_PORT",
    "IA_I2S0_PORT",
    "IA_I2S1_PORT",
    "MODEM_I2S1_PORT",
    "MODEM_I2S2_PORT",
    "BT_I2S_PORT",
    "FM_I2S_PORT",
    "HWCODEC_ASP_PORT",
    "HWCODEC_VSP_PORT",
    "HWCODEC_AUX_PORT",
};


// Port Group and associated port
// Group0: "HwCodecMediaPort", "HwCodecVoicePort"
// Group1: "BTPort", "HwCodecVoicePort"
// Group2: "IACommPort", "ModemCsvPort"
const char* const CAudioPlatformHardware::_acPortGroups[] = {
    "HWCODEC_ASP_PORT,HWCODEC_VSP_PORT",
    "HWCODEC_VSP_PORT,BT_I2S_PORT",
    "IA_I2S1_PORT,MODEM_I2S1_PORT"
};

const char* CAudioPlatformHardware::CODEC_DELAY_PROP_NAME = "Audio.Media.CodecDelayMs";

//
// Route description structure
//
const CAudioPlatformHardware::s_route_t CAudioPlatformHardware::_astAudioRoutes[] = {
    ////////////////////////////////////////////////////////////////////////
    //
    // Streams routes
    //
    ////////////////////////////////////////////////////////////////////////
    {
        "Media",
        CAudioRoute::EStreamRoute,
        "",
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
        ""
    },
    {
        "CompressedMedia",
        CAudioRoute::ECompressedStreamRoute,
        "",
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
        ""
    },
    {
        "ModemMix",
        CAudioRoute::EStreamRoute,
        "IA_I2S0_PORT,MODEM_I2S2_PORT",
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
        ""
    },
    {
        "HwCodecComm",
        CAudioRoute::EStreamRoute,
        "IA_I2S1_PORT,HWCODEC_VSP_PORT",
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
        ""
    },
    {
        "BtComm",
        CAudioRoute::EStreamRoute,
        "IA_I2S1_PORT,BT_I2S_PORT",
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
        ""
    },
    ////////////////////////////////////////////////////////////////////////
    //
    // External routes
    //
    ////////////////////////////////////////////////////////////////////////
    {
        "HwCodecMedia",
        CAudioRoute::EExternalRoute,
        "LPE_I2S3_PORT,HWCODEC_ASP_PORT",
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
        "CompressedMedia,Media"
    },
    {
        "HwCodecCSV",
        CAudioRoute::EExternalRoute,
        "MODEM_I2S1_PORT,HWCODEC_VSP_PORT",
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
        ""
    },
    {
        "BtCSV",
        CAudioRoute::EExternalRoute,
        "MODEM_I2S1_PORT,BT_I2S_PORT",
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
        ""
    },
    {
        "HwCodecFm",
        CAudioRoute::EExternalRoute,
        "FM_I2S_PORT,LPE_I2S2_PORT",
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
        ""
    },
    {
        "VirtualASP",
        CAudioRoute::EExternalRoute,
        "",
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
        },
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
        },
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
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
        ""
    }
};


const uint32_t CAudioPlatformHardware::_uiNbPortGroups = sizeof(CAudioPlatformHardware::_acPortGroups) /
        sizeof(CAudioPlatformHardware::_acPortGroups[0]);

const uint32_t CAudioPlatformHardware::_uiNbPorts = sizeof(CAudioPlatformHardware::_acPorts) /
        sizeof(CAudioPlatformHardware::_acPorts[0]);

const uint32_t CAudioPlatformHardware::_uiNbRoutes = sizeof(CAudioPlatformHardware::_astAudioRoutes) /
        sizeof(CAudioPlatformHardware::_astAudioRoutes[0]);

//
// Specific Applicability Rules
//
class CAudioStreamRouteMedia : public CAudioStreamRoute
{
public:
    CAudioStreamRouteMedia(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioStreamRoute(uiRouteIndex, pPlatformState),
        _uiCodecDelayMs(TProperty<int32_t>(CAudioPlatformHardware::CODEC_DELAY_PROP_NAME, 0))
    {
    }
    // Get amount of silence delay upon stream opening
    virtual uint32_t getOutputSilencePrologMs() const
    {

        if (((_pPlatformState->getDevices(true) & AudioSystem::DEVICE_OUT_SPEAKER) == AudioSystem::DEVICE_OUT_SPEAKER) ||
                ((_pPlatformState->getDevices(true) & AudioSystem::DEVICE_OUT_WIRED_HEADSET) == AudioSystem::DEVICE_OUT_WIRED_HEADSET)) {

            return _uiCodecDelayMs;
        }
        return CAudioStreamRoute::getOutputSilencePrologMs();
    }
private:
    uint32_t _uiCodecDelayMs;
};

class CAudioStreamRouteHwCodecComm : public CAudioStreamRoute
{
public:
    CAudioStreamRouteHwCodecComm(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioStreamRoute(uiRouteIndex, pPlatformState) {
    }

    virtual bool needReconfiguration(bool bIsOut) const
    {
        // The route needs reconfiguration except if:
        //      - still used by the same stream
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

    virtual bool needReconfiguration(bool bIsOut) const
    {
        // The route needs reconfiguration except if:
        //      - still used by the same stream
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

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t __UNUSED uiFlags = 0 ) const {

        // BT module must be off and as the BT is on the shared I2S bus
        // the modem must be alive as well to use this route
        if (!_pPlatformState->isModemAudioAvailable()) {

            return false;
        }
        if (!bIsOut) {

            // Input has no meaning except if this route is used in output
            return willBeUsed(CUtils::EOutput);
        }
        return CAudioExternalRoute::isApplicable(uidevices, iMode, bIsOut);
    }
};

class CAudioExternalRouteBtCSV : public CAudioExternalRoute
{
public:
    CAudioExternalRouteBtCSV(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioExternalRoute(uiRouteIndex, pPlatformState) {
    }

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t __UNUSED uiFlags = 0) const {

        // BT module must be off and as the BT is on the shared I2S bus
        // the modem must be alive as well to use this route
        if (!_pPlatformState->isModemAudioAvailable() || !_pPlatformState->isBtEnabled()) {

            return false;
        }
        if (!bIsOut) {

            // Input has no meaning except if this route is used in output
            return willBeUsed(CUtils::EOutput);
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

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t __UNUSED uiFlags) const
    {
        // BT module must be off and as the BT is on the shared I2S bus
        // the modem must be alive as well to use this route
        if (!_pPlatformState->getFmRxHwMode()) {

            return false;
        }
        return CAudioExternalRoute::isApplicable(uidevices, iMode, bIsOut);
    }
};

class CAudioExternalRouteHwCodecMedia : public CAudioExternalRoute
{
public:
    CAudioExternalRouteHwCodecMedia(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioExternalRoute(uiRouteIndex, pPlatformState)
    {
    }

    virtual bool needReconfiguration(bool bIsOut) const
    {
        // The route needs reconfiguration except if:
        //      - output devices did not change
        if (bIsOut) {

            return CAudioRoute::needReconfiguration(bIsOut) &&
                    (_pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EOutputDevicesChange) ||
                     _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EHwModeChange));
        }
        return CAudioRoute::needReconfiguration(bIsOut) &&
                (_pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EInputDevicesChange) ||
                 _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EInputSourceChange) ||
                 _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EHwModeChange));
    }
};

class CAudioExternalRouteVirtualASP : public CAudioExternalRoute
{
public:
    CAudioExternalRouteVirtualASP(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioExternalRoute(uiRouteIndex, pPlatformState)
    {
    }

    virtual bool isApplicable(uint32_t __UNUSED uidevices, int __UNUSED iMode, bool bIsOut, uint32_t __UNUSED uiFlags) const
    {
        // BT module must be off and as the BT is on the shared I2S bus
        // the modem must be alive as well to use this route
        if (bIsOut && _pPlatformState->isScreenOn()) {

            return true;
        }
        return false;
    }

    virtual bool needReconfiguration(bool bIsOut) const
    {
        // The route needs reconfiguration except if:
        //      - output devices did not change
        if (CAudioRoute::needReconfiguration(bIsOut) &&
                    _pPlatformState->hasPlatformStateChanged(CAudioPlatformState::EOutputDevicesChange)) {

            return true;
        }
        return false;
    }
};

//
// Once all deriavated class exception has been removed
// replace this function by a generic route creator according to the route type
//
CAudioRoute* CAudioPlatformHardware::createAudioRoute(uint32_t uiRouteIndex, CAudioPlatformState* pPlatformState)
{
    LOG_ALWAYS_FATAL_IF(pPlatformState == NULL);

    const string strName = getRouteName(uiRouteIndex);

    if (strName == "Media") {

        return new CAudioStreamRoute(uiRouteIndex, pPlatformState);

    } else if (strName == "CompressedMedia") {

        return new CAudioCompressedStreamRoute(uiRouteIndex, pPlatformState);

    } else if (strName == "HwCodecFm") {

        return new CAudioExternalRouteHwCodecFm(uiRouteIndex, pPlatformState);

    } else if (strName == "ModemMix") {

        return new CAudioStreamRouteModemMix(uiRouteIndex, pPlatformState);

    } else if (strName == "HwCodecComm") {

        return new CAudioStreamRouteHwCodecComm(uiRouteIndex, pPlatformState);

    } else if (strName == "BtComm") {

        return new CAudioStreamRouteBtComm(uiRouteIndex, pPlatformState);

    } else if (strName == "HwCodecMedia") {

        return new CAudioExternalRouteHwCodecMedia(uiRouteIndex, pPlatformState);

    } else if (strName == "HwCodecCSV") {

        return new CAudioExternalRouteHwCodecCSV(uiRouteIndex, pPlatformState);

    } else if (strName == "BtCSV") {

        return new CAudioExternalRouteBtCSV(uiRouteIndex, pPlatformState);

    } else if (strName == "VirtualASP") {

        return new CAudioExternalRouteVirtualASP(uiRouteIndex, pPlatformState);
    }
    ALOGE("%s: wrong route index=%d", __FUNCTION__, uiRouteIndex);
    return NULL;
}

}        // namespace android

