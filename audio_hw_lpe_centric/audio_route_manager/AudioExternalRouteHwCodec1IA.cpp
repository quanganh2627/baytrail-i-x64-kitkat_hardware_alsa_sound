/* AudioExternalRouteHwCodec1IA.cpp
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
#define LOG_TAG "AudioExternalRouteHwCodec1IA"

#include "AudioExternalRouteHwCodec1IA.h"

#define DEVICE_OUT_BUILTIN_ALL (0)

#define base    CAudioExternalRoute

namespace android_audio_legacy
{

const string strRouteName = "AudioExternalRouteHwCodec1IA";

CAudioExternalRouteHwCodec1IA::CAudioExternalRouteHwCodec1IA(uint32_t uiRouteId, CAudioPlatformState *platformState) :
    CAudioExternalRoute(uiRouteId, strRouteName, platformState)
{
}

bool CAudioExternalRouteHwCodec1IA::isApplicable(uint32_t , int , bool )
{
    return false;
}

}       // namespace android
