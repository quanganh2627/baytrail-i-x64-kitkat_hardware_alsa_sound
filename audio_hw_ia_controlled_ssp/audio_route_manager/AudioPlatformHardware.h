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

#pragma once

#include <tinyalsa/asoundlib.h>
#include <hardware_legacy/AudioHardwareBase.h>
#include "AudioPlatformState.h"
#include "AudioStreamRoute.h"
#include "AudioCompressedStreamRoute.h"
#include "AudioExternalRoute.h"
#include "AudioRoute.h"
#include "AudioRouteManager.h"
#include "Tokenizer.h"

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

#define NOT_APPLICABLE  (0)

//extern "C" static const pcm_config pcm_config_not_applicable;

static const pcm_config pcm_config_not_applicable = {
   channels             : 0,
   rate                 : 0,
   period_size          : 0,
   period_count         : 0,
   format               : PCM_FORMAT_S16_LE,
   start_threshold      : 0 ,
   stop_threshold       : 0 ,
   silence_threshold    : 0,
   avail_min            : 0,
};

class CAudioPlatformHardware {

public:
    static const CAudioRouteManager::SSelectionCriterionTypeValuePair _stRouteValuePairs[];
    static const uint32_t _uiNbRouteValuePairs;

    static CAudioRoute* createAudioRoute(uint32_t uiRouteIndex, CAudioPlatformState* pPlatformState);

    static uint32_t getNbPorts() { return _uiNbPorts; }

    static uint32_t getNbPortGroups() { return _uiNbPortGroups; }

    static uint32_t getNbRoutes() { return _uiNbRoutes; }

    //
    // Port helpers
    //
    static const char* getPortName(int uiPortIndex) {
        return _acPorts[uiPortIndex];
    }

    static uint32_t getPortId(uint32_t uiPortIndex) { return (uint32_t)1 << uiPortIndex; }

    //
    // Port group helpers
    //
    static uint32_t getPortsUsedByPortGroup(uint32_t uiPortGroupIndex) {

        std::string srtPorts(_astAudioRoutes[uiPortGroupIndex].pcPortsUsed);
        uint32_t uiPorts = 0;
        Tokenizer tokenizer(srtPorts, ",");
        std::vector<std::string> astrItems = tokenizer.split();

        uint32_t i;
        for (i = 0; i < astrItems.size(); i++) {

            uiPorts |= getPortIndexByName(astrItems[i]);
        }
        LOGD("%s Ports name=%s Ports=0x%X", __FUNCTION__, srtPorts.c_str(), uiPorts);
        return uiPorts;
    }

    //
    // Route helpers
    //
    static std::string getRouteName(int iRouteIndex) {
        return _astAudioRoutes[iRouteIndex].pcRouteName;
    }
    static uint32_t getRouteId(int iRouteIndex) {
        return (uint32_t)1 << iRouteIndex;
    }
    static uint32_t getRouteType(int iRouteIndex) {
        return _astAudioRoutes[iRouteIndex].uiRouteType;
    }
    static uint32_t getPortsUsedByRoute(int iRouteIndex) {

        std::string srtPorts(_astAudioRoutes[iRouteIndex].pcPortsUsed);
        uint32_t uiPorts = 0;
        Tokenizer tokenizer(srtPorts, ",");
        std::vector<std::string> astrItems = tokenizer.split();

        uint32_t i;
        for (i = 0; i < astrItems.size(); i++) {

            uiPorts |= getPortIndexByName(astrItems[i].c_str());
        }
        LOGD("%s Ports name=%s Ports=0x%X", __FUNCTION__, srtPorts.c_str(), uiPorts);
        return uiPorts;
    }
    static uint32_t getRouteApplicableDevices(int iRouteIndex, bool bIsOut) {
        return _astAudioRoutes[iRouteIndex].auiApplicableDevices[bIsOut];
    }
    static uint32_t getRouteApplicableMask(int iRouteIndex, bool bIsOut) {
        return _astAudioRoutes[iRouteIndex].uiApplicableMask[bIsOut];
    }
    static uint32_t getRouteApplicableModes(int iRouteIndex, bool bIsOut) {
        return _astAudioRoutes[iRouteIndex].uiApplicableModes[bIsOut];
    }
    static const char* getRouteCardName(int iRouteIndex) {
        return _astAudioRoutes[iRouteIndex].pcCardName;
    }
    static int32_t getRouteDeviceId(int iRouteIndex, bool bIsOut) {
        return _astAudioRoutes[iRouteIndex].aiDeviceId[bIsOut];
    }
    static const pcm_config& getRoutePcmConfig(int iRouteIndex, bool bIsOut) {
        return _astAudioRoutes[iRouteIndex].astPcmConfig[bIsOut];
    }
    static uint32_t getSlaveRoutes(int iRouteIndex) {

        std::string srtSlaveRoutes(_astAudioRoutes[iRouteIndex].pcSlaveRoutes);
        uint32_t uiSlaves = 0;
        Tokenizer tokenizer(srtSlaveRoutes, ",");
        std::vector<std::string> astrItems = tokenizer.split();

        uint32_t i;
        for (i = 0; i < astrItems.size(); i++) {

            uiSlaves |= getRouteIndexByName(astrItems[i]);
        }
        LOGD("%s acSlaveRoutes=%s uiSlaves=0x%X", __FUNCTION__, srtSlaveRoutes.c_str(), uiSlaves);
        return uiSlaves;
    }

    static uint32_t getRouteIndexByName(const std::string& strRouteName) {

        uint32_t index;
        for (index = 0; index < getNbRoutes(); index++) {

            std::string strRouteAtIndexName(_astAudioRoutes[index].pcRouteName);
            if (strRouteName == strRouteAtIndexName) {

                return 1 << index;
            }
        }
        return 0;
    }

    static uint32_t getPortIndexByName(const std::string& strPortName) {

        uint32_t index;
        for (index = 0; index < getNbPorts(); index++) {

            std::string strPortAtIndexName(_acPorts[index]);
            if (strPortName == strPortAtIndexName) {

                return 1 << index;
            }
        }
        return 0;
    }

    // Property name indicating time to write silence before first write
    static const char* CODEC_DELAY_PROP_NAME;

private:

    struct s_route_t {

        const string pcRouteName;
        CAudioRoute::RouteType uiRouteType;
        const char* pcPortsUsed;                         // separated coma literal list of ports
        uint32_t auiApplicableDevices[CUtils::ENbDirections];           // bit field
        uint32_t uiApplicableMask[CUtils::ENbDirections];               // bit field (For Input: InputSource, for output: OutputFlags
        uint32_t uiApplicableModes[CUtils::ENbDirections];              // bit field
        uint32_t uiApplicableStates[CUtils::ENbDirections];             // bit field
        const char* pcCardName;
        int32_t aiDeviceId[CUtils::ENbDirections];
        pcm_config astPcmConfig[CUtils::ENbDirections];
        const char* pcSlaveRoutes;                  // separated coma literal list of routes
    };

    static const uint32_t _uiNbPorts;

    static const uint32_t _uiNbPortGroups;

    static const uint32_t _uiNbRoutes;

    static const char* const _acPorts[];

    static const char* const _acPortGroups[];

    static const s_route_t _astAudioRoutes[];
};
};        // namespace android

