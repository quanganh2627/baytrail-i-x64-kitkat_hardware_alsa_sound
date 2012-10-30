/* AudioRouteFactory.cpp
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

#define LOG_TAG "AudioRouteFactory"
#include <utils/Log.h>

#include "AudioRouteFactory.h"

#include "AudioPlatformHardware.h"
#include "AudioRoute.h"
#include "AudioStreamRouteMedia.h"
#include "AudioStreamRouteVoice.h"
#include "AudioExternalRouteHwCodec0IA.h"
#include "AudioExternalRouteHwCodec1IA.h"
#include "AudioExternalRouteBtIA.h"
#include "AudioExternalRouteFMIA.h"
#include "AudioExternalRouteModemIA.h"

/// May add a new route, include header here...

#define SAMPLE_RATE_48000               (48000)
#define VOICE_CAPTURE_PERIOD_SIZE     (320) // 20ms @ 16k, mono
#define PLAYBACK_44100_PERIOD_SIZE   1024 //(PLAYBACK_44100_PERIOD_TIME_US * 44100 / USEC_PER_SEC)
#define PLAYBACK_48000_PERIOD_SIZE   1152 //(24000*2 * 48000 / USEC_PER_SEC)
#define CAPTURE_48000_PERIOD_SIZE    1152 //(CAPTURE_48000_PERIOD_SIZE * 48000 / USEC_PER_SEC)
#define NB_RING_BUFFER_NORMAL   2
#define NB_RING_BUFFER_INCALL   4

#define MEDIA_CARD_ID 2
#define MEDIA_PLAYBACK_DEVICE_ID    0
#define MEDIA_CAPTURE_DEVICE_ID    0



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
    period_size     : PLAYBACK_48000_PERIOD_SIZE / 2,
    period_count    : 4,
    format          : PCM_FORMAT_S16_LE,
    start_threshold : 0,
    stop_threshold  : 0,
    silence_threshold : 0,
    avail_min       : 0,
};

#define VOICE_CARD_ID 2
#define VOICE_PLAYBACK_DEVICE_ID    0
#define VOICE_CAPTURE_DEVICE_ID    0

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

static const pcm_config pcm_config_playback_low_latency = {
    channels        : 2,
    rate            : SAMPLE_RATE_48000,
    period_size     : PLAYBACK_48000_PERIOD_SIZE,
    period_count    : NB_RING_BUFFER_NORMAL,
    format          : PCM_FORMAT_S16_LE,
    start_threshold : PLAYBACK_48000_PERIOD_SIZE,
    stop_threshold  : PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
    silence_threshold : 0,
    avail_min       : PLAYBACK_48000_PERIOD_SIZE,
};

namespace android_audio_legacy
{

CAudioRoute* CAudioRouteFactory::getRoute(uint32_t uiRouteId, CAudioPlatformState* pPlatformState)
{
    switch(uiRouteId) {

    //
    // STREAMS ROUTES
    //
    case 1 << MEDIA:
        return new CAudioStreamRouteMedia(uiRouteId,
                                         MEDIA_PLAYBACK_DEVICE_ID,
                                         MEDIA_CAPTURE_DEVICE_ID,
                                         MEDIA_CARD_ID,
                                         pcm_config_media_playback,
                                         pcm_config_media_capture,
                                         pPlatformState);

    case 1 << VOICE:
        return new CAudioStreamRouteMedia(uiRouteId,
                                         VOICE_PLAYBACK_DEVICE_ID,
                                         VOICE_CAPTURE_DEVICE_ID,
                                         VOICE_CARD_ID,
                                         pcm_config_voice_downlink,
                                         pcm_config_voice_uplink,
                                         pPlatformState);

    //
    // EXTERNAL ROUTES
    //
    case 1 << HWCODEC_OIA:

        return new CAudioExternalRouteHwCodec0IA(uiRouteId, pPlatformState);

    case 1 << HWCODEC_1IA:

        return new CAudioExternalRouteHwCodec1IA(uiRouteId, pPlatformState);

    case 1 << BT_IA:

        return new CAudioExternalRouteBtIA(uiRouteId, pPlatformState);

    case 1 << FM_IA:

        return new CAudioExternalRouteFMIA(uiRouteId, pPlatformState);

    case 1 << MODEM_IA:

        return new CAudioExternalRouteModemIA(uiRouteId, pPlatformState);

    default:
        break;

    }
    /// May add a new route, include case here...

    return NULL;
}

}       // namespace android
