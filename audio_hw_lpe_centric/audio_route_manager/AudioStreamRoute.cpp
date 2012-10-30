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

#define INPUT_STREAM        false
#define OUTPUT_STREAM       true

#define base    CAudioRoute

namespace android_audio_legacy
{

CAudioStreamRoute::CAudioStreamRoute(uint32_t uiRouteId,
                                     const string &strName,
                                     int iOutputDeviceId,
                                     int iInputDeviceId,
                                     int iCardId,
                                     const pcm_config &outputPcmConfig,
                                     const pcm_config &inputPcmConfig,
                                     CAudioPlatformState *platformState) :
    CAudioRoute(uiRouteId, strName, platformState),
    _iCardId(iCardId)
{
    LOGD("%s: %s", __FUNCTION__, strName.c_str());
    _iDeviceId[OUTPUT_STREAM] = iOutputDeviceId;
    _iDeviceId[INPUT_STREAM] = iInputDeviceId;
    _pcmConfig[OUTPUT_STREAM] = outputPcmConfig;
    _pcmConfig[INPUT_STREAM] = inputPcmConfig;
    _pNewStreams[INPUT_STREAM] = NULL;
    _pNewStreams[OUTPUT_STREAM] = NULL;
    _pCurrentStreams[INPUT_STREAM] = NULL;
    _pCurrentStreams[OUTPUT_STREAM] = NULL;
}

//
// Basic identical conditions fonction
// To be implemented by derivated classes if different
// route policy
// TODO: what are the condition on MRFLD to consider that a route
// needs to be reconfigured
//
bool CAudioStreamRoute::needReconfiguration(bool bIsOut)
{
//    LOGD("%s: %s isOut=%d", __FUNCTION__, getName().c_str(), bIsOut);
    // TBD: what conditions will lead to set the need reconfiguration flag for this route???
    // The route needs reconfiguration except if:
    //      - still borrowed by the same stream
    //      - the stream is using the same device -> NO FOR MRFLD!!!
    if (base::needReconfiguration(bIsOut) ||
            (_pCurrentStreams[bIsOut] != _pNewStreams[bIsOut])) {

        return true;
    }
    return false;
}

status_t CAudioStreamRoute::route(bool bIsOut)
{
//    base::route(bIsOut);
    return openStream(bIsOut);
}

void CAudioStreamRoute::unRoute(bool bIsOut)
{
    closeStream(bIsOut);
 //   base::unRoute(bIsOut);
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
    if (_pNewStreams[INPUT_STREAM]) {

        _pNewStreams[INPUT_STREAM]->resetRoute();
        _pNewStreams[INPUT_STREAM] = NULL;
    }

    if (_pNewStreams[OUTPUT_STREAM]) {

        _pNewStreams[OUTPUT_STREAM]->resetRoute();
        _pNewStreams[OUTPUT_STREAM] = NULL;
    }


    base::resetAvailability();
}

status_t CAudioStreamRoute::setStream(ALSAStreamOps* pStream)
{
    LOGD("%s to %s route", __FUNCTION__, getName().c_str());
    bool bIsOut = pStream->isOut();

    assert(!_pNewStreams[bIsOut]);

    _pNewStreams[bIsOut] = pStream;

    _pNewStreams[bIsOut]->setNewRoute(this);

    return NO_ERROR;
}

bool CAudioStreamRoute::isApplicable(uint32_t , int , bool bIsOut)
{
    // Base class does not have much work to do than checking
    // if the route is already borrowed (in the future...)...
    return !(willBeBorrowed(bIsOut));
}

bool CAudioStreamRoute::currentlyBorrowed(bool bIsOut)
{
//    LOGD("%s: route %s in %s %s borrowed", __FUNCTION__, getName().c_str(), bIsOut? "PLAYBACK" : "CAPTURE", _pCurrentStreams[bIsOut]? "is" : "isn't");
    return !!_pCurrentStreams[bIsOut];
}

bool CAudioStreamRoute::willBeBorrowed(bool bIsOut)
{
//    LOGD("%s: route %s in %s %s be borrowed", __FUNCTION__, getName().c_str(), bIsOut? "PLAYBACK" : "CAPTURE", _pNewStreams[bIsOut]? "will" : "won't");
    return !!_pNewStreams[bIsOut];
}

int CAudioStreamRoute::getPcmDevice(bool bIsOut) const
{
    return _iDeviceId[bIsOut];
}

const pcm_config& CAudioStreamRoute::getPcmConfig(bool bIsOut) const
{
    return _pcmConfig[bIsOut];
}

int CAudioStreamRoute::getCardId() const
{
    return _iCardId;
}

}       // namespace android
