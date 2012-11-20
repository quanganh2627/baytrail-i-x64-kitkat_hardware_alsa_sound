/* AudioResampler.cpp
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

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioResampler"
//#define LOG_NDEBUG 0

#include <stdlib.h>
#include <string.h>
#include <cutils/log.h>

#include "AudioResampler.h"
#include "Resampler.h"

#define base CAudioConverter

namespace android_audio_legacy{

const uint32_t CAudioResampler::_guiPivotSampleRate = 48000;

// ----------------------------------------------------------------------------

CAudioResampler::CAudioResampler(SampleSpecItem eSampleSpecItem) :
    base(eSampleSpecItem),
    _pResampler(new CResampler(ERateSampleSpecItem)),
    _pPivotResampler(new CResampler(ERateSampleSpecItem)),
    _activeResamplerList()
{
}

CAudioResampler::~CAudioResampler()
{
    _activeResamplerList.clear();
    delete _pResampler;
    delete _pPivotResampler;
}

status_t CAudioResampler::doConfigure(const CSampleSpec& ssSrc, const CSampleSpec& ssDst)
{
    _activeResamplerList.clear();

    status_t status = base::doConfigure(ssSrc, ssDst);
    if (status != NO_ERROR) {

        return status;
    }

    status = _pResampler->doConfigure(ssSrc, ssDst);
    if (status != NO_ERROR) {

        //
        // Our resampling lib does not support all conversions
        // using 2 resamplers
        //
        LOGD("%s: trying to use working sample rate @ 48kHz", __FUNCTION__);
        CSampleSpec pivotSs = ssDst;
        pivotSs.setSampleRate(_guiPivotSampleRate);

        status = _pPivotResampler->doConfigure(ssSrc, pivotSs);
        if (status != NO_ERROR) {

            LOGD("%s: trying to use pivot sample rate @ %dkHz: FAILED", __FUNCTION__, _guiPivotSampleRate);
            return status;
        }
        _activeResamplerList.push_back(_pPivotResampler);

        status = _pResampler->doConfigure(pivotSs, ssDst);
        if (status != NO_ERROR) {

            LOGD("%s: trying to use pivot sample rate @ 48kHz: FAILED", __FUNCTION__);
            return status;
        }
    }
    _activeResamplerList.push_back(_pResampler);

    return status;
}

status_t CAudioResampler::convert(const void* src, void** dst, uint32_t inFrames, uint32_t *outFrames)
{
    void *srcBuf = (void* )src;
    void *dstBuf = NULL;
    size_t srcFrames = inFrames;
    size_t dstFrames = 0;
    status_t status = NO_ERROR;

    ResamplerListIterator it;
    for (it = _activeResamplerList.begin(); it != _activeResamplerList.end(); ++it) {

        CResampler* pConv = *it;
        dstFrames = 0;

        if (*dst && pConv == _activeResamplerList.back()) {

            // Last converter must output within the provided buffer (if provided!!!)
            dstBuf = *dst;
        }
        status = pConv->convert(srcBuf, &dstBuf, srcFrames, &dstFrames);
        if (status != NO_ERROR) {

            return status;
        }
        srcBuf = dstBuf;
        srcFrames = dstFrames;
    }
    *dst = dstBuf;
    *outFrames = dstFrames;
    return status;
}

// ----------------------------------------------------------------------------
}; // namespace android
