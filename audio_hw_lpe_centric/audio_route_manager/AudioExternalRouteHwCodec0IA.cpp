/* AudioExternalRouteHwCodec0IA.cpp
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

//#define LOG_NDEBUG 0
#define LOG_TAG "AudioExternalRouteHwCodec0IA"

#include "AudioExternalRouteHwCodec0IA.h"

#define DEVICE_OUT_BUILTIN_ALL (AudioSystem::DEVICE_OUT_EARPIECE | \
                    AudioSystem::DEVICE_OUT_SPEAKER | \
                    AudioSystem::DEVICE_OUT_WIRED_HEADSET | \
                    AudioSystem::DEVICE_OUT_WIRED_HEADPHONE)

#define DEVICE_IN_BUILTIN_ALL (AudioSystem::DEVICE_IN_BUILTIN_MIC | \
                    AudioSystem::DEVICE_IN_WIRED_HEADSET | \
                    AudioSystem::DEVICE_IN_BACK_MIC)

#define base    CAudioExternalRoute

namespace android_audio_legacy
{

const string strRouteName = "AudioExternalRouteHwCodec0IA";

CAudioExternalRouteHwCodec0IA::CAudioExternalRouteHwCodec0IA(uint32_t uiRouteId, CAudioPlatformState *platformState) :
    CAudioExternalRoute(uiRouteId, strRouteName, platformState)
{
}

bool CAudioExternalRouteHwCodec0IA::isApplicable(uint32_t devices, int mode, bool bIsOut)
{
    if (bIsOut) {

        if (devices & DEVICE_OUT_BUILTIN_ALL) {

            // This route is applicable if:
            //  -either At least one stream must be active to enable this route
            //  -or a voice call on Codec is on going
            return _pPlatformState->hasActiveStream(bIsOut) || _pPlatformState->isModemAudioAvailable();
        }
    } else {

        if (devices & DEVICE_IN_BUILTIN_ALL) {

            // This route is applicable if:
            //  -either At least one stream must be active to enable this route
            //  -or a voice call on Codec is on going
            return _pPlatformState->hasActiveStream(bIsOut) || _pPlatformState->isModemAudioAvailable();
        }
    }
    return false;
}

}       // namespace android
