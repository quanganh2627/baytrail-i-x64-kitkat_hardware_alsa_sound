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



#define DEVICE_OUT_MM_ALL (AudioSystem::DEVICE_OUT_EARPIECE | \
                AudioSystem::DEVICE_OUT_SPEAKER | \
                AudioSystem::DEVICE_OUT_WIRED_HEADSET | \
                AudioSystem::DEVICE_OUT_WIRED_HEADPHONE)


#define DEVICE_IN_BUILTIN_ALL (AudioSystem::DEVICE_IN_WIRED_HEADSET | \
                AudioSystem::DEVICE_IN_BACK_MIC | \
                AudioSystem::DEVICE_IN_AUX_DIGITAL | \
                AudioSystem::DEVICE_IN_BUILTIN_MIC)

#define DEVICE_OUT_BLUETOOTH_SCO_ALL (AudioSystem::DEVICE_OUT_BLUETOOTH_SCO | \
                AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET | \
                AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT)

#define DEVICE_IN_BLUETOOTH_SCO_ALL (AudioSystem::DEVICE_IN_BLUETOOTH_SCO_HEADSET)

/// May add a new route, include header here...
#define SAMPLE_RATE_8000               (8000)
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


#define VOICE_CARD_ID 2
#define VOICE_MIXING_DEVICE_ID    0
#define VOICE_RECORD_DEVICE_ID    0
#define VOICE_DOWNLINK_DEVICE_ID    0
#define VOICE_UPLINK_DEVICE_ID    0


#define BT_CARD_ID 2
#define BT_PLAYBACK_DEVICE_ID    0
#define BT_CAPTURE_DEVICE_ID    0


#define MODEM_MIX_CARD_ID 2
#define MODEM_MIX_PLAYBACK_DEVICE_ID    0
#define MODEM_MIX_CAPTURE_DEVICE_ID    0

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
   /* period_size     : */PLAYBACK_48000_PERIOD_SIZE / 2,
   /* period_count    : */4,
   /* format          : */PCM_FORMAT_S16_LE,
   /* start_threshold : */0,
   /* stop_threshold  : */0,
   /* silence_threshold : */0,
   /* avail_min       : */0,
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

static const pcm_config pcm_config_bt_playback = {
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

static const pcm_config pcm_config_bt_capture = {
   /* channels        : */1,
   /* rate            : */SAMPLE_RATE_8000,
   /* period_size     : */VOICE_CAPTURE_PERIOD_SIZE,
   /* period_count    : */NB_RING_BUFFER_INCALL,
   /* format          : */PCM_FORMAT_S16_LE,
   /* start_threshold : */1,
   /* stop_threshold  : */PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
   /* silence_threshold : */0,
   /* avail_min       : */PLAYBACK_48000_PERIOD_SIZE,
};

static const pcm_config pcm_config_modem_mix_playback = {
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

static const pcm_config pcm_config_modem_mix_capture = {
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
    IA_I2S0,
    IA_I2S1_PORT,
    MODEM_I2S1_PORT,
    MODEM_I2S2_PORT,
    BT_I2S_PORT,
    FM_I2S_PORT,
    HWCODEC_I2S1_PORT,
    HWCODEC_I2S2_PORT,
    HWCODEC_I2S3_PORT,
    HWCODEC_ASP_PORT,
    HWCODEC_VSP_PORT,
    HWCODEC_AUX_PORT,
    AUDIENCE_MIXING_PORTD,

    NB_AUDIO_PORT
} AudioPortId;

const char* CAudioPlatformHardware::_apcAudioPorts[CAudioPlatformHardware::_uiNbPorts] = {
    "LPE_I2S2_PORT",
    "LPE_I2S3_PORT",
    "IA_I2S0",
    "IA_I2S1",
    "MOODEM_I2S1_PORT",
    "MODEM_I2S2_PORT",
    "BT_I2S_PORT",
    "FM_I2S_PORT"
    "HWCODEC_I2S1_PORT",
    "HWCODEC_I2S2_PORT",
    "HWCODEC_I2S3_PORT"
};

// Port Group and associated port
// Group0: "HwCodecMediaPort", "HwCodecVoicePort"
// Group1: "BTPort", "HwCodecVoicePort"
// Group2: "IACommPort", "ModemCsvPort"
#define NB_PORT_GROUP   3
#define NB_PORT_PGR0    2
#define NB_PORT_PGR1    3
#define NB_PORT_PGR2    2

const int CAudioPlatformHardware::_gstrNbPortByGroup[CAudioPlatformHardware::_uiNbPortGroups] = {
    NB_PORT_PGR0,
    NB_PORT_PGR1,
    NB_PORT_PGR2
};

const CAudioPlatformHardware::s_port_group_t CAudioPlatformHardware::_astrPortGroups[CAudioPlatformHardware::_uiNbPortGroups] = {
    {"GROUP0", CAudioPlatformHardware::_gstrNbPortByGroup[0], {HWCODEC_MEDIA_PORT, HWCODEC_VOICE_PORT}},
    {"GROUP1", CAudioPlatformHardware::_gstrNbPortByGroup[1], {HWCODEC_VOICE_PORT, BT_PORT, FM_PORT}},
    {"GROUP2", CAudioPlatformHardware::_gstrNbPortByGroup[2], {IA_COMM_PORT, MODEM_CSV_PORT}}
};

typedef enum {
    HWCODEC_CSV,
    HWCODEC_COMM,
    MODEM_MIX,
    BT_CSV,
    BT_COMM,
    HWCODEC_MEDIA,

    NB_ROUTE
} AudioRouteId;

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
        1 << HWCODEC_CSV,
        EStreamRoute,
        {
            LPE_I2S3_PORT,
            HWCODEC_ASP_PORT
        },
        {
            DEVICE_OUT_MM_ALL | DEVICE_OUT_BLUETOOTH_SCO_ALL,
            DEVICE_IN_BUILTIN_ALL | DEVICE_IN_BLUETOOTH_SCO_ALL
        },
        {
            (1 << AUDIO_SOURCE_DEFAULT) | (1 << AUDIO_SOURCE_MIC) | (1 << AUDIO_SOURCE_CAMCORDER) | (1 << AUDIO_SOURCE_VOICE_RECOGNITION)
        },
        {
            (1 << MODE_NORMAL) | (1 << MODE_RINGTONE),
            (1 << MODE_NORMAL) | (1 << MODE_RINGTONE)
        },
        {
            1 << EStreamMixer,
            1 << EStreamDirect
        },
        MEDIA_CARD_ID,
        {
            MEDIA_PLAYBACK_DEVICE_ID,
            MEDIA_CAPTURE_DEVICE_ID
        },
        {
            pcm_config_media_playback,
            pcm_config_media_capture
        }
    },
    //
    // VOICE (VoIP) Route
    //
    {
        1 << HWCODEC_CSV,
        EStreamRoute,
        {
            IA_I2S1_PORT,
            HWCODEC_VSP_PORT
        },
        {
            DEVICE_OUT_MM_ALL | DEVICE_OUT_BLUETOOTH_SCO_ALL,
            DEVICE_IN_BUILTIN_ALL | DEVICE_IN_BLUETOOTH_SCO_ALL
        },
        {
            (1 << AUDIO_SOURCE_VOICE_COMMUNICATION)
        },
        {
            (1 << MODE_IN_COMMUNICATION),
            (1 << MODE_IN_COMMUNICATION)
        },
        {
            1 << EStreamDirect,
            1 << EStreamDirect
        },
        VOICE_CARD_ID,
        {
            VOICE_DOWNLINK_DEVICE_ID,
            VOICE_UPLINK_DEVICE_ID
        },
        {
            pcm_config_voice_downlink,
            pcm_config_voice_uplink
        }
    },
    //
    // Voice Mix route (IE mixing during CSV, VoIP or VoLTE call)
    //
    {
        1 << HWCODEC_CSV,
        EStreamRoute,
        {
            IA_I2S0_PORT,
            AUDIENCE_MIXING_PORT
        },
        {
            DEVICE_OUT_MM_ALL | DEVICE_OUT_BLUETOOTH_SCO_ALL,
            DEVICE_IN_BUILTIN_ALL | DEVICE_IN_BLUETOOTH_SCO_ALL
        },
        {
            (1 << AUDIO_SOURCE_VOICE_UPLINK) | (1 << AUDIO_SOURCE_VOICE_DOWNLINK) | (1 << AUDIO_SOURCE_VOICE_CALL)
        },
        {
            (1 << MODE_IN_CALL) | (1 << MODE_IN_COMMUNICATION),
            (1 << MODE_IN_CALL) | (1 << MODE_IN_COMMUNICATION)
        },
        {
            1 << EStreamMixer,
            1 << EStreamDirect
        },
        VOICE_CARD_ID,
        {
            VOICE_MIXING_DEVICE_ID,
            VOICE_RECORDING_DEVICE_ID
        },
        {
            pcm_config_voice_downlink,
            pcm_config_voice_uplink
        }
    }
    ////////////////////////////////////////////////////////////////////////
    //
    // External routes
    //
    ////////////////////////////////////////////////////////////////////////
    //
    // CSV route
    //


    //
    // BT route
    //

    //
    // FM route
    //
};

}        // namespace android

