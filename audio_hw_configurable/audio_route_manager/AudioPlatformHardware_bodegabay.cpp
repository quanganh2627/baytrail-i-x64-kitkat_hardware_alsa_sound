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

#include <tinyalsa/asoundlib.h>


/// May add a new route, include header here...
#define SAMPLE_RATE_8000                (8000)
#define SAMPLE_RATE_48000               (48000)
#define VOICE_CAPTURE_PERIOD_SIZE       (320) // 20ms @ 16k, mono
#define PLAYBACK_44100_PERIOD_SIZE      (1024) //(PLAYBACK_44100_PERIOD_TIME_US * 44100 / USEC_PER_SEC)
#define PLAYBACK_48000_PERIOD_SIZE      (6000) //(24000*2 * 48000 / USEC_PER_SEC)
#define CAPTURE_48000_PERIOD_SIZE       (1152) //(CAPTURE_48000_PERIOD_SIZE * 48000 / USEC_PER_SEC)
#define NB_RING_BUFFER_NORMAL           (4)
#define NB_RING_BUFFER_INCALL           (4)


#define MEDIA_CARD_NAME                 ("lm49453audio")
#define MEDIA_PLAYBACK_DEVICE_ID        (0)
#define MEDIA_CAPTURE_DEVICE_ID         (0)


#define VOICE_CARD_NAME                 ("lm49453audio")
#define VOICE_DOWNLINK_DEVICE_ID        (0)
#define VOICE_UPLINK_DEVICE_ID          (0)

namespace android_audio_legacy
{
// For playback, configure ALSA to start the transfer when the
// first period is full.
// For recording, configure ALSA to start the transfer on the
// first frame.
static const pcm_config pcm_config_media_playback = {
    channels        : 2,
    rate            : SAMPLE_RATE_48000,
    period_size     : PLAYBACK_48000_PERIOD_SIZE,
    period_count    : NB_RING_BUFFER_NORMAL,
    format          : PCM_FORMAT_S16_LE,
    start_threshold : PLAYBACK_48000_PERIOD_SIZE - 1,
    stop_threshold  : PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
    silence_threshold : 0,
    avail_min       : PLAYBACK_48000_PERIOD_SIZE,
};

static const pcm_config pcm_config_media_capture = {
    channels        : 2,
    rate            : SAMPLE_RATE_48000,
    period_size     : PLAYBACK_48000_PERIOD_SIZE,
    period_count    : NB_RING_BUFFER_NORMAL,
    format          : PCM_FORMAT_S16_LE,
    start_threshold : 1,
    stop_threshold  : PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
    silence_threshold : 0,
    avail_min       : PLAYBACK_48000_PERIOD_SIZE,
};

static const pcm_config pcm_config_voice_downlink = {
    channels        : 2,
    rate            : SAMPLE_RATE_48000,
    period_size     : VOICE_CAPTURE_PERIOD_SIZE,
    period_count    : NB_RING_BUFFER_INCALL,
    format          : PCM_FORMAT_S16_LE,
    start_threshold : PLAYBACK_48000_PERIOD_SIZE,
    stop_threshold  : PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
    silence_threshold : 0,
    avail_min       : PLAYBACK_48000_PERIOD_SIZE,
};

static const pcm_config pcm_config_voice_uplink = {
    channels        : 2,
    rate            : SAMPLE_RATE_48000,
    period_size     : VOICE_CAPTURE_PERIOD_SIZE,
    period_count    : NB_RING_BUFFER_INCALL,
    format          : PCM_FORMAT_S16_LE,
    start_threshold : 1,
    stop_threshold  : PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
    silence_threshold : 0,
    avail_min       : PLAYBACK_48000_PERIOD_SIZE,
};

const char* const CAudioPlatformHardware::_acPorts[] = {
};

// Port Group and associated port
const char* const CAudioPlatformHardware::_acPortGroups[] = {
};

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
        "MEDIA",
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
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE) | (1 << AudioSystem::MODE_IN_CALL),
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE) | (1 << AudioSystem::MODE_IN_CALL)
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
    //
    // Voice Route
    //
//    {
//        "VOICE",
//        CAudioRoute::EStreamRoute,
//          "",
//        {
//            DEVICE_IN_BUILTIN_ALL | DEVICE_IN_BLUETOOTH_SCO_ALL,
//            DEVICE_OUT_MM_ALL | DEVICE_OUT_BLUETOOTH_SCO_ALL
//        },
//        {
//            (1 << AUDIO_SOURCE_VOICE_UPLINK) | (1 << AUDIO_SOURCE_VOICE_DOWNLINK) | (1 << AUDIO_SOURCE_VOICE_CALL),
//            AUDIO_OUTPUT_FLAG_PRIMARY,
//        },
//        {
//            (1 << AudioSystem::MODE_IN_CALL),
//            (1 << AudioSystem::MODE_IN_CALL)
//        },
//        VOICE_CARD_NAME,
//        {
//            VOICE_UPLINK_DEVICE_ID,
//            VOICE_DOWNLINK_DEVICE_ID,
//        },
//        {
//            pcm_config_voice_downlink,
//            pcm_config_voice_uplink,
//        },
//        ""
//    },
    ////////////////////////////////////////////////////////////////////////
    //
    // External routes
    //
    ////////////////////////////////////////////////////////////////////////
    //
    // HwCodec 0 route
    //
    {
        "HWCODEC_OIA",
        CAudioRoute::EExternalRoute,
        {
            DEVICE_IN_BUILTIN_ALL,
            DEVICE_OUT_MM_ALL
        },
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
        },
        {
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE) | (1 << AudioSystem::MODE_IN_CALL),
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE) | (1 << AudioSystem::MODE_IN_CALL)
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
        "MODEM_IA,MEDIA"
    },
    //
    // HWCODEC 1 route
    //
    {
        "HWCODEC_1IA",
        CAudioRoute::EExternalRoute,
        "",
        {
            NOT_APPLICABLE,     // Why? because there are no input stream for the BT CSV UL!!!
            DEVICE_OUT_BLUETOOTH_SCO_ALL
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
    },
    //
    // ModemIA route
    //
    {
        "MODEM_IA",
        CAudioRoute::EExternalRoute,
        "",
        {
            NOT_APPLICABLE,
            ALL
        },
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
        },
        {
            (1 << AudioSystem::MODE_IN_CALL),
            (1 << AudioSystem::MODE_IN_CALL)
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
    //
    // BT route
    //
    {
        "BT_IA",
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
        "MODEM_IA,MEDIA"
    },
    //
    // FM route
    //
    {
        "FM_IA",
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


class CAudioExternalRouteHwCodec0IA : public CAudioExternalRoute
{
public:
    CAudioExternalRouteHwCodec0IA(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioExternalRoute(uiRouteIndex, pPlatformState) {
    }

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t __UNUSED uiFlags = 0) const {

        if (!bIsOut && (iMode == AudioSystem::MODE_IN_CALL)) {

            // In call, the output is applicable if the output stream is used
            return  willBeUsed(CUtils::EOutput);
        }
        return CAudioExternalRoute::isApplicable(uidevices, iMode, bIsOut);
    }
};

class CAudioExternalRouteHwCodec1IA : public CAudioExternalRoute
{
public:
    CAudioExternalRouteHwCodec1IA(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioExternalRoute(uiRouteIndex, pPlatformState) {
    }

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t __UNUSED uiFlags = 0) const {

        // This route is applicable if:
        //  -either At least one stream must be active to enable this route
        //  -or a voice call on Codec is on going
        if (!_pPlatformState->isModemAudioAvailable()) {

            return false;
        }
        return CAudioExternalRoute::isApplicable(uidevices, iMode, bIsOut);
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
        return CAudioExternalRoute::isApplicable(uidevices, iMode, bIsOut);
    }
};

class CAudioExternalRouteFMIA : public CAudioExternalRoute
{
public:
    CAudioExternalRouteFMIA(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState) :
        CAudioExternalRoute(uiRouteIndex, pPlatformState) {
    }

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t __UNUSED uiFlags = 0) const {

        if (_pPlatformState->getFmRxMode() != AudioSystem::MODE_FM_ON) {

            return false;
        }
        return CAudioExternalRoute::isApplicable(uidevices, iMode, bIsOut);
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

    if (strName == "HWCODEC_OIA") {

        return new CAudioExternalRouteHwCodec0IA(uiRouteIndex, pPlatformState);

    } else if (strName == "HWCODEC_1IA") {

        return new CAudioExternalRouteHwCodec1IA(uiRouteIndex, pPlatformState);

    } else if (strName == "MODEM_IA") {

        return new CAudioExternalRouteModemIA(uiRouteIndex, pPlatformState);

    } else if (strName == "BT_IA") {

        return new CAudioExternalRouteBtIA(uiRouteIndex, pPlatformState);

    } else if (strName == "FM_IA") {

        return new CAudioExternalRouteFMIA(uiRouteIndex, pPlatformState);

    } else if (strName == "MEDIA") {

        return new CAudioStreamRoute(uiRouteIndex, pPlatformState);

    } else if (strName == "VOICE") {

        return new CAudioStreamRoute(uiRouteIndex, pPlatformState);

    }
    ALOGE("%s: wrong route index=%d", __FUNCTION__, uiRouteIndex);
    return NULL;
}
}        // namespace android

