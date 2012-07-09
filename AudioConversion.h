/* AudioConversion.h
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
#include "SampleSpec.h"


using namespace android;

namespace android_audio_legacy {

// ----------------------------------------------------------------------------

class CAudioConverter;

class CAudioConversion {

    typedef list<CAudioConverter*>::iterator AudioConverterListIterator;
    typedef list<CAudioConverter*>::const_iterator AudioConverterListConstIterator;

public:

    CAudioConversion();
    virtual ~CAudioConversion();

    status_t configure(const CSampleSpec& ssSrc, const CSampleSpec& ssDst);

    status_t convert(const void* src, void** dst, const uint32_t inFrames, uint32_t *outFrames);

private:
    status_t doConfigureAndAddConverter(SampleSpecItem eConverterType, CSampleSpec* pSsSrc, const CSampleSpec* pSsDst);

    status_t configureAndAddConverter(SampleSpecItem eConverterType, CSampleSpec* pSsSrc, const CSampleSpec* pSsDst);

    void emptyConversionChain();

    // List of audio converter enabled
    list<CAudioConverter*>  _pActiveAudioConvList;

    // List of Audio Converter objects available
    // (Each converter works on a dedicated sample spec item)
    CAudioConverter* _apAudioConverter[ENbSampleSpecItems];

    CSampleSpec   _ssSrc;
    CSampleSpec   _ssDst;
};

// ----------------------------------------------------------------------------
}; // namespace android
