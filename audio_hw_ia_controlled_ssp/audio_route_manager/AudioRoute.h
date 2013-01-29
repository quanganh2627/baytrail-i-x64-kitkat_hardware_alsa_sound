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

#define INPUT        false
#define OUTPUT       true

#include <hardware_legacy/AudioHardwareBase.h>

namespace android_audio_legacy
{
class ALSAStreamOps;
class CAudioPort;
class CAudioPlatformState;

class CAudioRoute
{

public:

    enum RouteType {
        EStreamRoute,
        EExternalRoute,
        ECompressedStreamRoute,

        ENbRouteType
    };

    CAudioRoute(uint32_t uiRouteIndex, CAudioPlatformState *pPlatformState);
    virtual           ~CAudioRoute();

    void addPort(CAudioPort* pPort);

    // From AudioRouteManager
    virtual bool isApplicable(uint32_t uidevices, int iMode, bool bIsOut, uint32_t uiFlags = 0) const;

    // From RouteManager
    virtual void resetAvailability();

    // From Route Manager
    virtual void setBorrowed(bool bIsOut);

    // From RouteManager
    virtual bool currentlyBorrowed(bool bIsOut) const;

    virtual bool willBeBorrowed(bool bIsOut) const;

    // From Port
    void setCondemned();

    bool isCondemned() const;

    const string& getName() const { return _strName; }

    uint32_t getSlaveRoute() const { return _uiSlaveRoutes; }

    virtual RouteType getRouteType() const = 0;

    // Route order - for external until Manager under PFW
    virtual status_t route(bool bIsOut);

    // UnRoute order - for external until Manager under PFW
    virtual void unRoute(bool bIsOut);
    virtual void configure(bool bIsOut) { return ; }
    uint32_t getRouteId() const {return _uiRouteId; }

    // Filters the unroute/route
    // Returns true if a route is currently borrowed, will be borrowed
    // after reconsiderRouting but needs to be reconfigured
    virtual bool needReconfiguration(bool bIsOut) const;

protected:
    CAudioRoute(const CAudioRoute &);
    CAudioRoute& operator = (const CAudioRoute &);

private:
    string _strName;

    // A route is connected to 2 port, one call be NULL if no mutual exclusion exists on this port
    CAudioPort* _pPort[2];

    bool _bCondemned;

    uint32_t _uiRouteId;

protected:
    bool _bCurrentlyBorrowed[2];

    bool _bWillBeBorrowed[2];

public:
    uint32_t _uiApplicableDevices[2];

    uint32_t _uiApplicableModes[2];

    uint32_t _uiApplicableFlags[2];

    uint32_t _uiApplicableStreamType[2];

    uint32_t _uiSlaveRoutes;

    CAudioPlatformState* _pPlatformState;
};
// ----------------------------------------------------------------------------

};        // namespace android

