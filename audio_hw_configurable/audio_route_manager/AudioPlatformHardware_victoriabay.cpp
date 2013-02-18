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
#include <audio_effects/effect_aec.h>
#include <audio_effects/effect_agc.h>
#include <audio_effects/effect_ns.h>


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

using namespace std;

namespace android_audio_legacy
{
// For playback, configure ALSA to start the transfer when the
// first period is full.
// For recording, configure ALSA to start the transfer on the
// first frame.
static const pcm_config pcm_config_media_playback = {
    channels            : 2,
    rate                : SAMPLE_RATE_48000,
    period_size         : PLAYBACK_48000_PERIOD_SIZE,
    period_count        : NB_RING_BUFFER_NORMAL,
    format              : PCM_FORMAT_S16_LE,
    start_threshold     : (PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL) - 1,
    stop_threshold      : PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
    silence_threshold   : 0,
    avail_min           : PLAYBACK_48000_PERIOD_SIZE,
};

static const pcm_config pcm_config_media_capture = {
    channels            : 2,
    rate                : SAMPLE_RATE_48000,
    period_size         : CAPTURE_48000_PERIOD_SIZE,
    period_count        : NB_RING_BUFFER_NORMAL,
    format              : PCM_FORMAT_S16_LE,
    start_threshold     : 1,
    stop_threshold      : CAPTURE_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
    silence_threshold   : 0,
    avail_min           : CAPTURE_48000_PERIOD_SIZE,
};

static const pcm_config pcm_config_voice_downlink = {
    channels        : 2,
    rate            : SAMPLE_RATE_16000,
    period_size     : VOICE_16000_PERIOD_SIZE,
    period_count    : NB_RING_BUFFER_INCALL,
    format          : PCM_FORMAT_S16_LE,
    start_threshold : VOICE_16000_PERIOD_SIZE - 1,
    stop_threshold  : VOICE_16000_PERIOD_SIZE * NB_RING_BUFFER_INCALL,
    silence_threshold : 0,
    avail_min       : VOICE_16000_PERIOD_SIZE,
};

static const pcm_config pcm_config_voice_uplink = {
    channels        : 2,
    rate            : SAMPLE_RATE_16000,
    period_size     : VOICE_16000_PERIOD_SIZE,
    period_count    : NB_RING_BUFFER_INCALL,
    format          : PCM_FORMAT_S16_LE,
    start_threshold : 1,
    stop_threshold  : VOICE_16000_PERIOD_SIZE * NB_RING_BUFFER_INCALL,
    silence_threshold : 0,
    avail_min       : VOICE_16000_PERIOD_SIZE,
};

static const pcm_config pcm_config_voice_mixing_playback = {
    channels            : 2,
    rate                : SAMPLE_RATE_48000,
    period_size         : VOICE_48000_PERIOD_SIZE,
    period_count        : NB_RING_BUFFER_INCALL,
    format              : PCM_FORMAT_S16_LE,
    start_threshold     : VOICE_48000_PERIOD_SIZE - 1,
    stop_threshold      : VOICE_48000_PERIOD_SIZE * NB_RING_BUFFER_INCALL,
    silence_threshold   : 0,
    avail_min           : NB_RING_BUFFER_INCALL,
};

static const pcm_config pcm_config_voice_mixing_capture = {
    channels            : 2,
    rate                : SAMPLE_RATE_48000,
    period_size         : PLAYBACK_48000_PERIOD_SIZE / 2,
    period_count        : NB_RING_BUFFER_INCALL,
    format              : PCM_FORMAT_S16_LE,
    start_threshold     : 1,
    stop_threshold      : VOICE_48000_PERIOD_SIZE * NB_RING_BUFFER_INCALL,
    silence_threshold   : 0,
    avail_min           : NB_RING_BUFFER_INCALL,
};

/// Port section

const char* const CAudioPlatformHardware::_acPorts[] = {
    "LPE_I2S2_PORT",
    "LPE_I2S3_PORT",
    "IA_I2S0_PORT",
    "IA_I2S1_PORT",
    "MODEM_I2S_PORT",
    "BT_I2S_PORT",
    "FM_I2S_PORT",
    "HWCODEC_ASP_PORT",
    "HWCODEC_VSP_PORT",
    "HWCODEC_AUX_PORT"
};

const char* const CAudioPlatformHardware::_acPortGroups[] = {
    "IA_I2S1_PORT,MODEM_I2S_PORT",
    "BT_I2S_PORT,FM_I2S_PORT"
};

/// Route section
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
    //
    // MEDIA Route
    //
    {
        "Media",
        CAudioRoute::EStreamRoute,
        "",
        {
            DEVICE_IN_BUILTIN_ALL,
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
        {
            { CSampleSpec::ECopy, CSampleSpec::ECopy },
            { CSampleSpec::ECopy, CSampleSpec::ECopy }
        },
        ""
    },
    //
    // Compressed Media Route
    //
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
        {
            { CSampleSpec::ECopy, CSampleSpec::ECopy },
            { CSampleSpec::ECopy, CSampleSpec::ECopy }
        },
        ""
    },
    //
    // Voice Mix route (IE mixing during CSV, VoIP or VoLTE call)
    // Once VoIP DL uses a Direct Output, it will be used to mix streams during VoIP calls
    // so update applicability accordingly...
    //
    {
        "ModemMix",
        CAudioRoute::EStreamRoute,
        "IA_I2S0_PORT",
        {
            AudioSystem::DEVICE_IN_ALL,
            DEVICE_OUT_MM_ALL | DEVICE_OUT_BLUETOOTH_SCO_ALL
        },
        {
            (1 << AUDIO_SOURCE_VOICE_UPLINK) | (1 << AUDIO_SOURCE_VOICE_DOWNLINK) |
            (1 << AUDIO_SOURCE_VOICE_CALL) | (1 << AUDIO_SOURCE_MIC) |
            (1 << AUDIO_SOURCE_CAMCORDER) | (1 << AUDIO_SOURCE_VOICE_RECOGNITION),

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
        {
            { CSampleSpec::ECopy, CSampleSpec::ECopy },
            { CSampleSpec::EAverage, CSampleSpec::EAverage }
        },
        ""
    },
    //
    // VOICE (VoIP) (on any output device) Route
    // May be accessed by Direct Output streams
    //
    {
        "HwCodecComm",
        CAudioRoute::EStreamRoute,
        "IA_I2S1_PORT,HWCODEC_VSP_PORT",
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
        {
            { CSampleSpec::ECopy, CSampleSpec::ECopy },
            { CSampleSpec::ECopy, CSampleSpec::ECopy }
        },
        ""
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
        {
            channel_policy_not_applicable,
            channel_policy_not_applicable
        },
        "CompressedMedia,Media"
    },
    //
    // CSV external route (on any output device)
    //
    {
        "HwCodecCSV",
        CAudioRoute::EExternalRoute,
        "MODEM_I2S_PORT,HWCODEC_VSP_PORT",
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
        {
            channel_policy_not_applicable,
            channel_policy_not_applicable
        },
        ""
    },
    //
    // BT External route
    //
    {
        "HwCodecBt",
        CAudioRoute::EExternalRoute,
        "HWCODEC_AUX_PORT,BT_I2S_PORT",
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
        {
            channel_policy_not_applicable,
            channel_policy_not_applicable
        },
        "HwCodecCSV,HwCodecComm"
    },
    //
    // FM route
    //
    {
        "HwCodecFm",
        CAudioRoute::EExternalRoute,
        "HWCODEC_AUX_PORT,FM_I2S_PORT",
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
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
        {
            channel_policy_not_applicable,
            channel_policy_not_applicable
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
        {
            channel_policy_not_applicable,
            channel_policy_not_applicable
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
    CAudioStreamRouteHwCodecComm(uint32_t uiRouteIndex, CAudioPlatformState* pPlatformState) :
        CAudioStreamRoute(uiRouteIndex, pPlatformState) {

        _pEffectSupported.push_back(FX_IID_AGC);
        _pEffectSupported.push_back(FX_IID_AEC);
        _pEffectSupported.push_back(FX_IID_NS);
    }

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t uiMask = 0) const {

        if (!_pPlatformState->isSharedI2SBusAvailable()) {

            return false;
        }
        // This case cannot be handled through the route structure
        if ((iMode == AudioSystem::MODE_NORMAL) && (uidevices & DEVICE_BLUETOOTH_SCO_ALL(bIsOut))) {

            return true;
        }
        return CAudioStreamRoute::isApplicable(uidevices, iMode, bIsOut, uiMask);
    }

    virtual bool needReconfiguration(bool bIsOut) const
    {
        // The route needs reconfiguration except if:
        //      - still used by the same stream
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
    CAudioExternalRouteHwCodecBt(uint32_t uiRouteIndex, CAudioPlatformState* pPlatformState) :
        CAudioExternalRoute(uiRouteIndex, pPlatformState) {
    }

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t __UNUSED uiMask = 0) const {

        // For Victoria Bay platform, set Bluetooth always enabled when a BT route
        // is needed. This change will be reverted as soon as the Bluedroid stack
        // will be integrated as the key telling the audio manager that the Bluetooth
        // is on comes from the BT service.
#ifdef VB_HAL_AUDIO_TEMP
        _pPlatformState->setBtEnabled(true);
#endif

        // BT module must be on and as the BT is on the shared I2S bus
        // the share bus must be available
        if (!_pPlatformState->isBtEnabled()) {

            return false;
        }
        if (!bIsOut && (iMode == AudioSystem::MODE_IN_CALL)) {

            // In Voice CALL, the audio policy does not give any input device
            // So, Input has no meaning except if this route is used in output
            return willBeUsed(CUtils::EOutput);
        }
        return CAudioExternalRoute::isApplicable(uidevices, iMode, bIsOut);
    }
};

class CAudioExternalRouteHwCodecCSV : public CAudioExternalRoute
{
public:
    CAudioExternalRouteHwCodecCSV(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioExternalRoute(uiRouteIndex, pPlatformState) {

    }

  ~CAudioExternalRouteHwCodecCSV() {
  }

    //
    // This route is applicable in CALL mode, whatever the output device selected
    //
    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t __UNUSED uiMask = 0) const {

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
};

class CAudioExternalRouteVirtualASP : public CAudioExternalRoute
{
public:
    CAudioExternalRouteVirtualASP(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioExternalRoute(uiRouteIndex, pPlatformState)
    {
    }

    virtual bool isApplicable(uint32_t __UNUSED uidevices, int __UNUSED iMode, bool bIsOut, uint32_t __UNUSED uiMask) const
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

        return new CAudioStreamRouteMedia(uiRouteIndex, pPlatformState);

    } else if (strName == "CompressedMedia") {

        return new CAudioCompressedStreamRoute(uiRouteIndex, pPlatformState);

    } else if (strName == "HwCodecFm") {

        return new CAudioExternalRouteHwCodecFm(uiRouteIndex, pPlatformState);

    } else if (strName == "ModemMix") {

        return new CAudioStreamRoute(uiRouteIndex, pPlatformState);

    } else if (strName == "HwCodecComm") {

        return new CAudioStreamRouteHwCodecComm(uiRouteIndex, pPlatformState);

    } else if (strName == "HwCodecMedia") {

        return new CAudioExternalRoute(uiRouteIndex, pPlatformState);

    } else if (strName == "HwCodecCSV") {

        return new CAudioExternalRouteHwCodecCSV(uiRouteIndex, pPlatformState);

    } else if (strName == "HwCodecBt") {

        return new CAudioExternalRouteHwCodecBt(uiRouteIndex, pPlatformState);

    } else if (strName == "VirtualASP") {

        return new CAudioExternalRouteVirtualASP(uiRouteIndex, pPlatformState);
    }
    ALOGE("%s: wrong route index=%d", __FUNCTION__, uiRouteIndex);
    return NULL;
}

}        // namespace android

