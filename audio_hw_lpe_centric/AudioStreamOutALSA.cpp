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

#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>

#include <utils/Log.h>
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioStreamOutAlsa"

#include <utils/String8.h>

#include <cutils/properties.h>
#include <media/AudioRecord.h>
#include <hardware_legacy/power.h>

#include "AudioStreamOutALSA.h"
#include "AudioAutoRoutingLock.h"



#define base ALSAStreamOps

namespace android_audio_legacy
{

const uint32_t AudioStreamOutALSA::MAX_AGAIN_RETRY = 2;
const uint32_t AudioStreamOutALSA::WAIT_TIME_MS = 20;
const uint32_t AudioStreamOutALSA::WAIT_BEFORE_RETRY = 10000; //10ms
const uint32_t AudioStreamOutALSA::LATENCY_TO_BUFFER_INTERVAL_RATIO = 4;

AudioStreamOutALSA::AudioStreamOutALSA(AudioHardwareALSA *parent) :
    base(parent, "AudioOutLock"),
    mFrameCount(0)
{
}

AudioStreamOutALSA::~AudioStreamOutALSA()
{
    close();
}

uint32_t AudioStreamOutALSA::channels() const
{
    int c = ALSAStreamOps::channels();
    return c;
}

status_t AudioStreamOutALSA::setVolume(float left, float right)
{
    return NO_ERROR;
}

size_t AudioStreamOutALSA::generateSilence(size_t bytes)
{
    ALOGD("%s: on alsa device(0x%x)", __FUNCTION__, getCurrentDevice());

    usleep(((bytes * 1000 )/ frameSize() / sampleRate()) * 1000);
    mStandby = false;
    return bytes;
}

ssize_t AudioStreamOutALSA::write(const void *buffer, size_t bytes)
{
    acquirePowerLock();

    setStandby(false);

    // DONOT take routing lock anymore on MRFLD.
    // TODO: shall we need a lock within streams?

    // Check if the audio route is available for this stream
    if (!isRouteAvailable()) {

        return generateSilence(bytes);
    }

    assert(mHandle->handle);

    acoustic_device_t *aDev = acoustics();

    // For output, we will pass the data on to the acoustics module, but the actual
    // data is expected to be sent to the audio device directly as well.
    if (aDev && aDev->write)
        aDev->write(aDev, buffer, bytes);


    ssize_t srcFrames = mSampleSpec.convertBytesToFrames(bytes);
    size_t dstFrames = 0;
    char *srcBuf = (char* )buffer;
    char *dstBuf = NULL;
    status_t status;

    status = applyAudioConversion(srcBuf, (void**)&dstBuf, srcFrames, &dstFrames);

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
    ALOGV("%s: returns %lu", __FUNCTION__, CAudioUtils::convertFramesToBytes(CAudioUtils::convertSrcToDstInFrames(ret, mHwSampleSpec, mSampleSpec), mSampleSpec));
    return mSampleSpec.convertFramesToBytes(CAudioUtils::convertSrcToDstInFrames(ret, mHwSampleSpec, mSampleSpec));
}

ssize_t AudioStreamOutALSA::writeFrames(void* buffer, ssize_t frames)
{
    int ret = 0;

    ret = pcm_write(mHandle->handle,
                  (char *)buffer,
                  pcm_frames_to_bytes(mHandle->handle, frames ));

    ALOGV("%s gustave %d %d", __FUNCTION__, ret, pcm_frames_to_bytes(mHandle->handle, frames));

    if (ret) {

        ALOGE("%s: write error: %s", __FUNCTION__, pcm_get_error(mHandle->handle));
        return ret;
    }

    return frames;
}

status_t AudioStreamOutALSA::dump(int , const Vector<String16>& )
{
    return NO_ERROR;
}

status_t AudioStreamOutALSA::open(int mode)
{
    return setStandby(false);
}

status_t AudioStreamOutALSA::close()
{
    return setStandby(true);
}

status_t AudioStreamOutALSA::standby()
{
    LOGD("%s", __FUNCTION__);

    status_t status = setStandby(true);

    mFrameCount = 0;

    return status;
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
    // Not supported.
    return NO_ERROR;
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
