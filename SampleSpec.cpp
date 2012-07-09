#include "SampleSpec.h"
#include <system/audio.h>
#include <utils/Log.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

using namespace android;
using namespace std;

namespace android_audio_legacy {

// ----------------------------------------------------------------------------

// Generic Accessor
void CSampleSpec::setSampleSpecItem(SampleSpecItem eSampleSpecItem, uint32_t uiValue) {
    assert(eSampleSpecItem < ENbSampleSpecItems);
    _auiSampleSpec[eSampleSpecItem] = uiValue;
}
uint32_t CSampleSpec::getSampleSpecItem(SampleSpecItem eSampleSpecItem) const {
    assert(eSampleSpecItem < ENbSampleSpecItems);
    return _auiSampleSpec[eSampleSpecItem];
}
// ----------------------------------------------------------------------------
}; // namespace android

