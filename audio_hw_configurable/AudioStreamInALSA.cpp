/* AudioStreamInALSA.cpp
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

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioStreamInALSA"
#include <utils/Log.h>
#include <utils/String8.h>

#include <cutils/properties.h>
#include <media/AudioRecord.h>
#include <hardware_legacy/power.h>

#include "AudioStreamInALSA.h"
#include "AudioAutoRoutingLock.h"

#define base ALSAStreamOps


using namespace std;

namespace android_audio_legacy
{

const uint32_t AudioStreamInALSA::HIGH_LATENCY_TO_BUFFER_INTERVAL_RATIO = 1;
const uint32_t AudioStreamInALSA::LOW_LATENCY_TO_BUFFER_INTERVAL_RATIO = 4;
const uint32_t AudioStreamInALSA::CAPTURE_PERIOD_TIME_US = 40000;

AudioStreamInALSA::AudioStreamInALSA(AudioHardwareALSA *parent,
                                     AudioSystem::audio_in_acoustics audio_acoustics) :
    base(parent, CAPTURE_PERIOD_TIME_US, "AudioInLock"),
    mFramesLost(0),
    mAcoustics(audio_acoustics),
    mInputSource(0),
    mHwBuffer(NULL)
{
}

AudioStreamInALSA::~AudioStreamInALSA()
{
    // Note that memory allocated will
    // be freed upon doClose callback from route manager
    close();
}

status_t AudioStreamInALSA::setGain(float __UNUSED gain)
{
    return NO_ERROR;
}

size_t AudioStreamInALSA::generateSilence(void *buffer, size_t bytes)
{
    // Send zeroed buffer
    memset(buffer, 0, bytes);
    // No HW will drive the timeline:
    //       we are here because of hardware error or missing route availability.
    // Also, keep time sync by sleeping the equivalent amount of time.
    usleep(mSampleSpec.convertFramesToUsec(mSampleSpec.convertBytesToFrames(bytes)));
    mStandby = false;
    return bytes;
}

status_t AudioStreamInALSA::getNextBuffer(AudioBufferProvider::Buffer* pBuffer, int64_t __UNUSED pts)
{
    size_t maxFrames = static_cast<size_t>(pcm_bytes_to_frames(mHandle, mHwBufferSize));

    ssize_t hwFramesToRead = min(maxFrames, pBuffer->frameCount);
    ssize_t framesRead;

    framesRead = readHwFrames(mHwBuffer, hwFramesToRead);
    if (framesRead < 0) {

        return NOT_ENOUGH_DATA;
    }
    pBuffer->raw = mHwBuffer;
    pBuffer->frameCount = framesRead;

    return NO_ERROR;
}

void AudioStreamInALSA::releaseBuffer(AudioBufferProvider::Buffer __UNUSED * buffer)
{
    // Nothing special to do here...
}

ssize_t AudioStreamInALSA::readHwFrames(void *buffer, size_t frames)
{
    int ret = pcm_read(mHandle, (char *)buffer, mHwSampleSpec.convertFramesToBytes(frames));

    if (ret) {

        //
        // @todo: Shall we try some recover procedure???
        //
        ALOGE("%s: read error: requested %d (bytes=%d)frames %s",
              __FUNCTION__,
              frames,
              mHwSampleSpec.convertFramesToBytes(frames),
              pcm_get_error(mHandle));
        return ret;
    }

    return frames;
}

ssize_t AudioStreamInALSA::readFrames(void *buffer, size_t frames)
{
    //
    // No conversion required, read HW frames directly
    //
    if (mSampleSpec == mHwSampleSpec) {

        return readHwFrames(buffer, frames);
    }

    //
    // Otherwise, request for a converted buffer
    //
    status_t status = getConvertedBuffer(buffer, frames, this);
    if (status != NO_ERROR) {

        return status;
    }
    return frames;
}

ssize_t AudioStreamInALSA::read(void *buffer, ssize_t bytes)
{
    setStandby(false);

    CAudioAutoRoutingLock lock(mParent);

    // Check if the audio route is available for this stream
    if (!isRouteAvailable()) {

        ALOGD("%s(buffer=%p, bytes=%ld) No route available. Generating silence.",
            __FUNCTION__, buffer, bytes);
        return generateSilence(buffer, bytes);
    }

    LOG_ALWAYS_FATAL_IF(mHandle == NULL);

    ssize_t received_frames = -1;
    ssize_t frames = mSampleSpec.convertBytesToFrames(bytes);

    received_frames = readFrames(buffer, frames);

    if (received_frames < 0) {

        ALOGE("%s(buffer=%p, bytes=%ld) returns %ld. Generating silence.",
             __FUNCTION__, buffer, bytes, received_frames);
        //
        // Generate Silence here
        //
        generateSilence(buffer, bytes);
        return received_frames;
    }

    ssize_t readBytes = mSampleSpec.convertFramesToBytes(received_frames);

    if(mParent->mMicMuteState ) {

        ALOGD("%s(buffer=%p, bytes=%ld). Mic muted. Generating silence.",
            __FUNCTION__, buffer, bytes);
        generateSilence(buffer, readBytes);
    }
    return readBytes;
}

status_t AudioStreamInALSA::dump(int __UNUSED fd, const Vector<String16> __UNUSED &args)
{
    return NO_ERROR;
}

status_t AudioStreamInALSA::open(int __UNUSED mode)
{
    return setStandby(false);
}

status_t AudioStreamInALSA::close()
{
    return setStandby(true);
}

status_t AudioStreamInALSA::standby()
{
    return setStandby(true);
}

void AudioStreamInALSA::resetFramesLost()
{
    // setVoiceVolume and mixing during voice call cannot happen together
    // need a lock; but deadlock may appear during simultaneous R or W
    // so remove lock and the reset of mFramesLost which is never updated btw
}

unsigned int AudioStreamInALSA::getInputFramesLost() const
{
    unsigned int count = mFramesLost;   //set to 0 during construction

    AudioStreamInALSA* mutable_this = const_cast<AudioStreamInALSA*>(this);
    // Requirement from AudioHardwareInterface.h:
    // Audio driver is expected to reset the value to 0 and restart counting upon returning the current value by this function call.
    mutable_this->resetFramesLost();
    return count;
}

status_t  AudioStreamInALSA::setParameters(const String8& keyValuePairs)
{
    status_t status;

    ALOGD("%s in.\n", __FUNCTION__);

    // Give a chance to parent to handle the change
    status = mParent->setStreamParameters(this, keyValuePairs);

    if (status != NO_ERROR) {

        return status;
    }

    return ALSAStreamOps::setParameters(keyValuePairs);
}

status_t AudioStreamInALSA::allocateHwBuffer()
{
    unsigned int bufferSize;

    freeAllocatedBuffers();

    bufferSize = pcm_get_buffer_size(mHandle);

    mHwBufferSize = static_cast<size_t>(pcm_frames_to_bytes(mHandle, bufferSize));

    mHwBuffer = new char[mHwBufferSize];
    if (!mHwBuffer) {

        ALOGE("%s: cannot allocate resampler mHwbuffer", __FUNCTION__);
        return NO_MEMORY;
    }

    return NO_ERROR;
}

void AudioStreamInALSA::freeAllocatedBuffers()
{
     delete []mHwBuffer;
     mHwBuffer = NULL;
}

//
// Called from Route Manager Context -> WLocked
//
status_t AudioStreamInALSA::attachRoute()
{
    status_t status = base::attachRoute();
    if (status != NO_ERROR) {

        return status;
    }

    return allocateHwBuffer();
}

//
// Called from Route Manager Context -> WLocked
//
status_t AudioStreamInALSA::detachRoute()
{
    freeAllocatedBuffers();

    return base::detachRoute();
}

void AudioStreamInALSA::setInputSource(int inputSource)
{
    mInputSource = inputSource;
}

size_t AudioStreamInALSA::bufferSize() const
{
    uint32_t uiDivider = HIGH_LATENCY_TO_BUFFER_INTERVAL_RATIO;

    if (mInputSource == AUDIO_SOURCE_VOICE_COMMUNICATION) {

        uiDivider = LOW_LATENCY_TO_BUFFER_INTERVAL_RATIO;
    }
    return getBufferSize(uiDivider);
}

}       // namespace android
