/* RouteManager.cpp
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

#define LOG_TAG "AudioPortGroup"
#include <utils/Log.h>

#include "AudioPortGroup.h"
#include "AudioPort.h"

#include "AudioPlatformHardware.h"

namespace android_audio_legacy
{

CAudioPortGroup::CAudioPortGroup(uint32_t uiPortGroupIndex) :
    mName(CAudioPlatformHardware::getPortGroupName(uiPortGroupIndex)),
    mPortList(0)
{

}

CAudioPortGroup::~CAudioPortGroup()
{

}

void CAudioPortGroup::addPortToGroup(CAudioPort* port)
{
    ALOGD("%s", __FUNCTION__);
    if(!port) {

        return ;
    }

    mPortList.push_back(port);

    // Give the pointer on Group port back to the port
    port->addGroupToPort(this);

    ALOGD("%s: added %s to %s", __FUNCTION__, port->getName().c_str(), this->getName().c_str());
}

void CAudioPortGroup::condemnMutualExclusivePort(const CAudioPort* port)
{
    ALOGD("%s of port %s", __FUNCTION__, port->getName().c_str());
    if(!port) {

        return ;
    }
    PortListIterator it;

    // Find the applicable route for this route request
    for (it = mPortList.begin(); it != mPortList.end(); ++it) {

        CAudioPort* aPort = *it;
        if(aPort == port) {

            continue ;
        }
        aPort->setCondemned(true);
    }
}

}       // namespace android_audio_legacy
