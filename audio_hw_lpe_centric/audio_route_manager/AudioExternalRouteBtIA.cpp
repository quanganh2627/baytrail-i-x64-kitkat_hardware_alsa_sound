/* AudioExternalRouteBtIA.cpp
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
#define LOG_TAG "AudioExternalRouteBtIA"

#include "AudioExternalRouteBtIA.h"

#define DEVICE_OUT_BLUETOOTH_SCO_ALL (AudioSystem::DEVICE_OUT_BLUETOOTH_SCO | \
                AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET | \
                AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT)

#define DEVICE_IN_BLUETOOTH_SCO_ALL (AudioSystem::DEVICE_IN_BLUETOOTH_SCO_HEADSET)

#define base    CAudioExternalRoute

namespace android_audio_legacy
{

const string strRouteName = "AudioExternalRouteBtIA";

CAudioExternalRouteBtIA::CAudioExternalRouteBtIA(uint32_t uiRouteId, CAudioPlatformState *platformState) :
    CAudioExternalRoute(uiRouteId, strRouteName, platformState)
{
}

bool CAudioExternalRouteBtIA::isApplicable(uint32_t devices, int mode, bool bIsOut)
{
    if (!_pPlatformState->isBtEnabled()) {

        // BT chip is not enabled...
        return false;
    }
    if (bIsOut) {

        if (devices & DEVICE_OUT_BLUETOOTH_SCO_ALL) {

            // This route is applicable if:
            //  -either a stream is active
            //  -or a voice call on BT is on going
            return _pPlatformState->hasActiveStream(bIsOut) || _pPlatformState->isModemAudioAvailable();
        }
    } else {

        if (devices & DEVICE_IN_BLUETOOTH_SCO_ALL) {

            // This route is applicable if:
            //  -either a stream is active
            //  -or a voice call on BT is on going
            return _pPlatformState->hasActiveStream(bIsOut) || _pPlatformState->isModemAudioAvailable();
        }

    }
    return false;
}

}       // namespace android
