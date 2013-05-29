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

#define LOG_TAG "SampleSpec"

#include "SampleSpec.h"
#include <cutils/log.h>
#include <stdint.h>
#include <errno.h>
#include <limits>

using namespace std;

namespace android_audio_legacy {

const uint32_t CSampleSpec::USEC_PER_SEC = 1000000;

#define SAMPLE_SPEC_ITEM_IS_VALID(eSampleSpecItem) LOG_ALWAYS_FATAL_IF(eSampleSpecItem < 0 || eSampleSpecItem >= ENbSampleSpecItems)


CSampleSpec::CSampleSpec(uint32_t channel,
                         uint32_t format,
                         uint32_t rate)
{
    init(channel, format, rate);
}

CSampleSpec::CSampleSpec(uint32_t channel,
                         uint32_t format,
                         uint32_t rate,
                         const vector<ChannelsPolicy> &channelsPolicy)
{
    init(channel, format, rate);
    setChannelsPolicy(channelsPolicy);
}

void CSampleSpec::init(uint32_t channel,
                       uint32_t format,
                       uint32_t rate)
{
    _uiChannelMask = 0;
    setSampleSpecItem(EChannelCountSampleSpecItem, channel);
    setSampleSpecItem(EFormatSampleSpecItem, format);
    setSampleSpecItem(ERateSampleSpecItem, rate);
}

// Generic Accessor
void CSampleSpec::setSampleSpecItem(SampleSpecItem eSampleSpecItem, uint32_t uiValue)
{
    SAMPLE_SPEC_ITEM_IS_VALID(eSampleSpecItem);

    if (eSampleSpecItem == EChannelCountSampleSpecItem) {

        LOG_ALWAYS_FATAL_IF(uiValue >= MAX_CHANNELS);

        _aChannelsPolicy.clear();
        // Reset all the channels policy to copy by default
        for (uint32_t i = 0; i < uiValue; i++) {

            _aChannelsPolicy.push_back(ECopy);
        }
    }
    _auiSampleSpec[eSampleSpecItem] = uiValue;
}

void CSampleSpec::setChannelsPolicy(const vector<ChannelsPolicy>& channelsPolicy)
{
    LOG_ALWAYS_FATAL_IF(channelsPolicy.size() > _auiSampleSpec[EChannelCountSampleSpecItem]);
    _aChannelsPolicy = channelsPolicy;
}

CSampleSpec::ChannelsPolicy CSampleSpec::getChannelsPolicy(uint32_t uiChannelIndex) const
{
    LOG_ALWAYS_FATAL_IF(uiChannelIndex >= _aChannelsPolicy.size());
    return _aChannelsPolicy[uiChannelIndex];
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
    LOG_ALWAYS_FATAL_IF(getFrameSize() == 0);
    LOG_ALWAYS_FATAL_IF(frames > numeric_limits<size_t>::max() / getFrameSize());
    return frames * getFrameSize();
}

size_t CSampleSpec::convertFramesToUsec(uint32_t uiFrames) const
{
    LOG_ALWAYS_FATAL_IF(getSampleRate() == 0);
    LOG_ALWAYS_FATAL_IF((uiFrames / getSampleRate()) >
                        (numeric_limits<size_t>::max() / USEC_PER_SEC));
    return (USEC_PER_SEC * static_cast<uint64_t>(uiFrames)) / getSampleRate();
}

size_t CSampleSpec::convertUsecToframes(uint32_t uiIntervalUsec) const
{
    return (uint64_t)uiIntervalUsec * getSampleRate() / USEC_PER_SEC;
}

bool CSampleSpec::isSampleSpecItemEqual(SampleSpecItem eSampleSpecItem,
                                        const CSampleSpec& ssSrc,
                                        const CSampleSpec& ssDst)
{
    if (ssSrc.getSampleSpecItem(eSampleSpecItem) != ssDst.getSampleSpecItem(eSampleSpecItem)) {

        return false;
    }

    return ((eSampleSpecItem != EChannelCountSampleSpecItem) ||
            ssSrc.getChannelsPolicy() == ssDst.getChannelsPolicy());
}

}; // namespace android

