#pragma once

#include <stdint.h>
#include <sys/types.h>
#include <utils/Errors.h>

#include "AudioConverter.h"

using namespace android;

namespace android_audio_legacy {

// ----------------------------------------------------------------------------

class CResampler : public CAudioConverter {

public:
    CResampler(SampleSpecItem eSampleSpecItem);

    virtual ~CResampler();

    status_t resampleFrames(const void* in, void* out, const uint32_t inFrames, uint32_t *outFrames);

    virtual status_t doConfigure(const CSampleSpec& ssSrc, const CSampleSpec& ssDst);

private:
    // forbid copy
    CResampler(const CResampler &);
    CResampler& operator =(const CResampler &);


    status_t allocateBuffer();

    void convert_short_2_float(int16_t *inp, float * out, size_t sz) const;

    void convert_float_2_short(float *inp, int16_t * out, size_t sz) const;

    static const int BUF_SIZE = (1 << 13);
    size_t  mMaxFrameCnt;  /* max frame count the buffer can store */
    void*   mContext;      /* handle used to do resample */
    float*  mFloatInp;     /* here sample size is 4 bytes */
    float*  mFloatOut;     /* here sample size is 4 bytes */
};

// ----------------------------------------------------------------------------
}; // namespace android
