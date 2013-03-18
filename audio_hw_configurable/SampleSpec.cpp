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

#include <cutils/log.h>
#include "SampleSpec.h"

namespace android_audio_legacy {

const uint32_t CSampleSpec::USEC_PER_SEC = 1000000;

#define SAMPLE_SPEC_ITEM_IS_VALID(eSampleSpecItem) LOG_ALWAYS_FATAL_IF(eSampleSpecItem < 0 || eSampleSpecItem >= ENbSampleSpecItems)

// Generic Accessor
void CSampleSpec::setSampleSpecItem(SampleSpecItem eSampleSpecItem, uint32_t uiValue)
{
    SAMPLE_SPEC_ITEM_IS_VALID(eSampleSpecItem);
    _auiSampleSpec[eSampleSpecItem] = uiValue;
}

uint32_t CSampleSpec::getSampleSpecItem(SampleSpecItem eSampleSpecItem) const
{
    SAMPLE_SPEC_ITEM_IS_VALID(eSampleSpecItem);
    return _auiSampleSpec[eSampleSpecItem];
}

size_t CSampleSpec::getFrameSize() const
{
    return audio_bytes_per_sample(getFormat()) * getChannelCount();
}

size_t CSampleSpec::convertBytesToFrames(size_t bytes) const
{
    LOG_ALWAYS_FATAL_IF(getFrameSize() == 0);
    return bytes / getFrameSize();
}

size_t CSampleSpec::convertFramesToBytes(size_t frames) const
{
    return frames * getFrameSize();
}

size_t CSampleSpec::convertFramesToUsec(uint32_t uiFrames) const
{
    return USEC_PER_SEC * (uint64_t)uiFrames / getSampleRate();
}

size_t CSampleSpec::convertUsecToframes(uint32_t uiIntervalUsec) const
{
    return (uint64_t)uiIntervalUsec * getSampleRate() / USEC_PER_SEC;
}

}; // namespace android

