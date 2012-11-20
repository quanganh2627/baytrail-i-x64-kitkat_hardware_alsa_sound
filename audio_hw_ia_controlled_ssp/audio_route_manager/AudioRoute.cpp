/* AudioRoute.cpp
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

#define LOG_TAG "RouteManager/Route"
//#define LOG_NDEBUG 0
#include <utils/Log.h>
#include <string>

#include "AudioRoute.h"
#include "AudioPort.h"

#include "AudioPlatformHardware.h"


namespace android_audio_legacy
{

CAudioRoute::CAudioRoute(uint32_t uiRouteIndex, CAudioPlatformState* pPlatformState) :
    _pPlatformState(pPlatformState)
{
    _strName = CAudioPlatformHardware::getRouteName(uiRouteIndex);
    _uiRouteId = CAudioPlatformHardware::getRouteId(uiRouteIndex);

    _uiApplicableDevices[OUTPUT] = CAudioPlatformHardware::getRouteApplicableDevices(uiRouteIndex, OUTPUT);
    _uiApplicableModes[OUTPUT] = CAudioPlatformHardware::getRouteApplicableModes(uiRouteIndex, OUTPUT);
    _uiApplicableDevices[INPUT] = CAudioPlatformHardware::getRouteApplicableDevices(uiRouteIndex, INPUT);
    _uiApplicableModes[INPUT] = CAudioPlatformHardware::getRouteApplicableModes(uiRouteIndex, INPUT);
    _uiApplicableFlags[INPUT] = CAudioPlatformHardware::getRouteApplicableFlags(uiRouteIndex, INPUT);
    _uiApplicableFlags[OUTPUT] = CAudioPlatformHardware::getRouteApplicableFlags(uiRouteIndex, OUTPUT);
    _uiSlaveRoutes = CAudioPlatformHardware::getSlaveRoutes(uiRouteIndex);

    _pPort[OUTPUT] = NULL;
    _pPort[INPUT] = NULL;
    _bCurrentlyBorrowed[OUTPUT] = false;
    _bCurrentlyBorrowed[INPUT] = false;
    _bWillBeBorrowed[OUTPUT] = false;
    _bWillBeBorrowed[INPUT] = false;

}

CAudioRoute::~CAudioRoute()
{

}

void CAudioRoute::addPort(CAudioPort* pPort)
{
    if (pPort) {

        ALOGD("%s: %s to route %s", __FUNCTION__, pPort->getName().c_str(), getName().c_str());
        pPort->addRouteToPortUsers(this);
        if (!_pPort[0]) {

            _pPort[0]= pPort;
        }
        else {

            assert(!pPort[1]);
            _pPort[1] = pPort;
        }
    }
}

status_t CAudioRoute::route(bool bIsOut)
{
    ALOGD("%s: %s", __FUNCTION__, getName().c_str());

    _bCurrentlyBorrowed[bIsOut] = true;

    return NO_ERROR;
}

void CAudioRoute::unRoute(bool bIsOut)
{
    ALOGD("%s: %s", __FUNCTION__, getName().c_str());

    _bCurrentlyBorrowed[bIsOut] = false;
}



void CAudioRoute::resetAvailability()
{
    _bCondemned = false;
    _bWillBeBorrowed[0] = false;
    _bWillBeBorrowed[1] = false;
}

bool CAudioRoute::isApplicable(uint32_t uiDevices, int iMode, bool bIsOut, uint32_t) const
{
    ALOGI("%s: is Route %s applicable?", __FUNCTION__, getName().c_str());
    ALOGI("%s: \t\t\t (isCondemned()=%d willBeBorrowed(%s)=%d) && ", __FUNCTION__,
          isCondemned(), bIsOut? "output" : "input", willBeBorrowed(bIsOut));
    ALOGI("%s: \t\t\t ((1 << iMode)=0x%X & uiApplicableModes[%s]=0x%X) && ", __FUNCTION__,
          (1 << iMode), bIsOut? "output" : "input", _uiApplicableModes[bIsOut]);
    ALOGI("%s: \t\t\t (uiDevices=0x%X & _uiApplicableDevices[%s]=0x%X)", __FUNCTION__,
          uiDevices, bIsOut? "output" : "input", _uiApplicableDevices[bIsOut]);

    if (!isCondemned() &&
            !willBeBorrowed(bIsOut) &&
            ((1 << iMode) & _uiApplicableModes[bIsOut]) &&
            (uiDevices & _uiApplicableDevices[bIsOut])) {

        ALOGD("%s: Route %s is applicable", __FUNCTION__, getName().c_str());
        return true;
    }
    return false;
}

void CAudioRoute::setBorrowed(bool bIsOut)
{
    if (_bWillBeBorrowed[bIsOut]) {

        // Route is already borrowed in in/out, associated port already borrowed
        // Bailing out
        return ;
    }

    ALOGD("%s: route %s is now borrowed in %s", __FUNCTION__, getName().c_str(), bIsOut? "PLAYBACK" : "CAPTURE");

    _bWillBeBorrowed[bIsOut] = true;

    // Propagate the borrowed attribute to the ports
    // used by this route
    for(int i = 0; i < 2 ; i++) {

        if (!_pPort[i]) {

            continue;
        }
        _pPort[i]->setBorrowed(this);

    }
}

bool CAudioRoute::needReconfiguration(bool bIsOut) const
{
    //
    // Base class just check at least that the route is used currently
    // and will remain borrowed after routing
    //
    if (currentlyBorrowed(bIsOut) && willBeBorrowed(bIsOut)) {

        return true;
    }
    return false;
}

bool CAudioRoute::currentlyBorrowed(bool bIsOut) const
{
    return _bCurrentlyBorrowed[bIsOut];
}

bool CAudioRoute::willBeBorrowed(bool bIsOut) const
{
    return _bWillBeBorrowed[bIsOut];
}

void CAudioRoute::setCondemned()
{
    if (_bCondemned) {

        // Bailing out
        return ;
    }

    ALOGD("%s: route %s is now condemned", __FUNCTION__, getName().c_str());
    _bCondemned = true;
}

bool CAudioRoute::isCondemned() const
{
    return _bCondemned;
}

}       // namespace android
