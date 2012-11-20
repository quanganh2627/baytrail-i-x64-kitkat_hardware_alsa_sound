/* AudioCompressedStreamRoute.cpp
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

#define LOG_TAG "RouteManager/CompressedStreamRoute"

#include "AudioCompressedStreamRoute.h"
#include "AudioPlatformState.h"
#include "AudioPlatformHardware.h"

#define base    CAudioRoute

namespace android_audio_legacy
{
//
// Basic needReconfiguration fonction
// A route is set to be reconfigured if all these conditions are true:
//      -it is currently used
//      -it will be used after reconsider routing
//      -routing conditions changed
// To be implemented by derivated classes if different
// route policy
//
bool CAudioCompressedStreamRoute::needReconfiguration(bool bIsOut) const
{
    //ALOGD("%s: route %s needs to be reconfigured??? %d %d", __FUNCTION__, getName().c_str(), base::needReconfiguration(bIsOut), _pPlatformState->hasPlatformStateChanged());
    return false;
}

bool CAudioCompressedStreamRoute::isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t uiFlags) const
{
    if (bIsOut && (uiFlags & _uiApplicableFlags[OUTPUT])) {

        return base::isApplicable(uidevices, iMode, bIsOut, uiFlags);
    }
    return false;
}

}       // namespace android
