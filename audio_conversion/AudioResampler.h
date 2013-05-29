/*
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

#include <list>
#include "AudioConverter.h"

namespace android_audio_legacy {

class CResampler;

class CAudioResampler : public CAudioConverter {

    typedef std::list<CResampler*>::iterator ResamplerListIterator;

public:
    CAudioResampler(SampleSpecItem eSampleSpecItem);

    virtual ~CAudioResampler();

private:
    // forbid copy
    CAudioResampler(const CAudioResampler &);
    CAudioResampler& operator =(const CAudioResampler &);

    android::status_t resampleFrames(const void* src, void* dst, const uint32_t inFrames, uint32_t* outFrames);

    virtual android::status_t configure(const CSampleSpec& ssSrc, const CSampleSpec& ssDst);

    virtual android::status_t convert(const void* src, void** dst, uint32_t inFrames, uint32_t* outFrames);

    CResampler* _pResampler;
    CResampler* _pPivotResampler;

    // List of audio converter enabled
    std::list<CResampler*> _activeResamplerList;

    static const uint32_t _guiPivotSampleRate;
};

}; // namespace android
