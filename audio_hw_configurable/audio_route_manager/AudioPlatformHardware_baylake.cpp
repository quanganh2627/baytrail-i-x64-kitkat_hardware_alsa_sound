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

#define DEEP_PLAYBACK_PERIOD_TIME_MS    ((int)96)
#define PLAYBACK_PERIOD_TIME_MS         ((int)24)
#define VOICE_PERIOD_TIME_MS            ((int)20)

#define LONG_PERIOD_FACTOR              ((int)2)
#define SEC_PER_MSEC                    ((int)1000)

/// May add a new route, include header here...
#define NB_RING_BUFFER_NORMAL           ((int)2)
#define NB_RING_BUFFER_INCALL           ((int)4)

#define SAMPLE_RATE_8000                ((int)8000)
#define SAMPLE_RATE_48000               ((int)48000)

#define DEEP_PLAYBACK_48000_PERIOD_SIZE ((int)DEEP_PLAYBACK_PERIOD_TIME_MS * LONG_PERIOD_FACTOR * SAMPLE_RATE_48000 / SEC_PER_MSEC) //(96 * 2 * 48000 / USEC_PER_SEC)
#define PLAYBACK_48000_PERIOD_SIZE      ((int)PLAYBACK_PERIOD_TIME_MS * LONG_PERIOD_FACTOR * SAMPLE_RATE_48000 / SEC_PER_MSEC) //(24 *2 * 48000 / USEC_PER_SEC)
#define CAPTURE_48000_PERIOD_SIZE       ((int)VOICE_PERIOD_TIME_MS * LONG_PERIOD_FACTOR * SAMPLE_RATE_48000 / SEC_PER_MSEC) //(20 *2 * 48000 / USEC_PER_SEC)
#define VOICE_48000_PERIOD_SIZE         ((int)VOICE_PERIOD_TIME_MS * SAMPLE_RATE_48000 / SEC_PER_MSEC)  //(20 * 48000 / USEC_PER_SEC)



static const char* MEDIA_CARD_NAME = "baytrailaudio";
#define DEEP_MEDIA_PLAYBACK_DEVICE_ID   ((int)0)
#define MEDIA_PLAYBACK_DEVICE_ID        ((int)0)
#define MEDIA_CAPTURE_DEVICE_ID         ((int)0)

using namespace std;

namespace android_audio_legacy
{
// For playback, configure ALSA to start the transfer when the
// first period is full.
// For recording, configure ALSA to start the transfer on the
// first frame.
static const pcm_config pcm_config_deep_media_playback = {
   channels          : 2,
   rate              : SAMPLE_RATE_48000,
   period_size       : DEEP_PLAYBACK_48000_PERIOD_SIZE,
   period_count      : NB_RING_BUFFER_NORMAL,
   format            : PCM_FORMAT_S16_LE,
   start_threshold   : DEEP_PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL - 1,
   stop_threshold    : DEEP_PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
   silence_threshold : 0,
   avail_min         : DEEP_PLAYBACK_48000_PERIOD_SIZE,
};

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

const char* const CAudioPlatformHardware::_acPorts[] = {
    "LPE_I2S2_PORT",
    "LPE_I2S3_PORT",
    "IA_I2S0_PORT",
    "IA_I2S1_PORT",
    "HWCODEC_ASP_PORT",
    "HWCODEC_VSP_PORT",
    "HWCODEC_AUX_PORT",
};


// Port Group and associated port
// Group0: "HwCodecMediaPort", "HwCodecVoicePort"
// Group1: "IACommPort", "ModemCsvPort"
const char* const CAudioPlatformHardware::_acPortGroups[] = {
    "HWCODEC_ASP_PORT,HWCODEC_VSP_PORT"
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
    {
        "Media",
        CAudioRoute::EStreamRoute,
        "",
        {
            DEVICE_IN_BUILTIN_ALL,
            DEVICE_OUT_MM_ALL
        },
        {
            (1 << AUDIO_SOURCE_DEFAULT) | (1 << AUDIO_SOURCE_MIC) | (1 << AUDIO_SOURCE_CAMCORDER) |
            (1 << AUDIO_SOURCE_VOICE_RECOGNITION) | (1 << AUDIO_SOURCE_VOICE_COMMUNICATION),
            AUDIO_OUTPUT_FLAG_PRIMARY
        },
        {
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE) |
            (1 << AudioSystem::MODE_IN_COMMUNICATION),
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE) |
            (1 << AudioSystem::MODE_IN_COMMUNICATION)
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
    {
        "DeepMedia",
        CAudioRoute::EStreamRoute,
        "",
        {
            NOT_APPLICABLE,
            DEVICE_OUT_MM_ALL
        },
        {
            NOT_APPLICABLE,
            AUDIO_OUTPUT_FLAG_DEEP_BUFFER
        },
        {
            NOT_APPLICABLE,
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE)
        },
        {
            NOT_APPLICABLE,
            NOT_APPLICABLE
        },
        MEDIA_CARD_NAME,
        {
            NOT_APPLICABLE,
            DEEP_MEDIA_PLAYBACK_DEVICE_ID
        },
        {
            pcm_config_not_applicable,
            pcm_config_deep_media_playback
        },
        {
            { CSampleSpec::ECopy, CSampleSpec::ECopy },
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
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE) |
            (1 << AudioSystem::MODE_IN_COMMUNICATION),
            (1 << AudioSystem::MODE_NORMAL) | (1 << AudioSystem::MODE_RINGTONE) |
            (1 << AudioSystem::MODE_IN_COMMUNICATION)
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
        "CompressedMedia,Media,DeepMedia"
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

    } else if (strName == "DeepMedia") {

        return new CAudioStreamRoute(uiRouteIndex, pPlatformState);

    } else if (strName == "CompressedMedia") {

        return new CAudioCompressedStreamRoute(uiRouteIndex, pPlatformState);

    } else if (strName == "HwCodecMedia") {

        return new CAudioExternalRouteHwCodecMedia(uiRouteIndex, pPlatformState);
    }
    ALOGE("%s: wrong route index=%d", __FUNCTION__, uiRouteIndex);
    return NULL;
}

}        // namespace android

