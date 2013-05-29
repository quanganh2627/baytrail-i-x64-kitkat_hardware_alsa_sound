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
        } else if (ssSrc.isStereo() && ssDst.isStereo()) {

            // Iso channel, checks the channels policy
            if (!CSampleSpec::isSampleSpecItemEqual(EChannelCountSampleSpecItem, ssSrc, ssDst)) {

                _pfnConvertSamples = (SampleConverter)(&CAudioRemapper::convertChannelsPolicyInStereoS16);
            }
        } else {

            ret = INVALID_OPERATION;
        }
        break;

    case AUDIO_FORMAT_PCM_8_24_BIT:

        if (ssSrc.isMono() && ssDst.isStereo()) {

            _pfnConvertSamples = (SampleConverter)(&CAudioRemapper::convertMonoToStereoInS24o32);
        } else if (ssSrc.isStereo() && ssDst.isMono()) {

            _pfnConvertSamples = (SampleConverter)(&CAudioRemapper::convertStereoToMonoInS24o32);
        } else if (ssSrc.isStereo() && ssDst.isStereo()) {

            // Iso channel, checks the channels policy
            if (!CSampleSpec::isSampleSpecItemEqual(EChannelCountSampleSpecItem, ssSrc, ssDst)) {

                _pfnConvertSamples = (SampleConverter)(&CAudioRemapper::convertChannelsPolicyInStereoS24o32);
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

status_t CAudioRemapper::convertStereoToMonoInS16(const void* src,
                                                  void* dst,
                                                  const uint32_t inFrames,
                                                  uint32_t* outFrames)
{
    const int16_t* src16 = static_cast<const int16_t *>(src);
    int16_t* dst16 = static_cast<int16_t *>(dst);
    uint32_t uiSrcChannels = _ssSrc.getChannelCount();
    size_t frames;

    for (frames = 0; frames < inFrames; frames++) {

        dst16[frames] = getAveragedSrcFrameInS16(&src16[uiSrcChannels * frames]);
    }
    // Transformation is "iso"frames
    *outFrames = inFrames;
    return NO_ERROR;
}


status_t CAudioRemapper::convertMonoToStereoInS16(const void* src,
                                                  void* dst,
                                                  const uint32_t inFrames,
                                                  uint32_t* outFrames)
{
    const int16_t* src16 = static_cast<const int16_t *>(src);
    int16_t* dst16 = static_cast<int16_t *>(dst);
    size_t frames = 0;
    uint32_t uiDstChannels = _ssDst.getChannelCount();

    for (frames = 0; frames < inFrames; frames++) {

        uint32_t uiChannels;
        for (uiChannels = 0; uiChannels < uiDstChannels; uiChannels++) {

            if (_ssDst.getChannelsPolicy(uiChannels) != CSampleSpec::EIgnore) {

                dst16[uiDstChannels * frames + uiChannels] = src16[frames];
            }
        }
    }

    // Transformation is "iso"frames
    *outFrames = inFrames;
    return NO_ERROR;
}

status_t CAudioRemapper::convertChannelsPolicyInStereoS16(const void* src,
                                                          void* dst,
                                                          const uint32_t inFrames,
                                                          uint32_t *outFrames)
{
    const int16_t* src16 = static_cast<const int16_t *>(src);
    uint32_t frames = 0;
    uint32_t uiSrcChannels = _ssSrc.getChannelCount();

    struct StereoS16 {
        int16_t left;
        int16_t right;
    } *dst16 = static_cast<StereoS16 *>(dst);

    for (frames = 0; frames < inFrames; frames++) {

        dst16[frames].left = convertSampleInS16(&src16[uiSrcChannels * frames], ELeft);
        dst16[frames].right = convertSampleInS16(&src16[uiSrcChannels * frames], ERight);
    }
    // Transformation is "iso"frames
    *outFrames = inFrames;
    return NO_ERROR;
}

int16_t CAudioRemapper::convertSampleInS16(const int16_t* src16, Channel eChannel) const
{
    CSampleSpec::ChannelsPolicy dstPolicy = _ssDst.getChannelsPolicy(eChannel);

    if (dstPolicy == CSampleSpec::EIgnore) {

        // Destination policy is ignore, so set to null dest sample
        return 0;

    } else if (dstPolicy == CSampleSpec::EAverage) {

        // Destination policy is average, so average on all channels of the source frame
        return getAveragedSrcFrameInS16(src16);

    }
    // Destination policy is Copy
    // so copy only if source channel policy is not ignore
    if (_ssSrc.getChannelsPolicy(eChannel) != CSampleSpec::EIgnore) {

        return src16[eChannel];
    }
    // Even if policy is copy, if the source channel is ignore,
    // take the average of the other source channels
    return getAveragedSrcFrameInS16(src16);
}

int16_t CAudioRemapper::getAveragedSrcFrameInS16(const int16_t* src16) const
{
    uint32_t uiValidSrcChannels = 0;
    int32_t iDst = 0;

    // Loops on source channels, checks upon the channel policy to take it into account
    // or not.
    // Average on all valid source channels
    for (uint32_t iSrcChannels = 0; iSrcChannels < _ssSrc.getChannelCount(); iSrcChannels++) {

        if (_ssSrc.getChannelsPolicy(iSrcChannels) != CSampleSpec::EIgnore) {

            iDst += src16[iSrcChannels];
            uiValidSrcChannels += 1;
        }
    }
    if (uiValidSrcChannels) {

        iDst = iDst / uiValidSrcChannels;
    }
    return iDst;
}

status_t CAudioRemapper::convertStereoToMonoInS24o32(const void* src,
                                                     void* dst,
                                                     const uint32_t inFrames,
                                                     uint32_t* outFrames)
{
    const uint32_t* src32 = static_cast<const uint32_t *>(src);
    uint32_t* dst32 = static_cast<uint32_t *>(dst);
    uint32_t uiSrcChannels = _ssSrc.getChannelCount();
    size_t frames;

    for (frames = 0; frames < inFrames; frames++) {

        dst32[frames] = getAveragedSrcSampleInS32(&src32[uiSrcChannels * frames]);
    }
    // Transformation is "iso"frames
    *outFrames = inFrames;

    return NO_ERROR;
}

status_t CAudioRemapper::convertMonoToStereoInS24o32(const void* src,
                                                     void* dst,
                                                     const uint32_t inFrames,
                                                     uint32_t* outFrames)
{
    const uint32_t* src32 = static_cast<const uint32_t *>(src);
    uint32_t* dst32 = static_cast<uint32_t *>(dst);
    size_t frames = 0;
    uint32_t uiDstChannels = _ssDst.getChannelCount();

    for (frames = 0; frames < inFrames; frames++) {

        uint32_t uiChannels;
        for (uiChannels = 0; uiChannels < _ssDst.getChannelCount(); uiChannels++) {

            if (_ssDst.getChannelsPolicy(uiChannels) != CSampleSpec::EIgnore) {

                dst32[uiDstChannels * frames + uiChannels] = src32[frames];
            }
        }
    }
    // Transformation is "iso"frames
    *outFrames = inFrames;

    return NO_ERROR;
}

status_t CAudioRemapper::convertChannelsPolicyInStereoS24o32(const void* src,
                                                             void* dst,
                                                             const uint32_t inFrames,
                                                             uint32_t *outFrames)
{
    const uint32_t* src32 = static_cast<const uint32_t *>(src);
    uint32_t frames = 0;
    uint32_t uiSrcChannels = _ssSrc.getChannelCount();

    struct StereoS24o32 {
        uint32_t left;
        uint32_t right;
    } *dst32 = static_cast<StereoS24o32 *>(dst);

    for (frames = 0; frames < inFrames; frames++) {

        dst32[frames].left = convertSampleInS32(&src32[uiSrcChannels * frames], ELeft);
        dst32[frames].right = convertSampleInS32(&src32[uiSrcChannels * frames], ERight);
    }
    // Transformation is "iso"frames
    *outFrames = inFrames;

    return NO_ERROR;
}

int32_t CAudioRemapper::convertSampleInS32(const uint32_t* src32, Channel eChannel) const
{
    CSampleSpec::ChannelsPolicy dstPolicy = _ssDst.getChannelsPolicy(eChannel);

    if (dstPolicy == CSampleSpec::EIgnore) {

        return 0;

    } else if (dstPolicy == CSampleSpec::EAverage) {

        return getAveragedSrcSampleInS32(src32);
    }
    // Policy is Copy
    // Copy only if source channel policy is not ignore
    if (_ssSrc.getChannelsPolicy(eChannel) != CSampleSpec::EIgnore) {

        return src32[eChannel];
    }
    // Source channel policy is Ignore, so provide the average of all the
    // other channels of the source frame.
    return getAveragedSrcSampleInS32(src32);
}

int32_t CAudioRemapper::getAveragedSrcSampleInS32(const uint32_t* src32) const
{
    uint32_t uiValidSrcChannels = 0;
    uint64_t iDst = 0;
    //
    // Loops on source channels, checks upon the channel policy to take it into account
    // or not.
    // Average on all valid source channels
    //
    for (uint32_t iSrcChannels = 0; iSrcChannels < _ssSrc.getChannelCount(); iSrcChannels++) {

        if (_ssSrc.getChannelsPolicy(iSrcChannels) != CSampleSpec::EIgnore) {

            iDst += src32[iSrcChannels];
            uiValidSrcChannels += 1;
        }
    }
    if (uiValidSrcChannels) {

        iDst = iDst / uiValidSrcChannels;
    }
    return iDst;
}

}; // namespace android
