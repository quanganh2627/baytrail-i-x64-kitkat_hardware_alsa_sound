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

#define LOG_TAG "AudioStreamInALSA"

#include "AudioStreamInALSA.h"
#include "AudioAutoRoutingLock.h"

#include <hardware_legacy/power.h>
#include <media/AudioRecord.h>
#include <utils/Log.h>
#include <utils/String8.h>
#include <cutils/properties.h>
#include <algorithm>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>

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
    mFramesIn(0),
    mProcessingFramesIn(0),
    mProcessingBuffer(NULL),
    mProcessingBufferSizeInFrames(0),
    mReferenceFramesIn(0),
    mReferenceBuffer(NULL),
    mReferenceBufferSizeInFrames(0),
    mPreprocessorsHandlerList(),
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

status_t AudioStreamInALSA::getNextBuffer(AudioBufferProvider::Buffer* pBuffer,
                                          int64_t __UNUSED pts)
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

int AudioStreamInALSA::doProcessFrames(const void* buffer, ssize_t frames,
                                           ssize_t* processed_frames,
                                           ssize_t* processing_frames_in)
{
    int ret = 0;

    audio_buffer_t in_buf;
    audio_buffer_t out_buf;

    while ((*processed_frames < frames) && (*processing_frames_in > 0) && (ret == 0)) {

        Vector<AudioEffectHandle>::const_iterator it;
        for (it = mPreprocessorsHandlerList.begin(); it != mPreprocessorsHandlerList.end(); ++it) {

            if (it->mEchoReference != NULL) {

                pushEchoReference(*processing_frames_in, it->mPreprocessor, it->mEchoReference);
            }
            // in_buf.frameCount and out_buf.frameCount indicate respectively
            // the maximum number of frames to be consumed and produced by process()
            in_buf.frameCount = *processing_frames_in;
            in_buf.s16 = (int16_t *)((char* )mProcessingBuffer +
                                     mSampleSpec.convertFramesToBytes(*processed_frames));
            out_buf.frameCount = frames - *processed_frames;
            out_buf.s16 = (int16_t *)((char* )buffer +
                                      mSampleSpec.convertFramesToBytes(*processed_frames));

            ret = (*(it->mPreprocessor))->process(it->mPreprocessor, &in_buf, &out_buf);
            if (ret == 0)
            {
                //Note: it is useless to recopy the output of effect processing as input
                //for the next effect processing because it is done in webrtc::audio_processing

                // process() has updated the number of frames consumed and produced in
                // in_buf.frameCount and out_buf.frameCount respectively
                *processing_frames_in -= in_buf.frameCount;
                *processed_frames += out_buf.frameCount;
            }
        }
    }
    return ret;
}

ssize_t AudioStreamInALSA::processFrames(void* buffer, ssize_t frames)
{
    // first reload enough frames at the end of process input buffer
    if (mProcessingFramesIn < frames) {

        if (mProcessingBufferSizeInFrames < frames) {

            status_t ret = allocateProcessingMemory(frames);
            if (ret != OK) {

                return ret;
            }
        }

        ssize_t read_frames = readFrames((char* )mProcessingBuffer +
                                            mSampleSpec.convertFramesToBytes(mProcessingFramesIn),
                                         frames - mProcessingFramesIn);
        if (read_frames < 0) {

            return read_frames;
        }
        /* OK, we have to process all read frames */
        mProcessingFramesIn += read_frames;
        LOG_ALWAYS_FATAL_IF(mProcessingFramesIn < frames);
    }

    ssize_t processed_frames = 0;
    ssize_t processing_frames_in = mProcessingFramesIn;
    int processingReturn = 0;

    // Then process the frames
    processingReturn = doProcessFrames(buffer, frames, &processed_frames, &processing_frames_in);
    if (processingReturn != 0) {

        // Effects processing failed
        // at least, it is necessary to return the read HW frames
        ALOGD("%s: unable to apply any effect; returned value is %d", __FUNCTION__,
                                                                     processingReturn);
        memcpy(buffer,
               mProcessingBuffer,
               mSampleSpec.convertFramesToBytes(mProcessingFramesIn));
        processed_frames = mProcessingFramesIn;
    } else {

        // move remaining frames to the beginning of mProccesingBuffer because currently,
        // the configuration imposes working with 160 frames and effects library works
        // with 80 frames per cycle (10 ms), i.e. the processing of 160 read HW frames
        // requests two calls to effects library (which are done by while loop. In future or
        // if configuration changed, effects library processing could be not more multiple of
        // HW read frames, so it is necessary to realign the buffer
        if (processing_frames_in != 0) {

            LOG_ALWAYS_FATAL_IF(processing_frames_in <= 0);
            memmove(mProcessingBuffer,
                    (char* )mProcessingBuffer +
                    mSampleSpec.convertFramesToBytes(mProcessingFramesIn - processing_frames_in),
                    mSampleSpec.convertFramesToBytes(processing_frames_in));
        }
    }
    // at the end, we keep remainder frames not cosumed by effect processor.
    mProcessingFramesIn = processing_frames_in;

    return processed_frames;
}

ssize_t AudioStreamInALSA::read(void *buffer, ssize_t bytes)
{
    setStandby(false);

    CAudioAutoRoutingLock lock(mParent);

    // Check if the audio route is available for this stream
    if (!isRouteAvailable()) {

        ALOGD("%s(buffer=%p, bytes=%ld) No route available. Generating silence.",
            __FUNCTION__, buffer, static_cast<long int>(bytes));
        return generateSilence(buffer, bytes);
    }

    LOG_ALWAYS_FATAL_IF(mHandle == NULL);

    ssize_t received_frames = -1;
    ssize_t frames = mSampleSpec.convertBytesToFrames(bytes);

    if (!mPreprocessorsHandlerList.empty()) {

        received_frames = processFrames(buffer, frames);
    } else {

        received_frames = readFrames(buffer, frames);
    }

    if (received_frames < 0) {

        ALOGE("%s(buffer=%p, bytes=%ld) returns %ld. Generating silence.",
             __FUNCTION__, buffer, static_cast<long int>(bytes),
             static_cast<long int>(received_frames));
        //
        // Generate Silence here
        //
        generateSilence(buffer, bytes);
        return received_frames;
    }

    ssize_t readBytes = mSampleSpec.convertFramesToBytes(received_frames);

    if (mParent->mMicMuteState ) {

        ALOGD("%s(buffer=%p, bytes=%ld). Mic muted. Generating silence.",
            __FUNCTION__, buffer, static_cast<long int>(bytes));
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
    // Audio driver is expected to reset the value to 0 and restart counting upon
    // returning the current value by this function call.
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

    // Checks if any effect requested to add them
    checkAndAddAudioEffects();

    return allocateHwBuffer();
}

//
// Called from Route Manager Context -> WLocked
//
status_t AudioStreamInALSA::detachRoute()
{
    freeAllocatedBuffers();

    // Checks if any effect requested to remove them
    checkAndRemoveAudioEffects();

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

status_t AudioStreamInALSA::addAudioEffect(effect_handle_t effect)
{
    ALOGD("%s (effect=%p)", __FUNCTION__, effect);

    // Called from different context than the stream,
    // so Routing Lock must be held, so request the parent to perform the action.
    return mParent->addAudioEffectRequest(this, effect);
}

status_t AudioStreamInALSA::removeAudioEffect(effect_handle_t effect)
{
    ALOGD("%s (effect=%p)", __FUNCTION__, effect);

    // Called from different context than the stream,
    // so Routing Lock must be held, so request the parent to perform the action.
    return mParent->removeAudioEffectRequest(this, effect);
}

status_t AudioStreamInALSA::addAudioEffectRequest(effect_handle_t effect)
{
    AudioEffectsListIterator it;
    it = std::find(mRequestedEffects.begin(), mRequestedEffects.end(), effect);
    if (it != mRequestedEffects.end()) {

        ALOGW("%s: effets already requested", __FUNCTION__);
        return NO_ERROR;
    }

    ALOGD("%s list contains %d effects", __FUNCTION__, mRequestedEffects.size());
    ALOGD("%s (effect=%p)", __FUNCTION__, effect);
    mRequestedEffects.push_back(effect);
    ALOGD("%s list contains %d effects", __FUNCTION__, mRequestedEffects.size());
    return NO_ERROR;
}

status_t AudioStreamInALSA::removeAudioEffectRequest(effect_handle_t effect)
{

    AudioEffectsListIterator it;
    it = std::find(mRequestedEffects.begin(), mRequestedEffects.end(), effect);
    if (it != mRequestedEffects.end()) {

        // Remove element
        ALOGD("%s (effect=%p)", __FUNCTION__, effect);
        mRequestedEffects.erase(it);

        // Done
        return NO_ERROR;
    }

    ALOGD("%s (effect=%p) not found", __FUNCTION__, effect);
    return BAD_VALUE;
}

status_t AudioStreamInALSA::addAudioEffect_l(effect_handle_t effect, echo_reference_itfe *reference)
{
    LOG_ALWAYS_FATAL_IF(effect == NULL);

    //audio effects processing is very costy in term of CPU,
    // so useless to add the same effect more than one time
    Vector<AudioEffectHandle>::const_iterator it;
    it = std::find_if(mPreprocessorsHandlerList.begin(), mPreprocessorsHandlerList.end(),
                      std::bind2nd( MatchEffect(), effect) );
    if (it != mPreprocessorsHandlerList.end()) {

        ALOGW("%s (effect=%p): it is useless to add again the same effect",
                                                                        __FUNCTION__, effect);
        return NO_ERROR;
    }

    status_t ret = mPreprocessorsHandlerList.add(AudioEffectHandle(effect, reference));
    if (ret < 0) {

        ALOGE("%s (effect=%p): unable to add effect!", __FUNCTION__, effect);
        return ret;
    }
    ALOGD("%s (effect=%p): effect added. number of stored effects is %d", __FUNCTION__,
                                                    effect, mPreprocessorsHandlerList.size());
    return NO_ERROR;
}

//
// @brief Called from Route Manager locked context to remove an audio effect
//
status_t AudioStreamInALSA::removeAudioEffect_l(effect_handle_t effect)
{
    LOG_ALWAYS_FATAL_IF(effect == NULL);

    Vector<AudioEffectHandle>::iterator it;
    it = std::find_if(mPreprocessorsHandlerList.begin(), mPreprocessorsHandlerList.end(),
                      std::bind2nd( MatchEffect(), effect) );
    if (it != mPreprocessorsHandlerList.end()) {

        ALOGD("%s (effect=%p): effect has been found. number of effects before erase %d",
              __FUNCTION__, effect, mPreprocessorsHandlerList.size());
        if (it->mEchoReference != NULL) {

            /* stop reading from echo reference */
            it->mEchoReference->read(it->mEchoReference, NULL);
            mParent->resetEchoReference(it->mEchoReference);
            it->mEchoReference = NULL;
        }
        mPreprocessorsHandlerList.erase(it);
        ALOGD("%s (effect=%p): number of effects after erase %d",
              __FUNCTION__, effect, mPreprocessorsHandlerList.size());

        return NO_ERROR;
    }
    return BAD_VALUE;
}

void AudioStreamInALSA::getCaptureDelay(struct echo_reference_buffer* buffer)
{
    /* read frames available in kernel driver buffer */
    size_t kernel_frames;
    struct timespec tstamp;
    long buf_delay;
    long kernel_delay;
    long delay_ns;

    if (pcm_get_htimestamp(mHandle, &kernel_frames, &tstamp) < 0) {

        buffer->time_stamp.tv_sec  = 0;
        buffer->time_stamp.tv_nsec = 0;
        buffer->delay_ns           = 0;
        ALOGW("read get_capture_delay(): pcm_htimestamp error");
        return ;
    }
    // read frames available in audio HAL input buffer
    // add number of frames being read as we want the capture time of first sample
    // in current buffer.
    buf_delay = mSampleSpec.convertFramesToUsec(mFramesIn + mProcessingFramesIn);

    // add delay introduced by kernel
    kernel_delay = mHwSampleSpec.convertFramesToUsec(kernel_frames);

    delay_ns = kernel_delay + buf_delay;

    buffer->time_stamp = tstamp;
    buffer->delay_ns   = delay_ns;
    ALOGV("get_capture_delay time_stamp = [%ld].[%ld], delay_ns: [%d],"
          " kernel_delay:[%ld], buf_delay:[%ld], kernel_frames:[%d], ",
          buffer->time_stamp.tv_sec , buffer->time_stamp.tv_nsec, buffer->delay_ns,
          kernel_delay, buf_delay, kernel_frames);
}

int32_t AudioStreamInALSA::updateEchoReference(ssize_t frames,
                                               struct echo_reference_itfe* reference)
{
    struct echo_reference_buffer b;

    LOG_ALWAYS_FATAL_IF(reference == NULL);

    b.delay_ns = 0;

    if (mReferenceFramesIn < frames) {

        if (mReferenceBufferSizeInFrames < frames) {

            mReferenceBufferSizeInFrames = frames;
            int16_t* pReferenceBuffer = (int16_t *)realloc(mReferenceBuffer,
                                    mSampleSpec.convertFramesToBytes(mReferenceBufferSizeInFrames));
            if (pReferenceBuffer == NULL) {

                ALOGE(" %s(frames=%ld): realloc failed", __FUNCTION__,
                      static_cast<long int>(frames));
                return NO_MEMORY;
            }
            mReferenceBuffer = pReferenceBuffer;
        }

        b.frame_count = frames - mReferenceFramesIn;
        b.raw = (void *)((char* )mReferenceBuffer +
                         mSampleSpec.convertFramesToBytes(mReferenceFramesIn));

        getCaptureDelay(&b);

        if (reference->read(reference, &b) == 0) {

            mReferenceFramesIn += b.frame_count;
        } else {

            ALOGW("%s: NOT enough frames to read ref buffer", __FUNCTION__);
        }
    }
    return b.delay_ns;
}

status_t AudioStreamInALSA::pushEchoReference(ssize_t frames, effect_handle_t preprocessor,
                                              struct echo_reference_itfe* reference)
{
    /* read frames from echo reference buffer and update echo delay
     * mReferenceFramesIn is updated with frames available in mReferenceBuffer */
    int32_t delay_us = updateEchoReference(frames, reference) / 1000;

    LOG_ALWAYS_FATAL_IF(preprocessor == NULL || *preprocessor == NULL || reference == NULL);

    if (mReferenceFramesIn < frames) {

        frames = mReferenceFramesIn;
    }

    if ((*preprocessor)->process_reverse == NULL) {

        ALOGW(" %s(frames %ld): process_reverse is NULL", __FUNCTION__,
              static_cast<long int>(frames));
        return BAD_VALUE;
    }

    audio_buffer_t buf;

    buf.frameCount = mReferenceFramesIn;
    buf.s16 = mReferenceBuffer;

    status_t processingReturn = (*preprocessor)->process_reverse(preprocessor,
                                                        &buf,
                                                        NULL);
    setPreprocessorEchoDelay(preprocessor, delay_us);
    mReferenceFramesIn -= buf.frameCount;

    if (mReferenceFramesIn > 0) {

        memcpy(mReferenceBuffer,
               (char* )mReferenceBuffer + mSampleSpec.convertFramesToBytes(buf.frameCount),
               mSampleSpec.convertFramesToBytes(mReferenceFramesIn));
    }

    return processingReturn;
}

status_t AudioStreamInALSA::setPreprocessorParam(effect_handle_t handle, effect_param_t *param)
{
    LOG_ALWAYS_FATAL_IF(handle == NULL);
    LOG_ALWAYS_FATAL_IF(param == NULL);

    status_t ret;
    uint32_t size = sizeof(int);
    LOG_ALWAYS_FATAL_IF(param->psize < 1);
    uint32_t psize = ((param->psize - 1) / sizeof(int) + 1) * sizeof(int) + param->vsize;

    ret = (*handle)->command(handle,
                             EFFECT_CMD_SET_PARAM,
                             sizeof (effect_param_t) + psize,
                             param,
                             &size,
                             &param->status);

    return ret == 0 ? param->status : ret;
}

status_t AudioStreamInALSA::setPreprocessorEchoDelay(effect_handle_t handle, int32_t delay_us)
{
    LOG_ALWAYS_FATAL_IF(handle == NULL);
    /** effect_param_t contains extensible field "data"
     * in our case, it is necessary to "allocate" memory to store
     * AEC_PARAM_ECHO_DELAY and delay_us as uint32_t
     * so, computation of "allocated" memory is size of
     * effect_param_t in uint32_t + 2
     */
    uint32_t buf[sizeof(effect_param_t) / sizeof(uint32_t) + 2];
    effect_param_t *param = reinterpret_cast<effect_param_t*>(buf);

    param->psize = sizeof(uint32_t);
    param->vsize = sizeof(uint32_t);

    struct stDelay{
        uint32_t uiAecEchoDelay;
        uint32_t uiDelayUs;
    };
    stDelay* pData = reinterpret_cast<stDelay*>(param->data);

    pData->uiAecEchoDelay = AEC_PARAM_ECHO_DELAY;
    pData->uiDelayUs = delay_us;

    return setPreprocessorParam(handle, param);
}

status_t AudioStreamInALSA::allocateProcessingMemory(ssize_t frames)
{
    mProcessingBufferSizeInFrames = frames;

    int16_t* pProcessingBuffer = (int16_t *)realloc(mProcessingBuffer,
                                   mSampleSpec.convertFramesToBytes(mProcessingBufferSizeInFrames));
    if (pProcessingBuffer == NULL) {

        ALOGE(" %s(frames=%ld): realloc failed errno = %s!", __FUNCTION__,
              static_cast<long int>(frames), strerror(errno));
        return NO_MEMORY;
    }
    mProcessingBuffer = pProcessingBuffer;
    ALOGD("%s(frames=%ld): mProcessingBuffer=%p size extended to %ld frames (i.e. %d bytes)",
          __FUNCTION__,
          static_cast<long int>(frames),
          mProcessingBuffer,
          static_cast<long int>(mProcessingBufferSizeInFrames),
          mSampleSpec.convertFramesToBytes(mProcessingBufferSizeInFrames));

    return NO_ERROR;
}

status_t AudioStreamInALSA::checkAndAddAudioEffects()
{
    ALOGD("%s list contains %d effects", __FUNCTION__, mRequestedEffects.size());

    AudioEffectsListIterator it;

    for (it = mRequestedEffects.begin(); it != mRequestedEffects.end(); ++it) {

        mParent->addAudioEffect(this, *it);
    }
    return NO_ERROR;
}

status_t AudioStreamInALSA::checkAndRemoveAudioEffects()
{
    ALOGD("%s list contains %d effects", __FUNCTION__, mRequestedEffects.size());
    AudioEffectsListIterator it;

    for (it = mRequestedEffects.begin(); it != mRequestedEffects.end(); ++it) {

        mParent->removeAudioEffect(this, *it);
    }
    return NO_ERROR;
}

}       // namespace android
