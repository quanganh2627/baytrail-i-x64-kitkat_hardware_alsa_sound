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

namespace android_audio_legacy
{

CAudioRoute::CAudioRoute(uint32_t uiRouteId, const string& strName, CAudioPlatformState* pPlatformState) :
    _strName(strName),
    _uiRouteId(uiRouteId),
    _pPlatformState(pPlatformState)
{
    LOGD("%s: %s", __FUNCTION__, _strName.c_str());

    _bCurrentlyBorrowed[0] = false;
    _bCurrentlyBorrowed[1] = false;
    _bWillBeBorrowed[0] = false;
    _bWillBeBorrowed[1] = false;
}

CAudioRoute::~CAudioRoute()
{

}

status_t CAudioRoute::route(bool bIsOut)
{
    LOGD("%s: %s", __FUNCTION__, getName().c_str());

    _bCurrentlyBorrowed[bIsOut] = true;

    return NO_ERROR;
}

void CAudioRoute::unRoute(bool bIsOut)
{
    LOGD("%s: %s", __FUNCTION__, getName().c_str());

    _bCurrentlyBorrowed[bIsOut] = false;
}



void CAudioRoute::resetAvailability()
{
    _bWillBeBorrowed[0] = false;
    _bWillBeBorrowed[1] = false;
}

void CAudioRoute::setBorrowed(bool bIsOut)
{
    if (_bWillBeBorrowed[bIsOut]) {

        // Route is already borrowed in in/out
        // Bailing out
        return ;
    }

    LOGD("%s: route %s is now borrowed in %s", __FUNCTION__, getName().c_str(), bIsOut? "PLAYBACK" : "CAPTURE");

    _bWillBeBorrowed[bIsOut] = true;
}

bool CAudioRoute::needReconfiguration(bool bIsOut)
{
    if (currentlyBorrowed(bIsOut) && willBeBorrowed(bIsOut)) {

        return true;
    }
    return false;
}

bool CAudioRoute::currentlyBorrowed(bool bIsOut)
{
//    LOGD("%s: route %s %s currently borrowed", __FUNCTION__, getName().c_str(), _bCurrentlyBorrowed[bIsOut]? "is": "isn't");
    return _bCurrentlyBorrowed[bIsOut];
}

bool CAudioRoute::willBeBorrowed(bool bIsOut)
{
//    LOGD("%s: route %s %s be borrowed", __FUNCTION__, getName().c_str(), _bWillBeBorrowed[bIsOut]? "will": "will not");
    return _bWillBeBorrowed[bIsOut];
}

}       // namespace android
