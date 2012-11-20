/* PortGroup.h
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

#include "AudioPortGroup.h"

using namespace std;

namespace android_audio_legacy
{

class ALSAStreamOps;
class CAudioRoute;
class CAudioPort;

class CAudioPortGroup
{
    typedef list<CAudioPort*>::iterator PortListIterator;
    typedef list<CAudioPort*>::const_iterator PortListConstIterator;

public:
    CAudioPortGroup(uint32_t uiPortGroupIndex);
    virtual           ~CAudioPortGroup();

    // Add route to group
    void addPortToGroup(CAudioPort* port);

    // Condamns all the other port from this group
    void condemnMutualExclusivePort(const CAudioPort *port);

    const string& getName() const { return mName; }
private:
    string mName;

    // List of Ports that belongs to this PortGroup
    // All these port are mutual exlusive
    list<CAudioPort*> mPortList;
};
// ----------------------------------------------------------------------------

};        // namespace android

