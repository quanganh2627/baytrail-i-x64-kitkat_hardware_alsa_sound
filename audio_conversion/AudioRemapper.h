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
#pragma once

#include "AudioConverter.h"

namespace android_audio_legacy {

class CAudioRemapper : public CAudioConverter {

    enum Channel {

        ELeft = 0,
        ERight
    };

public:
    /**
     * Constructor of the remapper.
     * @param[in] eSampleSpecItem Sample specification item on which this audio
     *             converter is working on.
     */
    CAudioRemapper(SampleSpecItem eSampleSpecItem);

private:
    /**
     * Configure the remapper.
     * Selects the appropriate remap operation to use according to the source
     * and destination sample specifications.
     *
     * @param[in] ssSrc Reference on the source sample specifications.
     * @param[in] ssDst Reference on the destination sample specifications.
     *
     * @return error code.
     */
    virtual android::status_t doConfigure(const CSampleSpec& ssSrc, const CSampleSpec& ssDst);

    /**
     * Remap from stereo to mono in S16.
     * Convert a stereo source into a mono destination in S16 format.
     *
     * @param[in] src the address of the source buffer.
     * @param[out] dst the address of the destination buffer, caller to ensure the destination
     *             is large enough.
     * @param[in] inFrames number of input frames.
     * @param[out] outFrames pointer on output frames processed.
     *
     * @return error code.
     */
    android::status_t convertStereoToMonoInS16(const void* src,
                                               void* dst,
                                               const uint32_t inFrames,
                                               uint32_t* outFrames);

    /**
     * Remap from mono to stereo in S16.
     * Convert a mono source into a stereo destination in S16 format.
     *
     * @param[in] src the address of the source buffer.
     * @param[out] dst the address of the destination buffer, caller to ensure the destination
     *             is large enough.
     * @param[in] inFrames number of input frames.
     * @param[out] outFrames pointer on output frames processed.
     *
     * @return error code.
     */
    android::status_t convertMonoToStereoInS16(const void* src,
                                               void* dst,
                                               const uint32_t inFrames,
                                               uint32_t* outFrames);

    /**
     * Remap channels policy in S16.
     * Convert a stereo source into a stereo destination in S16 format
     * with different channels policy.
     *
     * @param[in] src the address of the source buffer.
     * @param[out] dst the address of the destination buffer, caller to ensure the destination
     *             is large enough.
     * @param[in] inFrames number of input frames.
     * @param[out] outFrames pointer on output frames processed.
     *
     * @return error code.
     */
    android::status_t convertChannelsPolicyInStereoS16(const void* src,
                                                       void* dst,
                                                       const uint32_t inFrames,
                                                       uint32_t *outFrames);

    /**
     * Remap from stereo to mono in S24 over 32.
     * Convert a stereo source into a mono destination in S24 over 32 format.
     *
     * @param[in] src the address of the source buffer.
     * @param[out] dst the address of the destination buffer, caller to ensure the destination
     *             is large enough.
     * @param[in] inFrames number of input frames.
     * @param[out] outFrames pointer on output frames processed.
     *
     * @return error code.
     */
    android::status_t convertStereoToMonoInS24o32(const void* src,
                                                  void* dst,
                                                  const uint32_t inFrames,
                                                  uint32_t* outFrames);

    /**
     * Remap from mono to stereo in S24 over 32.
     * Convert a mono source into a stereo destination in S24 over 32 format.
     *
     * @param[in] src the address of the source buffer.
     * @param[out] dst the address of the destination buffer., caller to ensure the destination
     *             is large enough.
     * @param[in] inFrames number of input frames.
     * @param[out] outFrames pointer on output frames processed.
     *
     * @return error code.
     */
    android::status_t convertMonoToStereoInS24o32(const void *src,
                                                  void* dst,
                                                  const uint32_t inFrames,
                                                  uint32_t* outFrames);

    /**
     * Remap channels policy in S24 over 32.
     * Gets on destination channel from the source frame according to the destination
     * channel policy in S24 over 32 format.
     *
     * @param[in] src the address of the source buffer.
     * @param[out] dst the address of the destination buffer, caller to ensure the destination
     *             is large enough.
     * @param[in] iinFrames number of input frames.
     * @param[out] outFrames pointer on output frames processed.
     *
     * @return error code.
     */
    android::status_t convertChannelsPolicyInStereoS24o32(const void* src,
                                                          void* dst,
                                                          const uint32_t inFrames,
                                                          uint32_t *outFrames);

    /**
     * Convert a source sample in S16.
     * Gets destination channel from the source sample according to the destination
     * channel policy.
     *
     * @param[in] src16 the address of the source frame.
     * @param[in] eChannel the channel of the destination.
     *
     * @return destination channel sample.
     */
    int16_t convertSampleInS16(const int16_t* src, Channel eChannel) const;

    /**
     * Convert a source sample in S24 over 32.
     * Gets destination channel from the source sample according to the destination
     * channel policy.
     *
     * @param[in] src32 the address of the source frame.
     * @param[in] epolicy the policy of the destination channel.
     *
     * @return destination channel sample.
     */
    int32_t convertSampleInS32(const int32_t* src32, Channel eChannel) const;

    /**
     * Average source frame in S24 over 32.
     * Gets an averaged value of the source audio frame taking into
     * account the policy of the source channels.
     *
     * @param[in] src32 the address of the source frame.
     *
     * @return destination channel sample.
     */
    int32_t getAveragedSrcSampleInS32(const int32_t* src32) const;

    /**
     * Average source frame in S16.
     * Gets an averaged value of the source audio frame taking into
     * account the policy of the source channels.
     *
     * @param[in] src16 the address of the source frame.
     *
     * @return destination channel sample.
     */
    int16_t getAveragedSrcFrameInS16(const int16_t* src16) const;
};

}; // namespace android

