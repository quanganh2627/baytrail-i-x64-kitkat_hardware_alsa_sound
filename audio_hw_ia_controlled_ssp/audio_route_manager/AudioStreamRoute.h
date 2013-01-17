/* AudioStreamRoute.h
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

#include <tinyalsa/asoundlib.h>

#include "AudioRoute.h"

namespace android_audio_legacy
{

class ALSAStreamOps;

class CAudioStreamRoute : public CAudioRoute
{
public:
    CAudioStreamRoute(uint32_t uiRouteIndex,
                      CAudioPlatformState* platformState);

    int getPcmDeviceId(bool bIsOut) const;

    const pcm_config& getPcmConfig(bool bIsOut) const;

    const char* getCardName() const;

    virtual RouteType getRouteType() const { return CAudioRoute::EStreamRoute; }

    // Assign a new stream to this route
    status_t setStream(ALSAStreamOps* pStream);

    // Route order - for external until Manager under PFW
    virtual status_t route(bool bForOutput);

    // Unroute order - for external until Manager under PFW
    virtual void unroute(bool bForOutput);

    virtual void configure(bool bIsOut);

    // Inherited for AudioRoute - called from RouteManager
    virtual void resetAvailability();

    // Inherited from AudioRoute - Called from AudioRouteManager
    virtual bool available(bool bIsOut);

    // Inherited from AudioRoute - Called from RouteManager
    virtual bool currentlyBorrowed(bool bIsOut) const;

    // Inherited from AudioRoute - Called from RouteManager
    virtual bool willBeBorrowed(bool bIsOut) const;

    // Inherited for AudioRoute - called from RouteManager
    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t uiFlagsInputSource = 0) const;

    // Filters the unroute/route
    virtual bool needReconfiguration(bool bIsOut) const;

    // Get amount of silence delay upon stream opening
    virtual uint32_t getOutputSilencePrologMs() const { return 0; }

protected:
    ALSAStreamOps* _pCurrentStreams[2];
    ALSAStreamOps* _pNewStreams[2];

private:
    int _iPcmDeviceId[2];
    const char* _pcCardName;
    pcm_config _pcmConfig[2];
};
// ----------------------------------------------------------------------------

};        // namespace android

