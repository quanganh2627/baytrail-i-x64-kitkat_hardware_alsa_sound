#pragma once

#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <utils/Errors.h>
#include <assert.h>
#include <tinyalsa/asoundlib.h>


using namespace android;

namespace android_audio_legacy {

// ----------------------------------------------------------------------------


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

    // Specific Accessors
    void setChannelCount(uint32_t uiChannelCount) { setSampleSpecItem(EChannelCountSampleSpecItem, uiChannelCount); }
    uint32_t getChannelCount() const { return getSampleSpecItem(EChannelCountSampleSpecItem); }
    void setSampleRate(uint32_t uiSampleRate) { setSampleSpecItem(ERateSampleSpecItem, uiSampleRate); }
    uint32_t getSampleRate() const { return getSampleSpecItem(ERateSampleSpecItem); }
    void setFormat(uint32_t uiFormat) { setSampleSpecItem(EFormatSampleSpecItem, uiFormat); }
    uint32_t getFormat() const { return getSampleSpecItem(EFormatSampleSpecItem); }
    void setChannelMask(uint32_t uiChannelMask) { _uiChannelMask = uiChannelMask; }
    uint32_t getChannelMask() const { return _uiChannelMask; }

    // Generic Accessor
    void setSampleSpecItem(SampleSpecItem eSampleSpecItem, uint32_t uiValue);

    uint32_t getSampleSpecItem(SampleSpecItem eSampleSpecItem) const;

    ssize_t getFrameSize() const;

    ssize_t convertBytesToFrames(ssize_t bytes) const;

    ssize_t convertFramesToBytes(ssize_t frames) const;

    float convertFramesToMs(uint32_t uiFrames) const;

    ssize_t convertUsecToframes(uint32_t uiIntervalUsec) const;

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

// ----------------------------------------------------------------------------
}; // namespace android

