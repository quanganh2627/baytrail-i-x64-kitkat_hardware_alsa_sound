/* StreamRoute.cpp
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

#define LOG_TAG "RouteManager/StreamRoute"
#include <utils/Log.h>

#include "AudioStreamRoute.h"
#include <ALSAStreamOps.h>

#include "AudioPlatformHardware.h"


#define base    CAudioRoute

namespace android_audio_legacy
{

CAudioStreamRoute::CAudioStreamRoute(uint32_t uiRouteIndex,
                                     CAudioPlatformState *platformState) :
    CAudioRoute(uiRouteIndex, platformState)
{
    _pcCardName = CAudioPlatformHardware::getRouteCardName(uiRouteIndex);
    _iPcmDeviceId[OUTPUT] = CAudioPlatformHardware::getRouteDeviceId(uiRouteIndex, OUTPUT);
    _iPcmDeviceId[INPUT] = CAudioPlatformHardware::getRouteDeviceId(uiRouteIndex, INPUT);
    _pcmConfig[OUTPUT] = CAudioPlatformHardware::getRoutePcmConfig(uiRouteIndex, OUTPUT);
    _pcmConfig[INPUT] = CAudioPlatformHardware::getRoutePcmConfig(uiRouteIndex, INPUT);

    _pNewStreams[INPUT] = NULL;
    _pNewStreams[OUTPUT] = NULL;
    _pCurrentStreams[INPUT] = NULL;
    _pCurrentStreams[OUTPUT] = NULL;
}

//
// Basic identical conditions fonction
// To be implemented by derivated classes if different
// route policy
//
bool CAudioStreamRoute::needReconfiguration(bool bIsOut) const
{
    // TBD: what conditions will lead to set the need reconfiguration flag for this route???
    // The route needs reconfiguration except if:
    //      - still borrowed by the same stream
    //      - the stream is using the same device
    //      - bluetooth noise reduction and echo cancelation is not changed
    //      - HAC mode is not changed
    //      - TTY direction is not changed

    if (base::needReconfiguration(bIsOut) &&
            (_pPlatformState->hasPlatformStateChanged(
                CAudioPlatformState::EBtHeadsetNrEcChange |
                CAudioPlatformState::EHacModeChange |
                CAudioPlatformState::ETtyDirectionChange) ||
            (_pCurrentStreams[bIsOut] != _pNewStreams[bIsOut]) ||
            (_pCurrentStreams[bIsOut]->getCurrentDevice() !=  _pNewStreams[bIsOut]->getNewDevice()))) {
        return true;
    }
    return false;
}

status_t CAudioStreamRoute::route(bool bIsOut)
{
    return openStream(bIsOut);
}

void CAudioStreamRoute::unRoute(bool bIsOut)
{
    closeStream(bIsOut);
}

status_t CAudioStreamRoute::openStream(bool bIsOut)
{
    status_t err = NO_ERROR;

    if (_pNewStreams[bIsOut]) {

        err = _pNewStreams[bIsOut]->doOpen();

        if (err != NO_ERROR) {

            // Failed to open output stream -> bailing out
            return err;
        }
        _pCurrentStreams[bIsOut] = _pNewStreams[bIsOut];

        // Consume the new device(s)
        _pCurrentStreams[bIsOut]->setCurrentDevice(_pCurrentStreams[bIsOut]->getNewDevice());
    }

    return NO_ERROR;
}

void CAudioStreamRoute::closeStream(bool bIsOut)
{
    // First unroute input stream
    if (_pCurrentStreams[bIsOut]) {

        _pCurrentStreams[bIsOut]->doClose();

        _pCurrentStreams[bIsOut] = NULL;
    }
}

void CAudioStreamRoute::resetAvailability()
{
    if (_pNewStreams[INPUT]) {

        _pNewStreams[INPUT]->resetRoute();
        _pNewStreams[INPUT] = NULL;
    }

    if (_pNewStreams[OUTPUT]) {

        _pNewStreams[OUTPUT]->resetRoute();
        _pNewStreams[OUTPUT] = NULL;
    }


    base::resetAvailability();
}

status_t CAudioStreamRoute::setStream(ALSAStreamOps* pStream)
{
    ALOGD("%s to %s route", __FUNCTION__, getName().c_str());
    bool bIsOut = pStream->isOut();

    assert(!_pNewStreams[bIsOut]);

    _pNewStreams[bIsOut] = pStream;

    _pNewStreams[bIsOut]->setNewRoute(this);

    return NO_ERROR;
}

bool CAudioStreamRoute::isApplicable(uint32_t uiDevices, int mode, bool bIsOut, uint32_t uiFlags) const
{
    ALOGI("%s: is Route %s applicable? ",__FUNCTION__, getName().c_str());
    ALOGI("%s: \t\t\t bIsOut=%s && uiFlags=0x%X & _uiApplicableFlags[%s]=0x%X", __FUNCTION__,
          bIsOut? "output" : "input",
          uiFlags,
          bIsOut? "output" : "input",
          _uiApplicableFlags[bIsOut]);

    if (!bIsOut && (uiFlags & _uiApplicableFlags[bIsOut]) == 0) {

        return false;
    }
    // Base class does not have much work to do than checking
    // if no stream is already using it and if not condemened
    return base::isApplicable(uiDevices, mode, bIsOut);
}

bool CAudioStreamRoute::available(bool bIsOut)
{
    // A route is available if no stream is already using it and if not condemened
    return (!isCondemned() && !_pNewStreams[bIsOut]);
}

bool CAudioStreamRoute::currentlyBorrowed(bool bIsOut) const
{
    return !!_pCurrentStreams[bIsOut];
}

bool CAudioStreamRoute::willBeBorrowed(bool bIsOut) const
{
    return !!_pNewStreams[bIsOut];
}

int CAudioStreamRoute::getPcmDeviceId(bool bIsOut) const
{
    return _iPcmDeviceId[bIsOut];
}

const pcm_config& CAudioStreamRoute::getPcmConfig(bool bIsOut) const
{
    return _pcmConfig[bIsOut];
}

const char* CAudioStreamRoute::getCardName() const
{
    return _pcCardName;
}

}       // namespace android
