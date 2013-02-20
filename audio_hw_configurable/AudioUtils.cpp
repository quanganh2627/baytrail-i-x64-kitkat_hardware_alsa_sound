/*
 ** Copyright 2013 Intel Corporation
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

#include <stdlib.h>
#include <utils/Log.h>
#include <system/audio.h>
#include "SampleSpec.h"
#include "AudioUtils.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioUtils"

using namespace android;
using namespace std;

namespace android_audio_legacy {

#define FRAME_ALIGNEMENT_ON_16  16

uint32_t CAudioUtils::alignOn16(uint32_t u)
{
    return (u + (FRAME_ALIGNEMENT_ON_16 - 1)) & ~(FRAME_ALIGNEMENT_ON_16 - 1);
}

ssize_t CAudioUtils::convertSrcToDstInBytes(ssize_t bytes, const CSampleSpec& ssSrc, const CSampleSpec& ssDst)
{
    return ssDst.convertFramesToBytes(convertSrcToDstInFrames(ssSrc.convertBytesToFrames(bytes), ssSrc, ssDst));
}

ssize_t CAudioUtils::convertSrcToDstInFrames(ssize_t frames, const CSampleSpec& ssSrc, const CSampleSpec& ssDst)
{
    LOG_ALWAYS_FATAL_IF(ssSrc.getSampleRate() == 0);
    return ((uint64_t)frames * ssDst.getSampleRate() + ssSrc.getSampleRate() - 1) / ssSrc.getSampleRate();
}

// This fonction translates the format from tiny ALSA
// to AudioSystem enum
int CAudioUtils::convertTinyToHalFormat(pcm_format format)
{
    int convFormat;

    switch(format) {

    case PCM_FORMAT_S16_LE:
        convFormat = AUDIO_FORMAT_PCM_16_BIT;
        break;
    case PCM_FORMAT_S32_LE:
        convFormat = AUDIO_FORMAT_PCM_32_BIT;
        break;
    default:
        ALOGE("%s: format not recognized", __FUNCTION__);
        convFormat = AUDIO_FORMAT_INVALID;
        break;
    }
    return convFormat;
}

pcm_format CAudioUtils::convertHalToTinyFormat(int format)
{
    pcm_format convFormat;

    switch(format) {

    case AUDIO_FORMAT_PCM_16_BIT:
        convFormat = PCM_FORMAT_S16_LE;
        break;
    case AUDIO_FORMAT_PCM_32_BIT:
        convFormat = PCM_FORMAT_S32_LE;
        break;
    default:
        ALOGE("%s: format not recognized", __FUNCTION__);
        convFormat = PCM_FORMAT_S16_LE;
        break;
    }
    return convFormat;
}

// This function return the card number associated with the card ID (name)
// passed as argument
int CAudioUtils::getCardNumberByName(const char* name)

{
    char id_filepath[PATH_MAX] = {0};
    char number_filepath[PATH_MAX] = {0};
    ssize_t written;

    snprintf(id_filepath, sizeof(id_filepath), "/proc/asound/%s", name);

    written = readlink(id_filepath, number_filepath, sizeof(number_filepath));
    if (written < 0) {
        ALOGE("Sound card %s does not exist", name);
        return written;
    } else if (written >= (ssize_t)sizeof(id_filepath)) {
        // This will probably never happen
        return -ENAMETOOLONG;
    }

    // We are assured, because of the check in the previous elseif, that this
    // buffer is null-terminated.  So this call is safe.
    // 4 == strlen("card")
    return atoi(number_filepath + 4);
}

uint32_t CAudioUtils::convertUsecToMsec(uint32_t uiTimeUsec)
{
    // Round up to the nearest Msec
    return ((uiTimeUsec + 999) / 1000);
}

}; // namespace android

