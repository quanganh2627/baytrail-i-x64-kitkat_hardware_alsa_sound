/* AudioRouteMSICVoice.h
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

#ifndef ANDROID_AUDIO_ROUTE_MSIC_VOICE_H
#define ANDROID_AUDIO_ROUTE_MSIC_VOICE_H

#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>

#include <utils/Log.h>
#include <utils/String8.h>

#include "AudioRoute.h"

namespace android_audio_legacy
{
class AudioRouteMSICVoice : public AudioRoute
{
public:
    AudioRouteMSICVoice(String8 aName) : AudioRoute(aName) {};
    virtual bool isApplicable(uint32_t devices, int mode, bool bForOutput);
};

// ----------------------------------------------------------------------------

};        // namespace android
#endif    // ANDROID_AUDIO_ROUTE_MSIC_VOICE_H
