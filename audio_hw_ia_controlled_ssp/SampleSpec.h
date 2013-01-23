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
#pragma once

#include <string.h>
#include <tinyalsa/asoundlib.h>
#include <system/audio.h>

namespace android_audio_legacy {

//
// Do not change the order: convertion steps will be ordered to make
// the computing as light as possible, it will works successively
// on the channels, then the format and the rate
//
enum SampleSpecItem {
    EChannelCountSampleSpecItem = 0,
    EFormatSampleSpecItem,
    ERateSampleSpecItem,

    ENbSampleSpecItems
};


class CSampleSpec {

public:
    bool operator==(const CSampleSpec& right) const { return !memcmp(_auiSampleSpec, right._auiSampleSpec, sizeof(_auiSampleSpec)); }
    bool operator!=(const CSampleSpec& right) const { return memcmp(_auiSampleSpec, right._auiSampleSpec, sizeof(_auiSampleSpec)); }

    // Specific Accessors
    void setChannelCount(uint32_t uiChannelCount) { setSampleSpecItem(EChannelCountSampleSpecItem, uiChannelCount); }
    uint32_t getChannelCount() const { return getSampleSpecItem(EChannelCountSampleSpecItem); }
    void setSampleRate(uint32_t uiSampleRate) { setSampleSpecItem(ERateSampleSpecItem, uiSampleRate); }
    uint32_t getSampleRate() const { return getSampleSpecItem(ERateSampleSpecItem); }
    void setFormat(uint32_t uiFormat) { setSampleSpecItem(EFormatSampleSpecItem, uiFormat); }
    audio_format_t getFormat() const { return static_cast<audio_format_t>(getSampleSpecItem(EFormatSampleSpecItem)); }
    void setChannelMask(uint32_t uiChannelMask) { _uiChannelMask = uiChannelMask; }
    uint32_t getChannelMask() const { return _uiChannelMask; }

    // Generic Accessor
    void setSampleSpecItem(SampleSpecItem eSampleSpecItem, uint32_t uiValue);

    uint32_t getSampleSpecItem(SampleSpecItem eSampleSpecItem) const;

    size_t getFrameSize() const;

    size_t convertBytesToFrames(size_t bytes) const;

    size_t convertFramesToBytes(size_t frames) const;

    size_t convertFramesToUsec(uint32_t uiFrames) const;

    size_t convertUsecToframes(uint32_t uiIntervalUsec) const;

    bool isMono() const { return _auiSampleSpec[EChannelCountSampleSpecItem] == 1; }

    bool isStereo() const { return _auiSampleSpec[EChannelCountSampleSpecItem] == 2; }

private:
    // Attributes

    // Array of sample spec items:
    //  -channel numbers
    //  -format
    //  -sample rate
    uint32_t _auiSampleSpec[ENbSampleSpecItems];

    // Bit field that defines the channels used.
    // Refer to AudioSystemLegacy.h for the definition of the bit field
    // (enum audio_channels)
    uint32_t _uiChannelMask;

    static const uint32_t USEC_PER_SEC;
};

}; // namespace android

