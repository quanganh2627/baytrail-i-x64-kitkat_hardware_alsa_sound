/* AudioStreamInALSA.cpp
 **
 ** Copyright 2011 Intel Corporation
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

#define LOG_TAG "RouteManager/Port"

#include "AudioPort.h"
#include "AudioPortGroup.h"
#include "AudioRoute.h"

#include "AudioPlatformHardware.h"


namespace android_audio_legacy
{

CAudioPort::CAudioPort(uint32_t uiPortIndex) :
    _acName(CAudioPlatformHardware::getPortName(uiPortIndex)),
    _uiPortId(CAudioPlatformHardware::getPortId(uiPortIndex)),
    _portGroupList(0),
    _pRouteAttached(0),
    _bCondemned(false),
    _bBorrowed(false)
{
    ALOGD("Port %s", _acName.c_str());
}

CAudioPort::~CAudioPort()
{

}

void CAudioPort::resetAvailability()
{
    _bBorrowed = false;
    _bCondemned = false;
    _pRouteAttached = NULL;
}

void CAudioPort::addGroupToPort(CAudioPortGroup* portGroup)
{
    ALOGD("%s", __FUNCTION__);
    if(!portGroup) {

        return ;
    }

    _portGroupList.push_back(portGroup);

    ALOGD("%s: added %s to %s", __FUNCTION__, portGroup->getName().c_str(), this->getName().c_str());
}

void CAudioPort::setCondemned(bool isCondemned)
{
    if (_bCondemned == isCondemned) {

        return ;
    }

    ALOGI("%s: port %s is condemned", __FUNCTION__, getName().c_str());

    isCondemned = _bCondemned;

    // Condemns now all route that use this port except the one that borrows the port
    RouteListIterator it;

    // Find the applicable route for this route request
    for (it = mRouteList.begin(); it != mRouteList.end(); ++it) {

        CAudioRoute* aRoute = *it;

        // No route is expected bo be attached on this port!!!
        assert(!_pRouteAttached);

        aRoute->setCondemned();
    }
}

// Set the borrowed attribute of the port
// Parse the list of Group Port in which this port is involved
// And condemns all port within these port group that are
// mutual exclusive with this one.
void CAudioPort::setBorrowed(CAudioRoute *pRoute)
{

    if (_bBorrowed) {

        // Port is already borrowed, bailing out
        return ;
    }

    ALOGI("%s: port %s is borrowed", __FUNCTION__, getName().c_str());

    _bBorrowed = true;
    _pRouteAttached = pRoute;

    PortGroupListIterator it;

    for (it = _portGroupList.begin(); it != _portGroupList.end(); ++it) {

        CAudioPortGroup* aPortGroup = *it;

        aPortGroup->condemnMutualExclusivePort(this);
    }
}

// This function add the route to this list of routes
// that use this port
void CAudioPort::addRouteToPortUsers(CAudioRoute* pRoute)
{
    ALOGD("%s", __FUNCTION__);

    assert(pRoute);

    mRouteList.push_back(pRoute);

    ALOGD("%s: added %s route to %s port users", __FUNCTION__, pRoute->getName().c_str(), _acName.c_str());
}

}       // namespace android
