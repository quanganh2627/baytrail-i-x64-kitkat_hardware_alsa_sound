#ifndef ANDROID_AUDIO_RESAMPLER_ALSA_H
#define ANDROID_AUDIO_RESAMPLER_ALSA_H

#include <stdint.h>
#include <sys/types.h>

namespace android_audio_legacy {

// ----------------------------------------------------------------------------

#ifndef AUDIOHAL_ALSA_DEFAULT_SAMPLE_RATE
#define AUDIOHAL_ALSA_DEFAULT_SAMPLE_RATE (44100)
#endif

#define __DEFAULT_IN_RATE AUDIOHAL_ALSA_DEFAULT_SAMPLE_RATE
#define __DEFAULT_OUT_RATE (48000)
#define __DEFAULT_CHANNELS (2)

class AudioResamplerALSA {
public:
    AudioResamplerALSA(int32_t outSampleRate = __DEFAULT_OUT_RATE,
      int32_t inSampleRate = __DEFAULT_IN_RATE, int channelCount = __DEFAULT_CHANNELS);
    ~AudioResamplerALSA();

    void resample(void** out, int32_t* outBytes, const void* in, int32_t inBytes);
    bool setSampleRate(int32_t inSampleRate, int32_t outSampleRate);

private:
    friend class AudioStreamOutALSA;
    // forbid copy
    AudioResamplerALSA(const AudioResamplerALSA &);
    AudioResamplerALSA& operator =(const AudioResamplerALSA &);

    void allocateBuffer();

    void convert_short_2_float(int16_t *inp, float * out, size_t sz) const;
    void convert_float_2_short(float *inp, int16_t * out, size_t sz) const;

    int32_t mOutSampleRate;
    int32_t mInSampleRate;
    int32_t mChannelCount;

    static const int BUF_SIZE = (1 << 13);
    size_t  mMaxFrameCnt;  /* max frame count the buffer can store */
    void*   mContext;      /* handle used to do resample */
    float*  mFloatInp;     /* here sample size is 4 bytes */
    float*  mFloatOut;     /* here sample size is 4 bytes */
    short*  mResult;       /* here sample size is 2 bytes */
};

// ----------------------------------------------------------------------------
}; // namespace android

#endif /*ANDROID_AUDIO_RESAMPLER_ALSA_H*/
