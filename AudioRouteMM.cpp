/* AudioRouteMM.cpp
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

#define LOG_TAG "AudioRouteMM"
#include <utils/Log.h>

#include "AudioHardwareALSA.h"

#include "AudioRouteMM.h"

#define DEVICE_OUT_MM_ALL (AudioSystem::DEVICE_OUT_EARPIECE | AudioSystem::DEVICE_OUT_SPEAKER | AudioSystem::DEVICE_OUT_WIRED_HEADSET |AudioSystem::DEVICE_OUT_WIRED_HEADPHONE | AudioSystem::DEVICE_OUT_AUX_DIGITAL | AudioSystem::DEVICE_OUT_WIDI_LOOPBACK)

#define DEVICE_IN_MM_ALL (AudioSystem::DEVICE_IN_WIRED_HEADSET | AudioSystem::DEVICE_IN_BACK_MIC | AudioSystem::DEVICE_IN_AUX_DIGITAL | AudioSystem::DEVICE_IN_BUILTIN_MIC)


namespace android_audio_legacy
{

bool AudioRouteMM::isApplicable(uint32_t devices, int mode, bool bForOutput)
{
    LOGD("isApplicable mode=%d devices=0x%x bForOutput=%d", mode, devices, bForOutput);
    if(bForOutput) {
        if ((devices & DEVICE_OUT_MM_ALL) && (mode == AudioSystem::MODE_NORMAL || mode == AudioSystem::MODE_RINGTONE))
            return true;
    }
    else if ((devices & DEVICE_IN_MM_ALL) && (mode == AudioSystem::MODE_NORMAL)) {

            return true;
    }
    return false;
}

}       // namespace android
