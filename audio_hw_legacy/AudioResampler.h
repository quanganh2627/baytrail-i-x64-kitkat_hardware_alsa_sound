#pragma once

#include <stdint.h>
#include <sys/types.h>
#include <utils/Errors.h>

#include "AudioConverter.h"

using namespace android;

namespace android_audio_legacy {

// ----------------------------------------------------------------------------

class CResampler;

class CAudioResampler : public CAudioConverter {

    typedef list<CResampler*>::iterator ResamplerListIterator;

public:
    CAudioResampler(SampleSpecItem eSampleSpecItem);

    virtual ~CAudioResampler();

private:
    // forbid copy
    CAudioResampler(const CAudioResampler &);
    CAudioResampler& operator =(const CAudioResampler &);

    status_t resampleFrames(const void* in, void* out, const uint32_t inFrames, uint32_t *outFrames);

    virtual status_t doConfigure(const CSampleSpec& ssSrc, const CSampleSpec& ssDst);

    virtual status_t convert(const void* src, void** dst, uint32_t inFrames, uint32_t *outFrames);

    CResampler* _pResampler;
    CResampler* _pPivotResampler;

    // List of audio converter enabled
    list<CResampler*>  _activeResamplerList;

    static const uint32_t _guiPivotSampleRate;
};

// ----------------------------------------------------------------------------
}; // namespace android
