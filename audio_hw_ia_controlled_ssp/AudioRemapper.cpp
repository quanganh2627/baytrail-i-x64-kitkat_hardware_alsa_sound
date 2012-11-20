/* AudioRemapper.cpp
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

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioRemapper"
//#define LOG_NDEBUG 0

#include <stdlib.h>
#include <string.h>
#include <cutils/log.h>

#include "AudioRemapper.h"

#define base CAudioConverter

namespace android_audio_legacy{


// ----------------------------------------------------------------------------

CAudioRemapper::CAudioRemapper(SampleSpecItem eSampleSpecItem) :
    base(eSampleSpecItem)
{
}

status_t CAudioRemapper::doConfigure(const CSampleSpec& ssSrc, const CSampleSpec& ssDst)
{
    status_t ret = base::doConfigure(ssSrc, ssDst);
    if (ret != NO_ERROR) {

        return ret;
    }

    switch (ssSrc.getFormat()) {

    case AUDIO_FORMAT_PCM_16_BIT:

        if (ssSrc.getChannelCount() == 1 && ssDst.getChannelCount() == 2) {

            _pfnConvertSamples = (ConvertSamples)(&CAudioRemapper::convertMonoToStereoInS16);
        } else if (ssSrc.getChannelCount() == 2 && ssDst.getChannelCount() == 1) {

            _pfnConvertSamples = (ConvertSamples)(&CAudioRemapper::convertStereoToMonoInS16);
        } else {

            ret = INVALID_OPERATION;
        }
        break;

    case AUDIO_FORMAT_PCM_8_24_BIT:

        if (ssSrc.getChannelCount() == 1 && ssDst.getChannelCount() == 2) {

            _pfnConvertSamples = (ConvertSamples)(&CAudioRemapper::convertMonoToStereoInS24o32);
        } else if (ssSrc.getChannelCount() == 2 && ssDst.getChannelCount() == 1) {

            _pfnConvertSamples = (ConvertSamples)(&CAudioRemapper::convertStereoToMonoInS24o32);
        } else {

            ret = INVALID_OPERATION;
        }
        break;

    default:

        ret = INVALID_OPERATION;
        break;
    }

    return ret;
}

status_t CAudioRemapper::convertStereoToMonoInS16(const void *src, void *dst, const uint32_t inFrames, uint32_t *outFrames)
{
    const int16_t *src16 = (const int16_t *)src;
    int16_t *dst16 = (int16_t *)dst;
    size_t frames = inFrames;

    while (frames > 0) {

        // Average, if DUAL mono, better to take only one channel???
        *dst16++ = (int16_t)(((int32_t)*src16 + (int32_t)*(src16 + 1)) >> 1);
        src16 += 2;
        frames -= 1;
    }
    // Transformation is "iso"frames
    *outFrames = inFrames;
    return NO_ERROR;
}

status_t CAudioRemapper::convertMonoToStereoInS16(const void *src, void *dst, const uint32_t inFrames, uint32_t *outFrames)
{
    const int16_t *src16 = (const int16_t *)src;
    int16_t *dst16 = (int16_t *)dst;
    size_t frames = inFrames;

    while (frames > 0) {

        *dst16++ = *src16;
        *dst16++ = *src16++;
        frames -= 1;
    }
    // Transformation is "iso"frames
    *outFrames = inFrames;
    return NO_ERROR;
}

status_t CAudioRemapper::convertStereoToMonoInS24o32(const void *src, void *dst, const uint32_t inFrames, uint32_t *outFrames)
{
    const int32_t *src32 = (const int32_t *)src;
    int32_t *dst32 = (int32_t *)dst;
    size_t frames = inFrames;

    while (frames > 0) {

        // Calculating average value of the two channels.
        // If a dual mono stream has to be processed, it would be more efficient to take only one of the two channels
        *dst32++ = (*src32 + (int32_t)*(src32 + 1)) >> 1;
        src32 += 2;
        frames -= 1;
    }
    // Transformation is "iso"frames
    *outFrames = inFrames;

    return NO_ERROR;
}

status_t CAudioRemapper::convertMonoToStereoInS24o32(const void* src, void* dst, const uint32_t inFrames, uint32_t *outFrames)
{
    const int32_t *src32 = (const int32_t *)src;
    int32_t *dst32 = (int32_t *)dst;
    size_t frames = inFrames;

    while (frames > 0) {

        *dst32++ = *src32;
        *dst32++ = *src32++;
        frames -= 1;
    }
    // Transformation is "iso"frames
    *outFrames = inFrames;

    return NO_ERROR;
}

// ----------------------------------------------------------------------------
}; // namespace android
