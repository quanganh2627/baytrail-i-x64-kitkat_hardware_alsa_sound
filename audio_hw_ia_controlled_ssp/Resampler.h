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

#include "AudioConverter.h"

namespace android_audio_legacy {

class CResampler : public CAudioConverter {

public:
    CResampler(SampleSpecItem eSampleSpecItem);

    virtual ~CResampler();

    android::status_t resampleFrames(const void* in, void* out, const uint32_t inFrames, uint32_t *outFrames);

    virtual android::status_t doConfigure(const CSampleSpec& ssSrc, const CSampleSpec& ssDst);

private:
    // forbid copy
    CResampler(const CResampler &);
    CResampler& operator =(const CResampler &);


    android::status_t allocateBuffer();

    void convert_short_2_float(int16_t *inp, float * out, size_t sz) const;

    void convert_float_2_short(float *inp, int16_t * out, size_t sz) const;

    static const int BUF_SIZE = (1 << 13);
    size_t  mMaxFrameCnt;  /* max frame count the buffer can store */
    void*   mContext;      /* handle used to do resample */
    float*  mFloatInp;     /* here sample size is 4 bytes */
    float*  mFloatOut;     /* here sample size is 4 bytes */
};

}; // namespace android
