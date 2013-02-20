/* AudioStreamOutALSA.cpp
 **
 ** Copyright 2008-2009 Wind River Systems
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#include <utils/Log.h>
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioStreamOutAlsa"

#include <utils/String8.h>

#include <cutils/properties.h>
#include <media/AudioRecord.h>
#include <hardware_legacy/power.h>

#include <tinyalsa/asoundlib.h>

#include "AudioStreamOutALSA.h"
#include "AudioAutoRoutingLock.h"
#include "AudioStreamRoute.h"


#define base ALSAStreamOps

namespace android_audio_legacy
{

const uint32_t AudioStreamOutALSA::MAX_AGAIN_RETRY = 2;
const uint32_t AudioStreamOutALSA::WAIT_TIME_MS = 20;
const uint32_t AudioStreamOutALSA::WAIT_BEFORE_RETRY_US = 10000; //10ms
const uint32_t AudioStreamOutALSA::LATENCY_TO_BUFFER_INTERVAL_RATIO = 2;
const uint32_t AudioStreamOutALSA ::USEC_PER_MSEC = 1000;
const uint32_t AudioStreamOutALSA::DEEP_PLAYBACK_PERIOD_TIME_US = 96000;
const uint32_t AudioStreamOutALSA::PLAYBACK_PERIOD_TIME_US = 48000;

AudioStreamOutALSA::AudioStreamOutALSA(AudioHardwareALSA *parent) :
    base(parent, PLAYBACK_PERIOD_TIME_US, "AudioOutLock"),
    mFrameCount(0),
    mFlags(AUDIO_OUTPUT_FLAG_NONE)
{
}

AudioStreamOutALSA::~AudioStreamOutALSA()
{
    close();
}

uint32_t AudioStreamOutALSA::channels() const
{
    return ALSAStreamOps::channels();
}

status_t AudioStreamOutALSA::setVolume(float __UNUSED left, float __UNUSED right)
{
    return NO_ERROR;
}

size_t AudioStreamOutALSA::generateSilence(size_t bytes)
{
    ALOGD("%s: on alsa device(0x%x)", __FUNCTION__, getCurrentDevices());

    usleep(((bytes * 1000 )/ frameSize() / sampleRate()) * 1000);
    mStandby = false;
    return bytes;
}

ssize_t AudioStreamOutALSA::write(const void *buffer, size_t bytes)
{
    setStandby(false);

    CAudioAutoRoutingLock lock(mParent);

    // Check if the audio route is available for this stream
    if (!isRouteAvailable()) {

        return generateSilence(bytes);
    }

    LOG_ALWAYS_FATAL_IF(mHandle == NULL);

    ssize_t srcFrames = mSampleSpec.convertBytesToFrames(bytes);
    size_t dstFrames = 0;
    char *dstBuf = NULL;
    status_t status;

    status = applyAudioConversion(buffer, (void**)&dstBuf, srcFrames, &dstFrames);

    if (status != NO_ERROR) {

        return status;
    }
    ALOGV("%s: srcFrames=%lu, bytes=%d dstFrames=%d", __FUNCTION__, srcFrames, bytes, dstFrames);

    ssize_t ret = writeFrames(dstBuf, dstFrames);

    if (ret < 0) {

        if (ret != -EPIPE) {

            // Returns asap to catch up the broken pipe error else, trash the audio data
            // and sleep the time the driver may use to consume it.
            generateSilence(bytes);
        }

        return ret;
    }
    ALOGV("%s: returns %lu", __FUNCTION__,
          CAudioUtils::convertFramesToBytes(CAudioUtils::convertSrcToDstInFrames(ret, mHwSampleSpec, mSampleSpec), mSampleSpec));
    return mSampleSpec.convertFramesToBytes(CAudioUtils::convertSrcToDstInFrames(ret, mHwSampleSpec, mSampleSpec));
}

ssize_t AudioStreamOutALSA::writeFrames(void* buffer, ssize_t frames)
{
    int ret;

    ret = pcm_write(mHandle,
                  (char *)buffer,
                  pcm_frames_to_bytes(mHandle, frames ));

    ALOGV("%s %d %d", __FUNCTION__, ret, pcm_frames_to_bytes(mHandle, frames));

    if (ret) {

        ALOGE("%s: write error: %s", __FUNCTION__, pcm_get_error(mHandle));
        return ret;
    }

    return frames;
}

status_t AudioStreamOutALSA::dump(int , const Vector<String16>& )
{
    return NO_ERROR;
}

status_t AudioStreamOutALSA::open(int __UNUSED mode)
{
    return setStandby(false);
}

//
// Called from Route Manager Context -> WLocked
//
status_t AudioStreamOutALSA::attachRoute()
{
    status_t status = base::attachRoute();
    if (status != NO_ERROR) {

        return status;
    }

    // Need to generate silence?
    LOG_ALWAYS_FATAL_IF(getCurrentRoute() == NULL || mHandle == NULL);

    uint32_t uiSilenceMs = getCurrentRoute()->getOutputSilencePrologMs();
    if (uiSilenceMs) {

        // Allocate a 1Ms buffer in stack
        uint32_t uiBufferSize = mHwSampleSpec.convertFramesToBytes(mHwSampleSpec.convertUsecToframes(1 * USEC_PER_MSEC));
        void* pSilenceBuffer = alloca(uiBufferSize);
        memset(pSilenceBuffer, 0, uiBufferSize);

        uint32_t uiMsCount;
        for (uiMsCount = 0; uiMsCount < uiSilenceMs; uiMsCount++) {

            pcm_write(mHandle,
                          (const char*)pSilenceBuffer,
                          uiBufferSize);
        }
    }

    return NO_ERROR;
}


status_t AudioStreamOutALSA::close()
{
    return setStandby(true);
}

status_t AudioStreamOutALSA::standby()
{
    mFrameCount = 0;

    return setStandby(true);
}

uint32_t AudioStreamOutALSA::latency() const
{
    // Android wants latency in milliseconds.
    return base::latency();
}

size_t AudioStreamOutALSA::bufferSize() const
{
    return getBufferSize(LATENCY_TO_BUFFER_INTERVAL_RATIO);
}

// return the number of audio frames written by the audio dsp to DAC since
// the output has exited standby
status_t AudioStreamOutALSA::getRenderPosition(uint32_t *dspFrames)
{
    *dspFrames = mFrameCount;
    return NO_ERROR;
}

// flush the data down the flow. It is similar to drop.
status_t AudioStreamOutALSA::flush()
{
    CAudioAutoRoutingLock lock(mParent);
    LOG_ALWAYS_FATAL_IF(mHandle == NULL);

    status_t status = pcm_stop(mHandle);
    ALOGD("pcm stop status %d", status);
    return status;
}


void AudioStreamOutALSA::setFlags(uint32_t uiFlags)
{
    mFlags = uiFlags;

    updatePeriodTime();
}

void AudioStreamOutALSA::updatePeriodTime()
{
    // Update the latency according to the flags
    uint32_t uiPeriodUs = mFlags & AUDIO_OUTPUT_FLAG_DEEP_BUFFER ?
                DEEP_PLAYBACK_PERIOD_TIME_US :
                PLAYBACK_PERIOD_TIME_US;

    setPeriodTime(uiPeriodUs);
}

status_t  AudioStreamOutALSA::setParameters(const String8& keyValuePairs)
{
    // Give a chance to parent to handle the change
    status_t status = mParent->setStreamParameters(this, keyValuePairs);

    if (status != NO_ERROR) {

        return status;
    }

    return ALSAStreamOps::setParameters(keyValuePairs);
}

}       // namespace android
