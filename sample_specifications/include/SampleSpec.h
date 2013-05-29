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
#include <vector>

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
    bool operator==(const CSampleSpec& right) const {

        return !memcmp(_auiSampleSpec, right._auiSampleSpec, sizeof(_auiSampleSpec)) &&
                (_aChannelsPolicy == right._aChannelsPolicy);
    }

    bool operator!=(const CSampleSpec& right) const {

        return !operator==(right);
    }

    /**
     * Channel policy definition.
     * The channel policy will be usefull in case of remap operation.
     * From this definition, the remapper must be able to infer conversion table.
     *
     * For example: on some stereo devices, a channel might be empty/invalid.
     * So, the other channel will be tagged as "average"
     *
     *      SOURCE              DESTINATION
     *  channel 1 (ECopy) ---> channel 1 (EAverage) = (source channel 1 + source channel 2) / 2
     *  channel 2 (ECopy) ---> channel 2 (EIgnore)  = empty
     *
     */
    enum ChannelsPolicy {
        ECopy = 0,      /**< This channel is valid. */
        EAverage,       /**< This channel contains all valid audio data. */
        EIgnore,        /**< This channel does not contains any valid audio data. */

        ENbChannelsPolicy
    };

    CSampleSpec(uint32_t channel = _defaultChannels,
                uint32_t format = _defaultFormat,
                uint32_t rate = _defaultRate);

    CSampleSpec(uint32_t channel,
               uint32_t format,
               uint32_t rate,
               const std::vector<ChannelsPolicy> &channelsPolicy);

    // Specific Accessors
    void setChannelCount(uint32_t uiChannelCount) {

        setSampleSpecItem(EChannelCountSampleSpecItem, uiChannelCount);
    }
    uint32_t getChannelCount() const { return getSampleSpecItem(EChannelCountSampleSpecItem); }
    void setSampleRate(uint32_t uiSampleRate) {

        setSampleSpecItem(ERateSampleSpecItem, uiSampleRate);
    }
    uint32_t getSampleRate() const { return getSampleSpecItem(ERateSampleSpecItem); }
    void setFormat(uint32_t uiFormat) { setSampleSpecItem(EFormatSampleSpecItem, uiFormat); }
    audio_format_t getFormat() const {

        return static_cast<audio_format_t>(getSampleSpecItem(EFormatSampleSpecItem));
    }
    void setChannelMask(uint32_t uiChannelMask) { _uiChannelMask = uiChannelMask; }
    uint32_t getChannelMask() const { return _uiChannelMask; }

    void setChannelsPolicy(const std::vector<ChannelsPolicy>& channelsPolicy);
    const std::vector<ChannelsPolicy>& getChannelsPolicy() const { return _aChannelsPolicy; }
    ChannelsPolicy getChannelsPolicy(uint32_t uiChannelIndex) const;

    // Generic Accessor
    void setSampleSpecItem(SampleSpecItem eSampleSpecItem, uint32_t uiValue);

    uint32_t getSampleSpecItem(SampleSpecItem eSampleSpecItem) const;

    size_t getFrameSize() const;

    /**
     * Converts the bytes number to frames number.
     * It converts a bytes number into a frame number it represents in this sample spec instance.
     * It asserts if the frame size is null.

     * @param[in] number of bytes to be translated in frames.
     *
     * @return frames number in the sample spec given in param for this instance.
     */
    size_t convertBytesToFrames(size_t bytes) const;

    /**
     * Converts the frames number to bytes number.
     * It converts the frame number (independant of the channel number and format number) into
     * a byte number (sample spec dependant). In case of overflow, this function will assert.

     * @param[in] number of frames to be translated in bytes.
     *
     * @return bytes number in the sample spec given in param for this instance
     */
    size_t convertFramesToBytes(size_t frames) const;

    /**
     * Translates the frame number into time.
     * It converts the frame number into the time it represents in this sample spec instance.
     * It may assert if overflow is detected.
     *
     * @param[in] number of frames to be translated in time.
     *
     * @return time in microseconds.
     */
    size_t convertFramesToUsec(uint32_t uiFrames) const;

    /**
     * Translates a time period into a frame number.
     * It converts a period of time into a frame number it represents in this sample spec instance.
     *
     * @param[in] time interval in micro second to be translated.
     *
     * @return number of frames corresponding to the period of time.
     */
    size_t convertUsecToframes(uint32_t uiIntervalUsec) const;

    bool isMono() const { return _auiSampleSpec[EChannelCountSampleSpecItem] == 1; }

    bool isStereo() const { return _auiSampleSpec[EChannelCountSampleSpecItem] == 2; }

    /**
     * Checks upon equality of a sample spec item.
     *  For channels, it checks:
     *          -not only that channels count is equal
     *          -but also the channels policy of source and destination is the same.
     * @param[in] eSampleSpecItem item to checks.
     * @param[in] ssSrc source sample specifications.
     * @param[in] ssDst destination sample specifications.
     *
     * @return true upon equality, false otherwise.
     */
    static bool isSampleSpecItemEqual(SampleSpecItem eSampleSpecItem,
                                      const CSampleSpec& ssSrc,
                                      const CSampleSpec& ssDst);

private:
    /**
     * Initialise the sample specifications.
     * Parts of the private constructor. It sets the basic fields, reset the channel mask to 0,
     * and sets the channel policy to "Copy" for each of the channels used.
     *
     * @param[in] channel number of channels.
     * @param[in] format sample format, eg 16 or 24 bits(coded on 32 bits).
     * @param[in] rate sample rate.
     */
    void init(uint32_t channel, uint32_t format, uint32_t rate);

    uint32_t _auiSampleSpec[ENbSampleSpecItems]; /**< Array of sample spec items:
                                                        -channel number
                                                        -format
                                                        -sample rate. */

    uint32_t _uiChannelMask; /**< Bit field that defines the channels used. */

    static const uint32_t USEC_PER_SEC; /**< constant to convert sec to / from microseconds. */

    std::vector<ChannelsPolicy> _aChannelsPolicy; /**< channels policy array. */

    static const uint32_t MAX_CHANNELS = 32; /**< supports until 32 channels. */
    static const uint32_t _defaultChannels = 2; /**< default channel used is stereo. */
    static const uint32_t _defaultFormat = AUDIO_FORMAT_PCM_16_BIT; /**< default format is 16bits.*/
    static const uint32_t _defaultRate = 48000; /**< default rate is 48 kHz. */
};

}; // namespace android

