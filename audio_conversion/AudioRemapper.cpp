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

#define LOG_TAG "AudioRemapper"

#include "AudioRemapper.h"
#include <cutils/log.h>

#define base AudioConverter

using namespace android;

namespace android_audio_legacy{


AudioRemapper::AudioRemapper(SampleSpecItem sampleSpecItem) :
    base(sampleSpecItem)
{
}

status_t AudioRemapper::configure(const SampleSpec &ssSrc, const SampleSpec &ssDst)
{
    status_t ret = base::configure(ssSrc, ssDst);
    if (ret != NO_ERROR) {

        return ret;
    }

    switch (ssSrc.getFormat()) {

    case AUDIO_FORMAT_PCM_16_BIT:

        if (ssSrc.isMono() && ssDst.isStereo()) {

            _convertSamplesFct =
                    static_cast<SampleConverter>(&AudioRemapper::convertMonoToStereoInS16);
        } else if (ssSrc.isStereo() && ssDst.isMono()) {

            _convertSamplesFct =
                    static_cast<SampleConverter>(&AudioRemapper::convertStereoToMonoInS16);
        } else if (ssSrc.isStereo() && ssDst.isStereo()) {

            // Iso channel, checks the channels policy
            if (!SampleSpec::isSampleSpecItemEqual(ChannelCountSampleSpecItem, ssSrc, ssDst)) {

                _convertSamplesFct =
                     static_cast<SampleConverter>(&AudioRemapper::convertChannelsPolicyInStereoS16);
            }
        } else {

            ret = INVALID_OPERATION;
        }
        break;

    case AUDIO_FORMAT_PCM_8_24_BIT:

        if (ssSrc.isMono() && ssDst.isStereo()) {

            _convertSamplesFct =
                    static_cast<SampleConverter>(&AudioRemapper::convertMonoToStereoInS24o32);
        } else if (ssSrc.isStereo() && ssDst.isMono()) {

            _convertSamplesFct =
                    static_cast<SampleConverter>(&AudioRemapper::convertStereoToMonoInS24o32);
        } else if (ssSrc.isStereo() && ssDst.isStereo()) {

            // Iso channel, checks the channels policy
            if (!SampleSpec::isSampleSpecItemEqual(ChannelCountSampleSpecItem, ssSrc, ssDst)) {

                _convertSamplesFct =
                  static_cast<SampleConverter>(&AudioRemapper::convertChannelsPolicyInStereoS24o32);
            }
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

status_t AudioRemapper::convertStereoToMonoInS16(const void *src,
                                                 void *dst,
                                                 const uint32_t inFrames,
                                                 uint32_t *outFrames)
{
    const int16_t *src16 = static_cast<const int16_t *>(src);
    int16_t *dst16 = static_cast<int16_t *>(dst);
    uint32_t srcChannels = _ssSrc.getChannelCount();
    size_t frames;

    for (frames = 0; frames < inFrames; frames++) {

        dst16[frames] = getAveragedSrcFrameInS16(&src16[srcChannels * frames]);
    }
    // Transformation is "iso" frames
    *outFrames = inFrames;
    return NO_ERROR;
}


status_t AudioRemapper::convertMonoToStereoInS16(const void *src,
                                                 void *dst,
                                                 const uint32_t inFrames,
                                                 uint32_t *outFrames)
{
    const int16_t *src16 = static_cast<const int16_t *>(src);
    int16_t *dst16 = static_cast<int16_t *>(dst);
    size_t frames = 0;
    uint32_t dstChannels = _ssDst.getChannelCount();

    for (frames = 0; frames < inFrames; frames++) {

        uint32_t channels;
        for (channels = 0; channels < dstChannels; channels++) {

            if (_ssDst.getChannelsPolicy(channels) != SampleSpec::Ignore) {

                dst16[dstChannels * frames + channels] = src16[frames];
            }
        }
    }

    // Transformation is "iso" frames
    *outFrames = inFrames;
    return NO_ERROR;
}

status_t AudioRemapper::convertChannelsPolicyInStereoS16(const void *src,
                                                         void *dst,
                                                         const uint32_t inFrames,
                                                         uint32_t *outFrames)
{
    const int16_t *src16 = static_cast<const int16_t *>(src);
    uint32_t frames = 0;
    uint32_t srcChannels = _ssSrc.getChannelCount();

    struct StereoS16 {
        int16_t left;
        int16_t right;
    } *dst16 = static_cast<StereoS16 *>(dst);

    for (frames = 0; frames < inFrames; frames++) {

        dst16[frames].left = convertSampleInS16(&src16[srcChannels * frames], Left);
        dst16[frames].right = convertSampleInS16(&src16[srcChannels * frames], Right);
    }
    // Transformation is "iso" frames
    *outFrames = inFrames;
    return NO_ERROR;
}

int16_t AudioRemapper::convertSampleInS16(const int16_t *src16, Channel channel) const
{
    SampleSpec::ChannelsPolicy dstPolicy = _ssDst.getChannelsPolicy(channel);

    if (dstPolicy == SampleSpec::Ignore) {

        // Destination policy is Ignore, so set to null dest sample
        return 0;

    } else if (dstPolicy == SampleSpec::Average) {

        // Destination policy is average, so average on all channels of the source frame
        return getAveragedSrcFrameInS16(src16);

    }
    // Destination policy is Copy
    // so copy only if source channel policy is not ignore
    if (_ssSrc.getChannelsPolicy(channel) != SampleSpec::Ignore) {

        return src16[channel];
    }
    // Even if policy is Copy, if the source channel is Ignore,
    // take the average of the other source channels
    return getAveragedSrcFrameInS16(src16);
}

int16_t AudioRemapper::getAveragedSrcFrameInS16(const int16_t *src16) const
{
    uint32_t validSrcChannels = 0;
    int32_t dst = 0;

    // Loops on source channels, checks upon the channel policy to take it into account
    // or not.
    // Average on all valid source channels
    for (uint32_t iSrcChannels = 0; iSrcChannels < _ssSrc.getChannelCount(); iSrcChannels++) {

        if (_ssSrc.getChannelsPolicy(iSrcChannels) != SampleSpec::Ignore) {

            dst += src16[iSrcChannels];
            validSrcChannels += 1;
        }
    }
    if (validSrcChannels) {

        dst = dst / validSrcChannels;
    }
    return dst;
}

status_t AudioRemapper::convertStereoToMonoInS24o32(const void *src,
                                                    void *dst,
                                                    const uint32_t inFrames,
                                                    uint32_t *outFrames)
{
    const uint32_t *src32 = static_cast<const uint32_t *>(src);
    uint32_t *dst32 = static_cast<uint32_t *>(dst);
    uint32_t srcChannels = _ssSrc.getChannelCount();
    size_t frames;

    for (frames = 0; frames < inFrames; frames++) {

        dst32[frames] = getAveragedSrcSampleInS32(&src32[srcChannels * frames]);
    }
    // Transformation is "iso" frames
    *outFrames = inFrames;

    return NO_ERROR;
}

status_t AudioRemapper::convertMonoToStereoInS24o32(const void *src,
                                                    void *dst,
                                                    const uint32_t inFrames,
                                                    uint32_t *outFrames)
{
    const uint32_t *src32 = static_cast<const uint32_t *>(src);
    uint32_t *dst32 = static_cast<uint32_t *>(dst);
    size_t frames = 0;
    uint32_t dstChannels = _ssDst.getChannelCount();

    for (frames = 0; frames < inFrames; frames++) {

        uint32_t channels;
        for (channels = 0; channels < _ssDst.getChannelCount(); channels++) {

            if (_ssDst.getChannelsPolicy(channels) != SampleSpec::Ignore) {

                dst32[dstChannels * frames + channels] = src32[frames];
            }
        }
    }
    // Transformation is "iso" frames
    *outFrames = inFrames;

    return NO_ERROR;
}

status_t AudioRemapper::convertChannelsPolicyInStereoS24o32(const void *src,
                                                            void *dst,
                                                            const uint32_t inFrames,
                                                            uint32_t *outFrames)
{
    const uint32_t *src32 = static_cast<const uint32_t *>(src);
    uint32_t frames = 0;
    uint32_t srcChannels = _ssSrc.getChannelCount();

    struct StereoS24o32 {
        uint32_t left;
        uint32_t right;
    } *dst32 = static_cast<StereoS24o32 *>(dst);

    for (frames = 0; frames < inFrames; frames++) {

        dst32[frames].left = convertSampleInS32(&src32[srcChannels * frames], Left);
        dst32[frames].right = convertSampleInS32(&src32[srcChannels * frames], Right);
    }
    // Transformation is "iso" frames
    *outFrames = inFrames;

    return NO_ERROR;
}

int32_t AudioRemapper::convertSampleInS32(const uint32_t *src32, Channel channel) const
{
    SampleSpec::ChannelsPolicy dstPolicy = _ssDst.getChannelsPolicy(channel);

    if (dstPolicy == SampleSpec::Ignore) {

        return 0;

    } else if (dstPolicy == SampleSpec::Average) {

        return getAveragedSrcSampleInS32(src32);
    }
    // Policy is Copy
    // Copy only if source channel policy is not Ignore
    if (_ssSrc.getChannelsPolicy(channel) != SampleSpec::Ignore) {

        return src32[channel];
    }
    // Source channel policy is Ignore, so provide the average of all the
    // other channels of the source frame.
    return getAveragedSrcSampleInS32(src32);
}

int32_t AudioRemapper::getAveragedSrcSampleInS32(const uint32_t *src32) const
{
    uint32_t validSrcChannels = 0;
    uint64_t dst = 0;
    //
    // Loops on source channels, checks upon the channel policy to take it into account
    // or not.
    // Average on all valid source channels
    //
    for (uint32_t iSrcChannels = 0; iSrcChannels < _ssSrc.getChannelCount(); iSrcChannels++) {

        if (_ssSrc.getChannelsPolicy(iSrcChannels) != SampleSpec::Ignore) {

            dst += src32[iSrcChannels];
             validSrcChannels += 1;
        }
    }
    if ( validSrcChannels) {

        dst = dst /  validSrcChannels;
    }
    return dst;
}

}; // namespace android
