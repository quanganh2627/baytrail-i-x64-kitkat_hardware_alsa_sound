/* Route.h
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

#include <string>

using namespace std;

#include <hardware_legacy/AudioHardwareBase.h>
#include "AudioPlatformState.h"

namespace android_audio_legacy
{
class ALSAStreamOps;
class CAudioPlatformState;

class CAudioRoute
{

public:

    enum RouteType {
        EStreamRoute,
        EExternalRoute
    };

    CAudioRoute(uint32_t routeId, const string &strName, CAudioPlatformState *pPlatformState);
    virtual           ~CAudioRoute();

    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut) = 0;

    // From RouteManager
    virtual void resetAvailability();

    // From Route Manager
    virtual void setBorrowed(bool bIsOut);

    // From RouteManager
    virtual bool currentlyBorrowed(bool bIsOut);

    virtual bool willBeBorrowed(bool bIsOut);

    const string& getName() const { return _strName; }

    virtual RouteType getRouteType() const = 0;

    // Route order - for external until Manager under PFW
    virtual status_t route(bool bIsOut);

    // UnRoute order - for external until Manager under PFW
    virtual void unRoute(bool bIsOut);

    uint32_t getRouteId() const {return _uiRouteId; }

    // Filters the unroute/route
    // Returns true if a route is currently borrowed, will be borrowed
    // after reconsiderRouting but needs to be reconfigured
    virtual bool needReconfiguration(bool bIsOut);

protected:
    CAudioRoute(const CAudioRoute &);
    CAudioRoute& operator = (const CAudioRoute &);

private:
    string _strName;

    uint32_t _uiRouteId;

protected:
    bool _bCurrentlyBorrowed[2];

    bool _bWillBeBorrowed[2];

public:
    CAudioPlatformState* _pPlatformState;
};
// ----------------------------------------------------------------------------

};        // namespace android

