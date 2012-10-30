/* ExternalRoute.h
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

using namespace std;

#include "AudioRoute.h"

namespace android_audio_legacy
{

class CAudioExternalRoute : public CAudioRoute
{
public:
    CAudioExternalRoute(uint32_t uiRouteId, const string& strName, CAudioPlatformState* platformState) : CAudioRoute(uiRouteId, strName, platformState) {}

    virtual RouteType getRouteType() const { return CAudioRoute::EExternalRoute; }

protected:
    // Filters the unroute/route
    virtual bool needReconfiguration(bool bIsOut);
};
// ----------------------------------------------------------------------------

};        // namespace android

