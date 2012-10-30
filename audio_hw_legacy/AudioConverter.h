/* AudioConverter.h
 **
 ** Copyright 2012 Intel Corporation
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

#include <stdint.h>
#include <sys/types.h>
#include <utils/Errors.h>

#include "AudioHardwareALSA.h"

using namespace android;

namespace android_audio_legacy {

// ----------------------------------------------------------------------------

class CAudioConverter {

public:

    typedef status_t (CAudioConverter::*ConvertSamples)(const void* src, void* dst, uint32_t inFrames, uint32_t *outFrames);

    CAudioConverter(SampleSpecItem eSampleSpecItem);
    virtual ~CAudioConverter();

    virtual status_t doConfigure(const CSampleSpec& ssSrc, const CSampleSpec& ssDst);

    virtual status_t convert(const void* src, void** dst, uint32_t inFrames, uint32_t *outFrames);

protected:

    size_t convertSrcFromDstInFrames(ssize_t frames) const;

    size_t convertSrcToDstInFrames(ssize_t frames) const;

    ConvertSamples _pfnConvertSamples;

    CSampleSpec   _ssSrc;
    CSampleSpec   _ssDst;

private:
    // forbid copy
    CAudioConverter(const CAudioConverter &);
    CAudioConverter& operator =(const CAudioConverter &);

    void* getOutputBuffer(ssize_t inFrames);
    status_t allocateConvertBuffer(ssize_t bytes);

    char*           _pConvertBuf;
    size_t          _pConvertBufSize;

    // Sample spec item on which the converter is working
    SampleSpecItem  _eSampleSpecItem;
};

// ----------------------------------------------------------------------------
}; // namespace android

