/*
 **
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

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioRemapper"

#include <cutils/log.h>
#include "AudioRemapper.h"

#define base CAudioConverter

using namespace android;

namespace android_audio_legacy{


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

        if (ssSrc.isMono() && ssDst.isStereo()) {

            _pfnConvertSamples = (SampleConverter)(&CAudioRemapper::convertMonoToStereoInS16);
        } else if (ssSrc.isStereo() && ssDst.isMono()) {

            _pfnConvertSamples = (SampleConverter)(&CAudioRemapper::convertStereoToMonoInS16);
        } else {

            ret = INVALID_OPERATION;
        }
        break;

    case AUDIO_FORMAT_PCM_8_24_BIT:

        if (ssSrc.isMono() && ssDst.isStereo()) {

            _pfnConvertSamples = (SampleConverter)(&CAudioRemapper::convertMonoToStereoInS24o32);
        } else if (ssSrc.isStereo() && ssDst.isMono()) {

            _pfnConvertSamples = (SampleConverter)(&CAudioRemapper::convertStereoToMonoInS24o32);
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

status_t CAudioRemapper::convertStereoToMonoInS16(const void* src, void* dst, const uint32_t inFrames, uint32_t* outFrames)
{
    const int16_t* src16 = (const int16_t* )src;
    int16_t* dst16 = (int16_t* )dst;
    size_t frames;

    for (frames = 0; frames < inFrames; frames++) {

        int32_t srcLeft = src16[2 * frames];
        int32_t srcRight = src16[2 * frames + 1];
        dst16[frames] = (srcLeft + srcRight) / 2;
    }

    // Transformation is "iso"frames
    *outFrames = inFrames;
    return NO_ERROR;
}

status_t CAudioRemapper::convertMonoToStereoInS16(const void* src, void* dst, const uint32_t inFrames, uint32_t* outFrames)
{
    const int16_t* src16 = (const int16_t* )src;
    int16_t *dst16 = (int16_t *)dst;
    size_t frames = 0;

    for (frames = 0; frames < inFrames; frames++) {

        int16_t* dstLeft = &dst16[2 * frames];
        int16_t* dstRight = &dst16[2 * frames + 1];
        *dstLeft = src16[frames];
        *dstRight = src16[frames];
    }

    // Transformation is "iso"frames
    *outFrames = inFrames;
    return NO_ERROR;
}

status_t CAudioRemapper::convertStereoToMonoInS24o32(const void* src, void* dst, const uint32_t inFrames, uint32_t* outFrames)
{
    const int32_t* src32 = (const int32_t* )src;
    int32_t* dst32 = (int32_t* )dst;
    size_t frames;

    for (frames = 0; frames < inFrames; frames++) {

        int32_t srcLeft = src32[2 * frames];
        int32_t srcRight = src32[2 * frames + 1];
        dst32[frames] = (srcLeft + srcRight) / 2;
    }
    // Transformation is "iso"frames
    *outFrames = inFrames;

    return NO_ERROR;
}

status_t CAudioRemapper::convertMonoToStereoInS24o32(const void* src, void* dst, const uint32_t inFrames, uint32_t* outFrames)
{
    const int32_t* src32 = (const int32_t* )src;
    int32_t *dst32 = (int32_t *)dst;
    size_t frames = 0;

    for (frames = 0; frames < inFrames; frames++) {

        int32_t* dstLeft = &dst32[2 * frames];
        int32_t* dstRight = &dst32[2 * frames + 1];
        *dstLeft = src32[frames];
        *dstRight = src32[frames];
    }
    // Transformation is "iso"frames
    *outFrames = inFrames;

    return NO_ERROR;
}

}; // namespace android
