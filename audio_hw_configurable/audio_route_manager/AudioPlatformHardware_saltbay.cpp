/*
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
#include "Property.h"
#include <audio_effects/effect_aec.h>
#include <audio_effects/effect_agc.h>
#include <audio_effects/effect_ns.h>
#include <tinyalsa/asoundlib.h>
#include <sstream>
#include <fstream>

/**
 *  24 ms makes a PCM frames count of 1152 which is aligned on a 16-frames
 *  boundary. This is the best optimized size for a buffer in the LPE.
 */
#define PLAYBACK_PERIOD_TIME_MS         ((int)24)
/**
 * 20 ms makes a PCM frames count of 960 which is aligned on a 16-frames
 * boundary. Also 20 ms is optimized for Audioflinger's minimal buffer size.
 */
#define CAPTURE_PERIOD_TIME_MS          ((int)20)
#define SEC_PER_MSEC                    ((int)1000)

#define SAMPLE_RATE_16000               (16000)  /**< 16Khz sample rate is used for VoIP */
#define SAMPLE_RATE_48000               (48000)  /**< 48Khz sample rate is used for Media */
/**
 * Voice downlink period size of 384 frames
 */
#define VOICE_DL_16000_PERIOD_SIZE      ((int)PLAYBACK_PERIOD_TIME_MS * SAMPLE_RATE_16000 / SEC_PER_MSEC)
/**
 * Voice uplink period size of 320 frames
 */
#define VOICE_UL_16000_PERIOD_SIZE      ((int)CAPTURE_PERIOD_TIME_MS * SAMPLE_RATE_16000 / SEC_PER_MSEC)
/**
 * Media playback period size of 1152 frames
 */
#define PLAYBACK_48000_PERIOD_SIZE      ((int)PLAYBACK_PERIOD_TIME_MS * SAMPLE_RATE_48000 / SEC_PER_MSEC)
/**
 * Media capture period size of 960 frames
 */
#define CAPTURE_48000_PERIOD_SIZE       ((int)CAPTURE_PERIOD_TIME_MS * SAMPLE_RATE_48000 / SEC_PER_MSEC)

/**
 * Use of 4 periods ring buffer
 */
#define NB_RING_BUFFER                  (4)

/**
 * Audio card for Media streams
 */
#define MEDIA_PLAYBACK_DEVICE_ID        (0)
#define MEDIA_CAPTURE_DEVICE_ID         (0)

/**
 * Audio card for VoIP calls
 */
#define VOICE_DOWNLINK_DEVICE_ID        (2)
#define VOICE_UPLINK_DEVICE_ID          (2)

namespace android_audio_legacy
{
// For playback, configure ALSA to start the transfer when the
// first period is full.
// For recording, configure ALSA to start the transfer on the
// first frame.
const pcm_config CAudioPlatformHardware::pcm_config_media_playback = {
    channels        : 2,
    rate            : SAMPLE_RATE_48000,
    period_size     : PLAYBACK_48000_PERIOD_SIZE,
    period_count    : NB_RING_BUFFER,
    format          : PCM_FORMAT_S16_LE,
    start_threshold : PLAYBACK_48000_PERIOD_SIZE - 1,
    stop_threshold  : PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER,
    silence_threshold : 0,
    avail_min       : PLAYBACK_48000_PERIOD_SIZE,
};

const pcm_config CAudioPlatformHardware::pcm_config_media_capture = {
    channels        : 2,
    rate            : SAMPLE_RATE_48000,
    period_size     : CAPTURE_48000_PERIOD_SIZE,
    period_count    : NB_RING_BUFFER,
    format          : PCM_FORMAT_S16_LE,
    start_threshold : 1,
    stop_threshold  : CAPTURE_48000_PERIOD_SIZE * NB_RING_BUFFER,
    silence_threshold : 0,
    avail_min       : CAPTURE_48000_PERIOD_SIZE,
};

const pcm_config CAudioPlatformHardware::pcm_config_deep_media_playback =
                CAudioPlatformHardware::pcm_config_media_playback;

static const pcm_config pcm_config_voice_downlink = {
    channels        : 2,
    rate            : SAMPLE_RATE_16000,
    period_size     : VOICE_DL_16000_PERIOD_SIZE,
    period_count    : NB_RING_BUFFER,
    format          : PCM_FORMAT_S16_LE,
    start_threshold : VOICE_DL_16000_PERIOD_SIZE,
    stop_threshold  : VOICE_DL_16000_PERIOD_SIZE * NB_RING_BUFFER,
    silence_threshold : 0,
    avail_min       : VOICE_DL_16000_PERIOD_SIZE,
};

static const pcm_config pcm_config_voice_uplink = {
    channels        : 2,
    rate            : SAMPLE_RATE_16000,
    period_size     : VOICE_UL_16000_PERIOD_SIZE,
    period_count    : NB_RING_BUFFER,
    format          : PCM_FORMAT_S16_LE,
    start_threshold : 1,
    stop_threshold  : VOICE_UL_16000_PERIOD_SIZE * NB_RING_BUFFER,
    silence_threshold : 0,
    avail_min       : VOICE_UL_16000_PERIOD_SIZE,
};

// Location of the file containing the name of the card
const char* const cardsPath = "/proc/asound/cards";

// Name of the card (boomer)
const char* lm49453audioCardName = "lm49453audio";

// Name of the card (wm8958)
const char* wm8958audioCardName = "wm8958audio";

// Name of the card (unknown)
const char* unknownAudioCardName = "unknown-audio-codec";

// Retrieve the name of the current card
const char* const mediaCardName = getCodecName();
const char* const voiceCardName = getCodecName();

// Defines the name of the Android property describing the name of the PFW configuration file
const char* const gPfwConfFilePropName = "AudioComms.PFW.ConfPath";

const char* const gPfwConfFileBoomerName =
        "/etc/parameter-framework/ParameterFrameworkConfiguration-boomer.xml";

const char* const gPfwConfFileWm8958Name =
        "/etc/parameter-framework/ParameterFrameworkConfiguration-wm8958.xml";

const char* const CAudioPlatformHardware::_acPorts[] = {
};

// Port Group and associated port
const char* const CAudioPlatformHardware::_acPortGroups[] = {
};

// Read the name of the card from the file /proc/asound/cards
const char* getCodecName()
{
    ifstream istrCards(cardsPath);

    if (!istrCards.is_open()) {

        ALOGE("Error opening file %s", cardsPath);

        return unknownAudioCardName;
    }

    stringstream strmBuffer;
    strmBuffer << istrCards.rdbuf();

    if (istrCards.fail()) {

        ALOGE("Error reading file %s", cardsPath);

        return unknownAudioCardName;
    }

    string strCards(strmBuffer.str());
    istrCards.close();

    const char* card_name;
    if (strCards.find(lm49453audioCardName) != string::npos) {

        // boomer
        card_name = lm49453audioCardName;
        TProperty<string>(gPfwConfFilePropName).setValue(gPfwConfFileBoomerName);

    } else if (strCards.find(wm8958audioCardName) != string::npos) {

        // wm8958
        card_name = wm8958audioCardName;
        TProperty<string>(gPfwConfFilePropName).setValue(gPfwConfFileWm8958Name);

    } else {

        // not found
        card_name = unknownAudioCardName;
    }

    return card_name;
}

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
    // Media Route
    //
    {
        "Media",
        CAudioRoute::EStreamRoute,
        "",
        {
            DEVICE_IN_BUILTIN_ALL | DEVICE_IN_BLUETOOTH_SCO_ALL |
            AudioSystem::DEVICE_IN_VOICE_CALL,
            DEVICE_OUT_MM_ALL | DEVICE_OUT_BLUETOOTH_SCO_ALL
        },
        // When VoIP will use direct output, add AUDIO_SOURCE_IN_COMMUNICATION
        {
            (1 << AUDIO_SOURCE_VOICE_UPLINK) | (1 << AUDIO_SOURCE_VOICE_DOWNLINK) |
            (1 << AUDIO_SOURCE_VOICE_CALL) | (1 << AUDIO_SOURCE_DEFAULT) |
            (1 << AUDIO_SOURCE_MIC) | (1 << AUDIO_SOURCE_CAMCORDER) |
            (1 << AUDIO_SOURCE_VOICE_RECOGNITION),
            AUDIO_OUTPUT_FLAG_PRIMARY
        },
        // When VoIP will use direct output, add MODE_IN_COMMUNICATION
        {
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE) |
            (1 << AudioSystem::MODE_IN_CALL),
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE) |
            (1 << AudioSystem::MODE_IN_CALL)
        },
        mediaCardName,
        {
            MEDIA_CAPTURE_DEVICE_ID,
            MEDIA_PLAYBACK_DEVICE_ID
        },
        {
            CAudioPlatformHardware::pcm_config_media_capture,
            CAudioPlatformHardware::pcm_config_media_playback
        },
        {
            { CSampleSpec::ECopy, CSampleSpec::EIgnore },
            { CSampleSpec::ECopy, CSampleSpec::ECopy }
        },
        ""
    },
    //
    // Voice Route
    //
    {
        "Voice",
        CAudioRoute::EStreamRoute,
          "",
        {
            DEVICE_IN_BUILTIN_ALL | DEVICE_IN_BLUETOOTH_SCO_ALL,
            DEVICE_OUT_MM_ALL | DEVICE_OUT_BLUETOOTH_SCO_ALL
        },
        {
            (1 << AUDIO_SOURCE_MIC) | (1 << AUDIO_SOURCE_VOICE_COMMUNICATION),
            AUDIO_OUTPUT_FLAG_PRIMARY,
        },
        {
            (1 << AudioSystem::MODE_IN_COMMUNICATION),
            (1 << AudioSystem::MODE_IN_COMMUNICATION)
        },
        voiceCardName,
        {
            VOICE_UPLINK_DEVICE_ID,
            VOICE_DOWNLINK_DEVICE_ID,
        },
        {
            pcm_config_voice_uplink,
            pcm_config_voice_downlink,
        },
        {
            { CSampleSpec::ECopy, CSampleSpec::EIgnore },
            { CSampleSpec::ECopy, CSampleSpec::ECopy }
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
    ////////////////////////////////////////////////////////////////////////
    //
    // External routes
    //
    ////////////////////////////////////////////////////////////////////////
    //
    // HwCodec 0 route
    //
    {
        "HwCodec0IA",
        CAudioRoute::EExternalRoute,
        "",
        {
            AudioSystem::DEVICE_IN_BACK_MIC | AudioSystem::DEVICE_IN_AUX_DIGITAL | AudioSystem::DEVICE_IN_BUILTIN_MIC,
            AudioSystem::DEVICE_OUT_EARPIECE | AudioSystem::DEVICE_OUT_SPEAKER
        },
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
        },
        {
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE) | (1 << AudioSystem::MODE_IN_CALL) | (1 << AudioSystem::MODE_IN_COMMUNICATION),
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE) | (1 << AudioSystem::MODE_IN_CALL) | (1 << AudioSystem::MODE_IN_COMMUNICATION)
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
        "ModemIA,Voice,Media,ContextAwareness,CompressedMedia,AlwaysListening"
    },
    //
    // HWCODEC 1 route
    //
    {
        "HwCodec1IA",
        CAudioRoute::EExternalRoute,
        "",
        {
            AudioSystem::DEVICE_IN_WIRED_HEADSET,
            AudioSystem::DEVICE_OUT_WIRED_HEADPHONE | AudioSystem::DEVICE_OUT_WIRED_HEADSET
        },
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
        },
        {
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE) | (1 << AudioSystem::MODE_IN_CALL) | (1 << AudioSystem::MODE_IN_COMMUNICATION),
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE) | (1 << AudioSystem::MODE_IN_CALL) | (1 << AudioSystem::MODE_IN_COMMUNICATION)
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
        "ModemIA,Voice,Media,CompressedMedia,AlwaysListening"
    },
    //
    // ModemIA route
    //
    {
        "ModemIA",
        CAudioRoute::EExternalRoute,
        "",
        {
            NOT_APPLICABLE,
            DEVICE_OUT_MM_ALL | DEVICE_OUT_BLUETOOTH_SCO_ALL
        },
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
        },
        {
            (1 << AudioSystem::MODE_IN_CALL),
            (1 << AudioSystem::MODE_IN_CALL)
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
    // BT route
    //
    {
        "BtIA",
        CAudioRoute::EExternalRoute,
        "",
        {
            DEVICE_IN_BLUETOOTH_SCO_ALL,
            DEVICE_OUT_BLUETOOTH_SCO_ALL
        },
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
        },
        {
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE) | (1 << AudioSystem::MODE_IN_CALL) | (1 << AudioSystem::MODE_IN_COMMUNICATION),
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE) | (1 << AudioSystem::MODE_IN_CALL) | (1 << AudioSystem::MODE_IN_COMMUNICATION)
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
        "ModemIA,Voice,Media"
    },
    //
    // FM route
    //
    {
        "FMIA",
        CAudioRoute::EExternalRoute,
        "",
        {
            NOT_APPLICABLE,
            DEVICE_OUT_MM_ALL
        },
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
        },
        {
            (1 << AudioSystem::MODE_NORMAL),
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
    ////////////////////////////////////////////////////////////////////////
    //
    // Virtual routes
    //
    ////////////////////////////////////////////////////////////////////////
    //
    // Context Awareness
    //
    {
        "ContextAwareness",
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
    // Always Listening
    //
    {
        "AlwaysListening",
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
};

const uint32_t CAudioPlatformHardware::_uiNbPortGroups = sizeof(CAudioPlatformHardware::_acPortGroups) /
        sizeof(CAudioPlatformHardware::_acPortGroups[0]);

const uint32_t CAudioPlatformHardware::_uiNbPorts = sizeof(CAudioPlatformHardware::_acPorts) /
        sizeof(CAudioPlatformHardware::_acPorts[0]);

const uint32_t CAudioPlatformHardware::_uiNbRoutes = sizeof(CAudioPlatformHardware::_astAudioRoutes) /
        sizeof(CAudioPlatformHardware::_astAudioRoutes[0]);

class CAudioExternalRouteHwCodec0IA : public CAudioExternalRoute
{
public:
    CAudioExternalRouteHwCodec0IA(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioExternalRoute(uiRouteIndex, pPlatformState) {
    }

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t __UNUSED uiFlags = 0) const {
        if (!bIsOut && _pPlatformState->isContextAwarenessEnabled()) {
            //this route is always applicable in capture/context awareness mode
            return true;
        }

        if (!bIsOut
            && _pPlatformState->isAlwaysListeningEnabled()
            && ((_pPlatformState->getDevices(CUtils::EOutput)
                 & AudioSystem::DEVICE_OUT_WIRED_HEADSET) == 0)) {
            // This route is always applicable in "Always Listening" mode
            // unless the headset is available (in which case we should use it
            // and elect the HwCodec1IA route)
            return true;
        }

        if (iMode == AudioSystem::MODE_IN_CALL) {

            int iTtyDirection = _pPlatformState->getTtyDirection();

            if (iTtyDirection == TTY_UPLINK) {

                // In call, TTY HCO mode, Earpiece is used as Output Device
                // Force OpenedPlaybackRoutes to HwCodec0IA|ModemIA
                return bIsOut;

            } else if (iTtyDirection == TTY_DOWNLINK) {

                // In call, TTY VCO mode, DMIC is used as Input Device
                // Force OpenedCaptureRoutes to HwCodec0IA|ModemIA
                return !bIsOut;

            } else if (!bIsOut) {

                // In call, the output is applicable if the output stream is used
                return willBeUsed(CUtils::EOutput);
            }
        }
        return CAudioExternalRoute::isApplicable(uidevices, iMode, bIsOut);
    }

    virtual bool needReconfiguration(bool isOut) const
    {
        return (CAudioRoute::needReconfiguration(isOut) &&
                _pPlatformState->hasPlatformStateChanged(
                    CAudioPlatformState::EHacModeChange |
                    CAudioPlatformState::ETtyDirectionChange |
                    CAudioPlatformState::EBandTypeChange |
                    CAudioPlatformState::HwModeChange)) ||
                CAudioExternalRoute::needReconfiguration(isOut);
    }
};

class CAudioExternalRouteHwCodec1IA : public CAudioExternalRoute
{
public:
    CAudioExternalRouteHwCodec1IA(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioExternalRoute(uiRouteIndex, pPlatformState) {
    }

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t __UNUSED uiFlags = 0) const {

        if (!bIsOut
            && _pPlatformState->isAlwaysListeningEnabled()
            && ((_pPlatformState->getDevices(CUtils::EOutput)
                 & AudioSystem::DEVICE_OUT_WIRED_HEADSET) != 0)) {
            // This route is always applicable in "Always Listening" mode if
            // the wired headset is available
            return true;
        }

        if (iMode == AudioSystem::MODE_IN_CALL) {

            int iTtyDirection = _pPlatformState->getTtyDirection();

            if (iTtyDirection == TTY_UPLINK) {

                // In call, TTY HCO mode, Earpiece is used as Output Device
                // Force OpenedPlaybackRoutes to HwCodec0IA|ModemIA
                return !bIsOut;

           } else if (iTtyDirection == TTY_DOWNLINK) {

                // In call, TTY VCO mode, DMIC is used as Input Device
                // Force OpenedCaptureRoutes to HwCodec0IA|ModemIA
                return bIsOut;

            } else if (!bIsOut) {

                // In call, the output is applicable if the output stream is used
                return willBeUsed(CUtils::EOutput);
            }
        }
        return CAudioExternalRoute::isApplicable(uidevices, iMode, bIsOut);
    }

    virtual bool needReconfiguration(bool isOut) const
    {
        return (CAudioRoute::needReconfiguration(isOut) &&
                _pPlatformState->hasPlatformStateChanged(
                    CAudioPlatformState::EHacModeChange |
                    CAudioPlatformState::ETtyDirectionChange |
                    CAudioPlatformState::EBandTypeChange |
                    CAudioPlatformState::HwModeChange)) ||
                CAudioExternalRoute::needReconfiguration(isOut);
    }
};

class CAudioExternalRouteModemIA : public CAudioExternalRoute
{
public:
    CAudioExternalRouteModemIA(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioExternalRoute(uiRouteIndex, pPlatformState) {
    }

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t __UNUSED uiFlags = 0) const {

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

    virtual bool needReconfiguration(bool __UNUSED bIsOut) const
    {
        return false;
    }
};

class CAudioExternalRouteBtIA : public CAudioExternalRoute
{
public:
    CAudioExternalRouteBtIA(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioExternalRoute(uiRouteIndex, pPlatformState) {
    }

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t __UNUSED uiFlags = 0) const {

        // BT module must be off and as the BT is on the shared I2S bus
        // the modem must be alive as well to use this route
        if (!_pPlatformState->isBtEnabled()) {

            return false;
        }

        if (!bIsOut && (iMode == AudioSystem::MODE_IN_CALL)) {

            // In call, the output is applicable if the output stream is used
            return willBeUsed(CUtils::EOutput);
        }
        return CAudioExternalRoute::isApplicable(uidevices, iMode, bIsOut);
    }

    virtual bool needReconfiguration(bool __UNUSED bIsOut) const
    {
        return false;
    }
};

class CAudioExternalRouteFMIA : public CAudioExternalRoute
{
public:
    CAudioExternalRouteFMIA(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioExternalRoute(uiRouteIndex, pPlatformState) {
    }
};

class CAudioVirtualRouteContextAwareness : public CAudioExternalRoute
{
public:
    CAudioVirtualRouteContextAwareness(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioExternalRoute(uiRouteIndex, pPlatformState) {
    }

    virtual bool isApplicable(uint32_t __UNUSED uidevices, int __UNUSED iMode, bool bIsOut,
                              uint32_t __UNUSED uiFlags = 0) const {
        return !bIsOut && _pPlatformState->isContextAwarenessEnabled();
    }
};

class CAudioVirtualRouteAlwaysListening : public CAudioExternalRoute
{
public:
    CAudioVirtualRouteAlwaysListening(uint32_t routeIndex, CAudioPlatformState *platformState) :
        CAudioExternalRoute(routeIndex, platformState) {
    }

    virtual bool isApplicable(uint32_t devices, int mode, bool isOut,
                              uint32_t __UNUSED flags = 0) const {
        return !isOut
               && _pPlatformState->isAlwaysListeningEnabled()
               && (devices == 0) // Do not activate LPAL if there is any input device...
               && !(mode == AudioSystem::MODE_IN_CALL  // ... or if we are in (CSV or VoIP) call.
                    || mode == AudioSystem::MODE_IN_COMMUNICATION);
    }
};

class CAudioLPECentricStreamRoute : public CAudioStreamRoute
{
public:
    CAudioLPECentricStreamRoute(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioStreamRoute(uiRouteIndex, pPlatformState) {
    }

    virtual bool isPreEnableRequired() { return true; }

    virtual bool isPostDisableRequired() { return true; }

    virtual bool isApplicable(uint32_t uidevices, int iMode,
                              bool bIsOut, uint32_t uiFlags) const {
        // Prevent using media or voice route if BT device is selected
        // and bt chip is disabled. Silence will be generated to avoid
        // audioflinger timeout of blocking read/write (due to LPE off).
        // Note that this change is notably applicable to LPE centric archs
        if (uidevices & DEVICE_BLUETOOTH_SCO_ALL(bIsOut) &&
            (!_pPlatformState->isBtEnabled())) {
            return false;
        }
        return CAudioStreamRoute::isApplicable(uidevices, iMode, bIsOut, uiFlags);
    }
};

class CAudioMediaLPECentricStreamRoute : public CAudioLPECentricStreamRoute
{
public:
    CAudioMediaLPECentricStreamRoute(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioLPECentricStreamRoute(uiRouteIndex, pPlatformState) {
    }

    virtual bool isApplicable(uint32_t uidevices, int iMode,
                              bool bIsOut, uint32_t uiFlags) const {

        // Prevent from recording Voice Call if telephony mode is not In Call
        if ((!bIsOut) &&
                (uidevices & AudioSystem::DEVICE_IN_VOICE_CALL) &&
                (iMode != AudioSystem::MODE_IN_CALL)) {

            return false;
        }

        return CAudioLPECentricStreamRoute::isApplicable(uidevices, iMode, bIsOut, uiFlags);
    }
};

class CAudioVoiceLPECentricStreamRoute : public CAudioLPECentricStreamRoute
{
public:
    CAudioVoiceLPECentricStreamRoute(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioLPECentricStreamRoute(uiRouteIndex, pPlatformState) {

        _pEffectSupported.push_back(FX_IID_AGC);
        _pEffectSupported.push_back(FX_IID_AEC);
        _pEffectSupported.push_back(FX_IID_NS);
    }
};

//
// Once all deriavated class exception has been removed
// replace this function by a generic route creator according to the route type
//
CAudioRoute* CAudioPlatformHardware::createAudioRoute(uint32_t uiRouteIndex, CAudioPlatformState* pPlatformState)
{
    assert(pPlatformState);

    const string strName = getRouteName(uiRouteIndex);

    if (strName == "HwCodec0IA") {

        return new CAudioExternalRouteHwCodec0IA(uiRouteIndex, pPlatformState);

    } else if (strName == "HwCodec1IA") {

        return new CAudioExternalRouteHwCodec1IA(uiRouteIndex, pPlatformState);

    } else if (strName == "ModemIA") {

        return new CAudioExternalRouteModemIA(uiRouteIndex, pPlatformState);

    } else if (strName == "BtIA") {

        return new CAudioExternalRouteBtIA(uiRouteIndex, pPlatformState);

    } else if (strName == "FMIA") {

        return new CAudioExternalRouteFMIA(uiRouteIndex, pPlatformState);

    } else if (strName == "Media") {

        return new CAudioMediaLPECentricStreamRoute(uiRouteIndex, pPlatformState);

    } else if (strName == "Voice") {

        return new CAudioVoiceLPECentricStreamRoute(uiRouteIndex, pPlatformState);

    } else if (strName == "CompressedMedia") {

        return new CAudioCompressedStreamRoute(uiRouteIndex, pPlatformState);

    } else if (strName == "ContextAwareness") {

        return new CAudioVirtualRouteContextAwareness(uiRouteIndex, pPlatformState);

    } else if (strName == "AlwaysListening") {

        return new CAudioVirtualRouteAlwaysListening(uiRouteIndex, pPlatformState);

    }
    ALOGE("%s: wrong route index=%d", __FUNCTION__, uiRouteIndex);
    return NULL;
}

}        // namespace android

