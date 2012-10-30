/* AudioStreamRouteVoice.cpp
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
#define LOG_TAG "RouteHwCodecMedia"

#include "AudioStreamRouteVoice.h"

#define base CAudioStreamRoute

#define DEVICE_OUT_MM_ALL (AudioSystem::DEVICE_OUT_EARPIECE | \
                AudioSystem::DEVICE_OUT_SPEAKER | \
                AudioSystem::DEVICE_OUT_WIRED_HEADSET | \
                AudioSystem::DEVICE_OUT_WIRED_HEADPHONE)


#define DEVICE_IN_BUILTIN_ALL (AudioSystem::DEVICE_IN_WIRED_HEADSET | \
                AudioSystem::DEVICE_IN_BACK_MIC | \
                AudioSystem::DEVICE_IN_AUX_DIGITAL | \
                AudioSystem::DEVICE_IN_BUILTIN_MIC)


namespace android_audio_legacy
{

static const uint32_t devicesMask = DEVICE_OUT_MM_ALL | DEVICE_IN_BUILTIN_ALL;

const string strRouteName = "AudioStreamRouteVoice";

CAudioStreamRouteVoice::CAudioStreamRouteVoice(uint32_t uiRouteId,
                                             int iOutputDeviceId,
                                             int iInputDeviceId,
                                             int iCardId,
                                             const pcm_config& outputPcmConfig,
                                             const pcm_config& inputPcmConfig,
                                             CAudioPlatformState* platformState) :
    base(uiRouteId,
         strRouteName,
         iOutputDeviceId,
         iInputDeviceId,
         iCardId,
         outputPcmConfig,
         inputPcmConfig,
         platformState)
{
}

//
// This route is applicable for VoIP direct output and input streams only
//
bool CAudioStreamRouteVoice::isApplicable(uint32_t devices, int mode, bool bIsOut)
{
    return false;
}

}       // namespace android
