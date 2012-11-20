/* Port.h
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

#pragma once

#include <string>
#include <list>

using namespace std;

namespace android_audio_legacy
{
class ALSAStreamOps;
class CAudioPortGroup;
class CAudioRoute;

class CAudioPort
{
    typedef list<CAudioPortGroup*>::iterator PortGroupListIterator;
    typedef list<CAudioPortGroup*>::const_iterator PortGroupListConstIterator;
    typedef list<CAudioRoute*>::iterator RouteListIterator;
    typedef list<CAudioRoute*>::const_iterator RouteListConstIterator;

public:
    CAudioPort(uint32_t uiPortIndex);
    virtual           ~CAudioPort();

    // From PortGroup
    void setCondemned(bool isCondemned);

    // From Route: this port is now borrowed
    void setBorrowed(CAudioRoute* pRoute);

    void resetAvailability();

    const string& getName() const { return _acName; }

    uint32_t getPortId() const { return _uiPortId; }

    // Add a Route to the list of route that use this port
    void addRouteToPortUsers(CAudioRoute* pRoute);

    // From Group Port
    void addGroupToPort(CAudioPortGroup* portGroup);

protected:
    CAudioPort(const CAudioPort &);
    CAudioPort& operator = (const CAudioPort &);

private:
    string _acName;

    uint32_t _uiPortId;

    // list of Port groups to which this port belongs - can be null if this port does not have
    // any mutual exclusion issue
    list<CAudioPortGroup*> _portGroupList;

    CAudioRoute* _pRouteAttached;

    list<CAudioRoute*> mRouteList;

    bool _bCondemned;
    bool _bBorrowed;
};
// ----------------------------------------------------------------------------

};        // namespace android

