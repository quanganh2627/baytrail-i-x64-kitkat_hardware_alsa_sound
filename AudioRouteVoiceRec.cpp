/* AudioRouteVoiceRec.cpp
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

#define LOG_TAG "AudioRouteVoiceRec"
#include <utils/Log.h>

#include "AudioHardwareALSA.h"

#include "AudioRouteVoiceRec.h"

namespace android_audio_legacy
{

bool AudioRouteVoiceRec::isApplicable(uint32_t devices, int mode, bool bForOutput)
{
    LOGD("isApplicable mode=%d devices=0x%x bForOutput=%d", mode, devices, bForOutput);
    if(!bForOutput) {
        if((devices & AudioSystem::DEVICE_IN_VOICE_CALL) && (mode == AudioSystem::MODE_IN_CALL))
            return true;
    }
    return false;
}

}       // namespace android
