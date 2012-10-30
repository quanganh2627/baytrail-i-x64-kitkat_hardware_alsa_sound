#pragma once

#include <stdint.h>
#include <sys/types.h>
#include <utils/Errors.h>

#include "AudioConverter.h"

using namespace android;

namespace android_audio_legacy {

// ----------------------------------------------------------------------------

class CAudioReformatter : public CAudioConverter {

public:
    CAudioReformatter(SampleSpecItem eSampleSpecItem);

private:
    virtual status_t doConfigure(const CSampleSpec& ssSrc, const CSampleSpec& ssDst);

    status_t convertS16toS24over32(const void* src, void* dst, const uint32_t inFrames, uint32_t *outFrames);

    status_t convertS24over32toS16(const void *src, void* dst, const uint32_t inFrames, uint32_t *outFrames);
};

// ----------------------------------------------------------------------------
}; // namespace android

