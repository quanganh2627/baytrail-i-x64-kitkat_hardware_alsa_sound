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

#define LOG_TAG "AudioConverter"

#include <cutils/log.h>
#include "AudioUtils.h"
#include "AudioConverter.h"

using namespace android;

namespace android_audio_legacy{

CAudioConverter::CAudioConverter(SampleSpecItem eSampleSpecItem) :
    _pfnConvertSamples(NULL),
    _ssSrc(),
    _ssDst(),
    _pConvertBuf(NULL),
    _pConvertBufSize(0),
    _eSampleSpecItem(eSampleSpecItem)
{
}

CAudioConverter::~CAudioConverter()
{
    delete []_pConvertBuf;
}

//
// This function gets an output buffer suitable
// to convert inFrames input frames
//
void* CAudioConverter::getOutputBuffer(ssize_t inFrames)
{
    status_t ret = NO_ERROR;
    size_t outBufSizeInBytes = _ssDst.convertFramesToBytes(convertSrcToDstInFrames(inFrames));

    if (outBufSizeInBytes > _pConvertBufSize) {

        ret = allocateConvertBuffer(outBufSizeInBytes);
        if (ret != NO_ERROR) {

            LOGE("%s: could not allocate memory for operation", __FUNCTION__);
            return NULL;
        }
    }
    return (void* )_pConvertBuf;
}

status_t CAudioConverter::allocateConvertBuffer(ssize_t bytes)
{
    status_t ret = NO_ERROR;
    // Allocate one more frame for resampler
    _pConvertBufSize = bytes + (audio_bytes_per_sample(_ssDst.getFormat()) * _ssDst.getChannelCount());

    delete []_pConvertBuf;
    _pConvertBuf = NULL;

    _pConvertBuf = new char[_pConvertBufSize];

    if (!_pConvertBuf) {

        LOGE("cannot allocate resampler tmp buffers.\n");
        ret = NO_MEMORY;
    }
    return ret;
}

status_t CAudioConverter::doConfigure(const CSampleSpec& ssSrc, const CSampleSpec& ssDst)
{
    _ssSrc = ssSrc;
    _ssDst = ssDst;

    for (int i = 0; i < ENbSampleSpecItems; i++) {

        if (i == _eSampleSpecItem) {

            if (ssSrc.getSampleSpecItem((SampleSpecItem)i) == ssDst.getSampleSpecItem((SampleSpecItem)i)) {

                // The Sample spec items on which the converter is working
                // are the same...
                return INVALID_OPERATION;
            }
            continue;
        }
        if (ssSrc.getSampleSpecItem((SampleSpecItem)i) != ssDst.getSampleSpecItem((SampleSpecItem)i)) {

            // The Sample spec items on which the converter is NOT working
            // MUST BE the same...
            LOGE("%s: not supported", __FUNCTION__);
            return INVALID_OPERATION;
        }
    }

    // Reset the convert function pointer
    _pfnConvertSamples = NULL;

    // force the size to 0 to clear the buffer
    _pConvertBufSize = 0;

    return NO_ERROR;
}

status_t CAudioConverter::convert(const void* src, void** dst, const uint32_t inFrames, uint32_t* outFrames)
{
    void* outBuf;
    status_t ret = NO_ERROR;

    // output buffer might be provided by the caller
    outBuf = *dst != NULL ? *dst : getOutputBuffer(inFrames);
    if (!outBuf) {

        return NO_MEMORY;
    }
    if (_pfnConvertSamples != NULL) {

        ret = (this->*_pfnConvertSamples)(src, outBuf, inFrames, outFrames);
    }
    *dst = outBuf;

    return ret;
}

size_t CAudioConverter::convertSrcToDstInFrames(ssize_t frames) const
{
    return CAudioUtils::convertSrcToDstInFrames(frames, _ssSrc, _ssDst);
}

size_t CAudioConverter::convertSrcFromDstInFrames(ssize_t frames) const
{
    return CAudioUtils::convertSrcToDstInFrames(frames, _ssDst, _ssSrc);
}

}; // namespace android
