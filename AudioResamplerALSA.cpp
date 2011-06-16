#define LOG_TAG "AudioResamplerALSA"
#define LOG_NDEBUG 0

#include <stdlib.h>
#include <string.h>
#include <cutils/log.h>

#include "AudioResamplerALSA.h"
#include <alsa/asoundlib.h>
#include <iasrc_resampler.h>

namespace android {

// ----------------------------------------------------------------------------

AudioResamplerALSA::AudioResamplerALSA(int32_t outSampleRate, int32_t inSampleRate,
    int32_t channelCount) : mOutSampleRate(outSampleRate),
    mInSampleRate(inSampleRate), mChannelCount(channelCount), mMaxFrameCnt(0),
    mContext(0), mFloatInp(0), mFloatOut(0), mResult(0)
{
    if (iaresamplib_supported_conversion(mInSampleRate, mOutSampleRate)) {
        iaresamplib_new(&mContext, mChannelCount, mInSampleRate, mOutSampleRate);
        if (!mContext) {
            LOGE("cannot create resampler handle for lacking of memory.\n");
        }
    }
}

AudioResamplerALSA::~AudioResamplerALSA()
{
    if (mContext) {
        iaresamplib_reset(mContext);
        iaresamplib_delete(&mContext);
        mContext = NULL;
    }

    if (mFloatInp) delete []mFloatInp;
    if (mFloatOut) delete []mFloatOut;
    if (mResult) delete []mResult;

    mFloatInp = mFloatOut = NULL;
    mResult = NULL;
}

void AudioResamplerALSA::allocateBuffer()
{
    if (mMaxFrameCnt == 0) {
        mMaxFrameCnt = BUF_SIZE;
    } else {
        mMaxFrameCnt *= 2; // simply double the buf size
    }

    if (mFloatInp) delete []mFloatInp;
    if (mFloatOut) delete []mFloatOut;
    if (mResult) delete []mResult;

    mFloatInp = mFloatOut = NULL;
    mResult = NULL;

    mFloatInp = new float[(mMaxFrameCnt + 1) * mChannelCount];
    mFloatOut = new float[(mMaxFrameCnt + 1) * mChannelCount];
    mResult = new short[(mMaxFrameCnt +1 ) * mChannelCount];

    if (!mFloatInp || !mFloatOut || !mResult) {
        LOGE("cannot allocate resampler tmp buffers.\n");
    }
}

bool AudioResamplerALSA::setSampleRate(int32_t inSampleRate, int32_t outSampleRate)
{
    if (inSampleRate == mInSampleRate &&
        outSampleRate == mOutSampleRate && mContext) {
        return true;
    }

    if (mContext) {
        iaresamplib_reset(mContext);
        iaresamplib_delete(&mContext);
        mContext = NULL;
    }

    mInSampleRate = inSampleRate;
    mOutSampleRate = outSampleRate;

    if (iaresamplib_supported_conversion(mInSampleRate, mOutSampleRate)) {
        iaresamplib_new(&mContext, mChannelCount, mInSampleRate, mOutSampleRate);
        if (!mContext) {
            LOGE("cannot create resampler handle for lacking of memory.\n");
            return false;
        }
        return true;
    }

    LOGE("SRC lib doesn't support this conversion.\n");
    return false;
}

void AudioResamplerALSA::convert_short_2_float(int16_t *inp, float * out, size_t sz) const
{
    size_t i;
    for (i = 0; i < sz; i++) {
        *out++ = (float) *inp++;
    }
}

void AudioResamplerALSA::convert_float_2_short(float *inp, int16_t * out, size_t sz) const
{
    size_t i;
    for (i = 0; i < sz; i++) {
        if (*inp > SHRT_MAX) {
            *inp = SHRT_MAX;
        } else if (*inp < SHRT_MIN) {
            *inp = SHRT_MIN;
        }
        *out++ = (short) *inp++;
    }
}

void AudioResamplerALSA::resample(void** out, int32_t* outBytes, const void* in, int32_t inBytes)
{
    int sample_size = 2; /* S16_LE */
    int bytes_per_frame = mChannelCount * sample_size;
    unsigned int inFrameCount = inBytes / bytes_per_frame;
    size_t outFrameCount = (inFrameCount * mOutSampleRate) / mInSampleRate;

    while (outFrameCount > mMaxFrameCnt) allocateBuffer();

    unsigned int out_n_frames;
    convert_short_2_float((short*)in, mFloatInp, inFrameCount * mChannelCount);
    iaresamplib_process_float(mContext, mFloatInp, inFrameCount, mFloatOut, &out_n_frames);
    convert_float_2_short(mFloatOut, mResult, out_n_frames * mChannelCount);

    *outBytes = out_n_frames * bytes_per_frame;
    *out = mResult;
}

// ----------------------------------------------------------------------------
}; // namespace android
