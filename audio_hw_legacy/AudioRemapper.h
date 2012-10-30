#pragma once

#include <stdint.h>
#include <sys/types.h>
#include <utils/Errors.h>

#include "AudioConverter.h"

using namespace android;

namespace android_audio_legacy {

// ----------------------------------------------------------------------------

class CAudioRemapper : public CAudioConverter {

public:
    CAudioRemapper(SampleSpecItem eSampleSpecItem);

private:
    virtual status_t doConfigure(const CSampleSpec& ssSrc, const CSampleSpec& ssDst);

    status_t convertStereoToMonoInS16(const void* src, void* dst, const uint32_t inFrames, uint32_t *outFrames);

    status_t convertMonoToStereoInS16(const void* src, void* dst, const uint32_t inFrames, uint32_t *outFrames);

    status_t convertStereoToMonoInS24o32(const void* src, void* dst, const uint32_t inFrames, uint32_t *outFrames);

    status_t convertMonoToStereoInS24o32(const void *src, void* dst, const uint32_t inFrames, uint32_t *outFrames);
};

// ----------------------------------------------------------------------------
}; // namespace android

