#include "SampleSpec.h"
#include "AudioUtils.h"

#include <system/audio.h>
#include <utils/Log.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

using namespace android;
using namespace std;

namespace android_audio_legacy {

// ----------------------------------------------------------------------------

float CSampleSpec::framesToMs(uint32_t uiFrames) const
{
    return 1000.0*(float)(uiFrames) / (getChannelCount()*getSampleRate());
}

// Generic Accessor
void CSampleSpec::setSampleSpecItem(SampleSpecItem eSampleSpecItem, uint32_t uiValue)
{
    assert(eSampleSpecItem < ENbSampleSpecItems);
    _auiSampleSpec[eSampleSpecItem] = uiValue;
}

uint32_t CSampleSpec::getSampleSpecItem(SampleSpecItem eSampleSpecItem) const
{
    assert(eSampleSpecItem < ENbSampleSpecItems);
    return _auiSampleSpec[eSampleSpecItem];
}

ssize_t CSampleSpec::getFrameSize() const
{
    return CAudioUtils::formatSize(getFormat()) * getChannelCount();
}

ssize_t CSampleSpec::convertBytesToFrames(ssize_t bytes) const
{
    assert(getFrameSize());
    return bytes / getFrameSize();
}

ssize_t CSampleSpec::convertFramesToBytes(ssize_t frames) const
{
    return frames * getFrameSize();
}

// ----------------------------------------------------------------------------
}; // namespace android

