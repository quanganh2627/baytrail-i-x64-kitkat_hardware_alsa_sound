/* AudioRouteManager.h
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

#ifndef ANDROID_AUDIO_ROUTE_MANAGER_H
#define ANDROID_AUDIO_ROUTE_MANAGER_H

#include <utils/List.h>

#include <hardware_legacy/AudioHardwareBase.h>

#include "AudioRoute.h"

namespace android_audio_legacy
{

class ALSAStreamOps;
class AudioRoute;

class AudioRouteManager
{
//    friend class AudioRoute;
    typedef List<AudioRoute*>::iterator AudioRouteListIterator;

public:
    AudioRouteManager();
    virtual           ~AudioRouteManager();

    status_t addRoute(AudioRoute *route);

    status_t setRouteAccessible(const String8& name, bool isAccessible, int mode, AudioRoute::Direction dir = AudioRoute::FullDuplex);

    status_t setSharedRouteAccessible(bool isAccessible, int mode, AudioRoute::Direction dir = AudioRoute::FullDuplex);

    status_t route(ALSAStreamOps* pStream, uint32_t devices, int mode, bool bForOutput);

protected:
    AudioRouteManager(const AudioRouteManager &);
    AudioRouteManager& operator = (const AudioRouteManager &);

    AudioRoute* getRoute(uint32_t devices, int mode, bool bForOutput);

private:
    AudioRoute* findRouteByName(const String8& name);
    List<AudioRoute*> _audioRouteList;
};
// ----------------------------------------------------------------------------

};        // namespace android
#endif    // ANDROID_AUDIO_ROUTE_MANAGER_H
