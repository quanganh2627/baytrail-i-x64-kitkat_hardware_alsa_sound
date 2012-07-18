/* AudioRouteManager.cpp
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

#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string>

#define LOG_TAG "AudioRouteManager"
#include <utils/Log.h>

#include "AudioHardwareALSA.h"
#include "AudioRouteManager.h"
#include "AudioRoute.h"

namespace android_audio_legacy
{

AudioRouteManager::AudioRouteManager()
{

}

AudioRouteManager::~AudioRouteManager()
{
    AudioRouteListIterator it;

    // Delete all routes
    for (it = _audioRouteList.begin(); it != _audioRouteList.end(); ++it) {

        delete *it;
    }
}

status_t AudioRouteManager::route(ALSAStreamOps* pStream, uint32_t devices, int mode, bool bForOutput)
{
//    status_t status;
    AudioRoute *aRoute;
    ALOGD("route mode=%d devices=0x%x bForOutput=%d", mode, devices, bForOutput);

    if (!devices)
    {
        // Request with NULL device -> unroute request
        // Set the route to the NULL route
        ALOGD("%s: null device => unroute", __FUNCTION__);
        aRoute = NULL;
    }
    else
    {
        if (_audioRouteList.empty()) {

            // No domains associated
            return BAD_VALUE;
        }

        aRoute = getRoute(devices, mode, bForOutput);
        if(!aRoute)
            return BAD_VALUE;
    }

    // Associate the stream and the route found
    return pStream->setRoute(aRoute, devices, mode);
}

status_t AudioRouteManager::addRoute(AudioRoute *route)
{
    ALOGD("addRoute");
    if(route)
        _audioRouteList.push_back(route);
    else
        return BAD_VALUE;

    return NO_ERROR;
}


AudioRoute* AudioRouteManager::getRoute(uint32_t devices, int mode, bool bForOutput)
{
    ALOGD("getRoute");

    AudioRoute *aRoute =  NULL;
    AudioRouteListIterator it;

    // Find the applicable route for this route request
    for (it = _audioRouteList.begin(); it != _audioRouteList.end(); ++it) {

        aRoute = *it;
        if(aRoute->isApplicable(devices, mode, bForOutput)) {
            ALOGD("route has been found");
            return aRoute;
        }
    }
    LOGE("route NO ROUTE FOUND!!!");
    return NULL;
}

AudioRoute* AudioRouteManager::findRouteByName(const String8& name)
{
    ALOGD("findRouteByName");
    AudioRoute* aRoute =  NULL;
    AudioRouteListIterator it;

    // Find the applicable route for this route request
    for (it = _audioRouteList.begin(); it != _audioRouteList.end(); ++it) {
        aRoute = *it;
        if(name == aRoute->getName()) {
            break;
        }
    }
    return aRoute;
}

status_t AudioRouteManager::setRouteAccessible(const String8& name, bool isAccessible, int mode, AudioRoute::Direction dir)
{
    ALOGD("setRouteAccessible");
    AudioRoute* aRoute =  NULL;
    AudioRouteListIterator it;

    // Find the applicable route for this route request
    aRoute = findRouteByName(name);

    if(aRoute != NULL)
        aRoute->setRouteAccessible(isAccessible, mode, dir);
    else
        return BAD_VALUE;

    return NO_ERROR;
}

}       // namespace android
