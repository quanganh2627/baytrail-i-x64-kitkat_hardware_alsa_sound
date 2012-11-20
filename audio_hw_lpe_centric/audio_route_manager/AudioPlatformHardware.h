/* AudioPlatformHardware.h
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

#pragma once

#include <tinyalsa/asoundlib.h>
#include <hardware_legacy/AudioHardwareBase.h>
#include "AudioPlatformState.h"
#include "AudioStreamRoute.h"
#include "AudioExternalRoute.h"
#include "AudioRoute.h"
#include "AudioRouteManager.h"

using namespace std;

namespace android_audio_legacy
{

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

#define ALL             (-1)
#define NOT_APPLICABLE  (0)

static const pcm_config pcm_config_not_applicable = {
   /* channels        : */0,
   /* rate            : */0,
   /* period_size     : */0,
   /* period_count    : */0,
   /* format          : */PCM_FORMAT_S16_LE,
   /* start_threshold : */0 ,
   /* stop_threshold  : */0 ,
   /* silence_threshold : */0,
   /* avail_min       : */0,
};

class CAudioPlatformHardware {

public:
    static const CAudioRouteManager::SSelectionCriterionTypeValuePair _stRouteValuePairs[];
    static const uint32_t _uiNbRouteValuePairs;

    static CAudioRoute *createAudioRoute(uint32_t uiRouteId, CAudioPlatformState* pPlatformState);

    static uint32_t getNbRoutes() { return _uiNbRoutes; }

    //
    // Route helpers
    //
    static const char* getRouteName(int iRouteIndex) { return _astrAudioRoutes[iRouteIndex].routeName; }
    static uint32_t getRouteId(int iRouteIndex) { return _astrAudioRoutes[iRouteIndex].uiRouteId; }
    static CAudioRoute::RouteType getRouteType(int iRouteIndex) { return _astrAudioRoutes[iRouteIndex].uiRouteType; }
    static uint32_t getRouteApplicableDevices(int iRouteIndex, bool bIsOut) { return _astrAudioRoutes[iRouteIndex].auiApplicableDevices[bIsOut]; }
    static uint32_t getRouteApplicableFlags(int iRouteIndex, bool bIsOut) { return _astrAudioRoutes[iRouteIndex].uiApplicableFlags[bIsOut]; }
    static uint32_t getRouteApplicableModes(int iRouteIndex, bool bIsOut) { return _astrAudioRoutes[iRouteIndex].uiApplicableModes[bIsOut]; }
    static int32_t getRouteCardId(int iRouteIndex) { return _astrAudioRoutes[iRouteIndex].iCardId; }
    static int32_t getRouteDeviceId(int iRouteIndex, bool bIsOut) { return _astrAudioRoutes[iRouteIndex].aiDeviceId[bIsOut]; }
    static const pcm_config& getRoutePcmConfig(int iRouteIndex, bool bIsOut) { return _astrAudioRoutes[iRouteIndex].astrPcmConfig[bIsOut]; }
    static uint32_t getSlaveRoutes(int iRouteIndex) { return _astrAudioRoutes[iRouteIndex].uiSlaveRoutes; }

private:

    struct s_route_t {
        const char* routeName;
        uint32_t uiRouteId;
        CAudioRoute::RouteType uiRouteType;
        uint32_t auiApplicableDevices[2];           // bit field
        uint32_t uiApplicableFlags[2];              // bit field (For Input: InputSource, for output: OutputFlags
        uint32_t uiApplicableModes[2];              // bit field
        int32_t iCardId;
        int32_t aiDeviceId[2];
        pcm_config astrPcmConfig[2];
        uint32_t uiSlaveRoutes;                     // bit field (slave routes used by this route)
    };

    static const uint32_t _uiNbRoutes;

    static const s_route_t _astrAudioRoutes[];//_uiNbRoutes];
};
};        // namespace android

