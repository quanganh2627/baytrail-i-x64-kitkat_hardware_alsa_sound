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

#include <utils/Log.h>
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioStreamInAlsa"

#include <utils/String8.h>

#include <cutils/properties.h>
#include <media/AudioRecord.h>
#include <hardware_legacy/power.h>

#include "AudioHardwareALSA.h"

// Maximum sample rate for VOIP which will use NB audio processing.
// A higher sample rate will use a WB audio processing.
#define MAX_VOIP_SAMPLE_RATE_FOR_NARROW_BAND_PROCESSING 8000

#define CAPTURE_BUFFER_TIME_US   (20000)  //microseconds

#define base ALSAStreamOps

namespace android_audio_legacy
{

AudioStreamInALSA::AudioStreamInALSA(AudioHardwareALSA *parent,
                                     alsa_handle_t *handle,
                                     AudioSystem::audio_in_acoustics audio_acoustics) :
    ALSAStreamOps(parent, handle, "AudioInLock"),
    mFramesLost(0),
    mAcoustics(audio_acoustics),
    mFramesIn(0),
    mProcessingFramesIn(0),
    mProcessingBuffer(NULL),
    mProcessingBufferSizeInFrames(0),
    mReferenceFramesIn(0),
    mReferenceBuffer(NULL),
    mReferenceBufferSizeInFrames(0),
    mPreprocessorsHandlerList(),
    mHwBuffer(NULL),
    mInputSource(0)
{
    acoustic_device_t *aDev = acoustics();

    if (aDev) aDev->set_params(aDev, mAcoustics, NULL);
}

AudioStreamInALSA::~AudioStreamInALSA()
{
    freeAllocatedBuffers();

    AutoW lock(mParent->mLock);
    for (Vector<AudioEffectHandle>::iterator i = mPreprocessorsHandlerList.begin(); i != mPreprocessorsHandlerList.end(); i++)
    {
        if (i->mEchoReference != NULL)
        {
            /* stop reading from echo reference */
            i->mEchoReference->read(i->mEchoReference, NULL);
            mParent->resetEchoReference(i->mEchoReference);
            i->mEchoReference = NULL;
        }
    }
    mPreprocessorsHandlerList.clear();
    close();

    free(mReferenceBuffer);

    free(mProcessingBuffer);
}

void AudioStreamInALSA::freeAllocatedBuffers()
{
    delete []mHwBuffer;
    mHwBuffer = NULL;
}

status_t AudioStreamInALSA::setGain(float gain)
{
    return mixer() ? mixer()->setMasterGain(gain) : (status_t)NO_INIT;
}

status_t AudioStreamInALSA::allocateProcessingMemory(ssize_t frames)
{
    mProcessingBufferSizeInFrames = frames;
    mProcessingBuffer = (int16_t *)realloc(mProcessingBuffer,
                                           mSampleSpec.convertFramesToBytes(mProcessingBufferSizeInFrames));
    ALOGD("%s(frames=%ld): mProcessingBuffer=%p size extended to %ld frames (i.e. %ld bytes)",
          __FUNCTION__,
          frames,
          mProcessingBuffer,
          mProcessingBufferSizeInFrames,
          mSampleSpec.convertFramesToBytes(mProcessingBufferSizeInFrames));

    if(mProcessingBuffer == NULL) {

        ALOGE(" %s(frames=%ld): realloc failed errno = %s!", __FUNCTION__, frames, strerror(errno));
        return NO_MEMORY;
    }
    return NO_ERROR;
}

size_t AudioStreamInALSA::generateSilence(void *buffer, size_t bytes)
{
    // Simulate audio input timing and send zeroed buffer
    usleep(((bytes * 1000 )/ frameSize() / sampleRate()) * 1000);
    memset(buffer, 0, bytes);
    mStandby = false;
    return bytes;
}

status_t AudioStreamInALSA::getNextBuffer(AudioBufferProvider::Buffer* pBuffer, int64_t pts)
{
    (void)pts;

    size_t maxFrames =  static_cast<size_t>(snd_pcm_bytes_to_frames(mHandle->handle, mHwBufferSize));

    ssize_t hwFramesToRead = (maxFrames < pBuffer->frameCount)? maxFrames : pBuffer->frameCount;
    ssize_t framesRead;

    framesRead = readHwFrames(mHwBuffer, hwFramesToRead);
    if (framesRead < 0) {

        return NOT_ENOUGH_DATA;
    }
    pBuffer->raw = mHwBuffer;
    pBuffer->frameCount = framesRead;

    return NO_ERROR;
}

void AudioStreamInALSA::releaseBuffer(AudioBufferProvider::Buffer* buffer)
{
    // Nothing special to do here...
}

ssize_t AudioStreamInALSA::readHwFrames(void *buffer, ssize_t frames)
{
    snd_pcm_sframes_t n;
    ssize_t received_frames = 0;
    status_t err;

    int wait_status = 0;

    while( received_frames < frames) {

        n = snd_pcm_readi(mHandle->handle,
                          (char *)buffer + snd_pcm_frames_to_bytes(mHandle->handle, received_frames),
                          frames - received_frames);

        if( (n == -EAGAIN) || ((n >= 0) && (n + received_frames) < frames)) {

            wait_status = mParent->getAlsaHwDevice()->wait_pcm(mHandle);

            if (wait_status == 0) {
                int remaining_frames;
                // Need to differentiate timeout after having failed to receive some sample and having received
                // some samples but less than the required number
                if (n < 0) {
                    n = 0;
                }

                remaining_frames = frames - received_frames - n;

                LOGW("%s: wait_pcm timeout! Generating %fms of silence.", __FUNCTION__, mHwSampleSpec.framesToMs(remaining_frames));

                // Timeout due to a potential hardware failure. We need to generate silence in this case.
                n +=  mHwSampleSpec.convertBytesToFrames(
                            generateSilence((char *)buffer + snd_pcm_frames_to_bytes(mHandle->handle, received_frames + n),
                                       mHwSampleSpec.convertFramesToBytes(remaining_frames)));
            }
        }
        else if (n == -EBADFD) {

            ALOGE("read err: %s, TRY REOPEN...", snd_strerror(n));
            err = mHandle->module->open(mHandle, mHandle->curDev, mHandle->curMode, mParent->getFmRxMode());
            if(err != NO_ERROR) {

                ALOGE("Open device error");
                return err;
            }
        }
        else if (n < 0) {

            ALOGE("read err: %s", snd_strerror(n));
            err = snd_pcm_recover(mHandle->handle, n, 1);
            if(err != NO_ERROR) {

                ALOGE("pcm read recover error: %s", snd_strerror(n));
                return err;
            }
        }
        if(n > 0) {

            received_frames += n;
        }
    }
    return received_frames;
}

ssize_t AudioStreamInALSA::readFrames(void *buffer, ssize_t frames)
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

ssize_t AudioStreamInALSA::processFrames(void *buffer, ssize_t frames)
{
    // first reload enough frames at the end of process input buffer
    if (mProcessingFramesIn < frames)
    {
        if (mProcessingBufferSizeInFrames < frames)
        {
            status_t ret = allocateProcessingMemory(frames);
            if (ret != NO_ERROR)
            {
                return ret;
            }
        }

        ssize_t read_frames = readFrames((char* )mProcessingBuffer + mSampleSpec.convertFramesToBytes(mProcessingFramesIn),
                                         frames - mProcessingFramesIn);
        if (read_frames < 0)
        {
            return read_frames;
        }
        /* OK, we have to process all read frames */
        mProcessingFramesIn += read_frames;
        assert(mProcessingFramesIn >=  frames);
    }

    audio_buffer_t in_buf;
    audio_buffer_t out_buf;
    ssize_t processed_frames = 0;
    ssize_t processing_frames_in = mProcessingFramesIn;
    Vector<AudioEffectHandle>::const_iterator i;
    int processingReturn = 0;

    // Then process the frames
    while ((processed_frames < frames) && (processing_frames_in > 0) && (processingReturn == 0))
    {
        for (i = mPreprocessorsHandlerList.begin(); i != mPreprocessorsHandlerList.end(); i++)
        {
            if (i->mEchoReference != NULL)
            {
                pushEchoReference(processing_frames_in, i->mPreprocessor, i->mEchoReference);
            }
            // in_buf.frameCount and out_buf.frameCount indicate respectively
            // the maximum number of frames to be consumed and produced by process()
            in_buf.frameCount = processing_frames_in;
            in_buf.s16 = (int16_t *)((char* )mProcessingBuffer + mSampleSpec.convertFramesToBytes(processed_frames));
            out_buf.frameCount = frames - processed_frames;
            out_buf.s16 = (int16_t *)((char* )buffer + mSampleSpec.convertFramesToBytes(processed_frames));

            processingReturn = (*(i->mPreprocessor))->process(i->mPreprocessor, &in_buf, &out_buf);
            if(processingReturn == 0)
            {
                //Note: it is useless to recopy the output of effect processing as input for the next effect processing
                //because it is done in webrtc::audio_processing

                // process() has updated the number of frames consumed and produced in
                // in_buf.frameCount and out_buf.frameCount respectively
                processing_frames_in -= in_buf.frameCount;
                processed_frames += out_buf.frameCount;
            }
        }
    }
    // if effects processing failed, at least, it is necessary to return the read HW frames
    if(processingReturn != 0)
    {
        ALOGD("%s: unable to apply any effect; returned value is %d", __FUNCTION__, processingReturn);
        memcpy(buffer,
               mProcessingBuffer,
               mSampleSpec.convertFramesToBytes(mProcessingFramesIn));
        processed_frames = mProcessingFramesIn;
    }
    else
    {
        // move remaining frames to the beginning of mProccesingBuffer because currently,
        // the configuration imposes working with 160 frames and effects library works
        // with 80 frames per cycle (10 ms), i.e. the processing of 160 read HW frames
        // requests two calls to effects library (which are done by while loop. In future or
        // if configuration changed, effects library processing could be not more multiple of
        // HW read frames, so it is necessary to realign the buffer
        if (processing_frames_in != 0)
        {
            assert(processing_frames_in > 0);
            memmove(mProcessingBuffer,
                    (char* )mProcessingBuffer + mSampleSpec.convertFramesToBytes(mProcessingFramesIn - processing_frames_in),
                    mSampleSpec.convertFramesToBytes(processing_frames_in));
        }
    }
    // at the end, we keep remainder frames not cosumed by effect processor.
    mProcessingFramesIn = processing_frames_in;

    return processed_frames;
}

ssize_t AudioStreamInALSA::read(void *buffer, ssize_t bytes)
{
    AutoR lock(mParent->mLock);

    status_t err;

    acquirePowerLock();

    // Check if the audio route is available for this stream
    if(!routeAvailable()) {

        return generateSilence(buffer, bytes);
    }

    if(mStandby) {

        err = ALSAStreamOps::open(mHandle->curDev, mHandle->curMode);
        if (err < 0) {

            ALOGE("%s: Cannot open alsa device(0x%x) in mode (%d)", __FUNCTION__, mHandle->curDev, mHandle->curMode);
            return err;
        }

        err = allocateHwBuffer();
        if (err < 0) {

            ALOGE("%s: Cannot allocate HwBuffer", __FUNCTION__);
            return err;
        }
        mStandby = false;
    }

    assert(mHandle->handle);

    // if we deal with a NULL sink -> do not let alsa do the job
    if (snd_pcm_type(mHandle->handle) == SND_PCM_TYPE_NULL)
    {
        return generateSilence(buffer, bytes);
    }

    if (mParent->getVpcHwDevice() && mParent->getVpcHwDevice()->mix_enable && mHandle->curMode == AudioSystem::MODE_IN_CALL) {

        mParent->getVpcHwDevice()->mix_enable(false, mHandle->curDev);
    }

    acoustic_device_t *aDev = acoustics();

    // If there is an acoustics module read method, then it overrides this
    // implementation (unlike AudioStreamOutALSA write).
    if (aDev && aDev->read) {
        return aDev->read(aDev, buffer, bytes);
    }

    ssize_t received_frames = -1;
    ssize_t frames = mSampleSpec.convertBytesToFrames(bytes);

    //workaround, will be replaced by BZ#49961
    // if at least one effect has been added
    //   if platform embeds an audience chip
    //     if (mode is not in_call and mode is not in_communication)) then
    //       the input stream should be processed using SW effects
    //     else
    //       *raw* samples are sent to upper layers
    //     end
    //   else
    //     the input stream should be processed using SW effects
    //   end
    // else
    //   *raw* samples are sent to upper layers
    // end
    //[*raw* samples but which have been treated by audience if pf embeds audience chip
    //if platform does not embed audience chip but effects are expected, it is sufficient
    //to add (configure) appropriate effects in /vendor/etc/audio_effects.conf file
    if (!mPreprocessorsHandlerList.empty() &&
            (!(mParent->getHaveAudience()) ||
             ((mHandle->curMode != AudioSystem::MODE_IN_CALL) && (mHandle->curMode != AudioSystem::MODE_IN_COMMUNICATION))))
    {
        received_frames = processFrames(buffer, frames);
    }
    else
    {
        received_frames = readFrames(buffer, frames);
    }

    if (received_frames < 0) {

        ALOGE("%s(buffer=%p, bytes=%ld) will return %ld (strerror \"%s\")", __FUNCTION__, buffer, bytes, received_frames, snd_strerror(received_frames));
        return received_frames;
    }

    ssize_t readBytes = mSampleSpec.convertFramesToBytes(received_frames);

    if(mParent->mMicMuteState ) {

        memset(buffer, 0, readBytes);
    }
    return readBytes;
}

status_t AudioStreamInALSA::dump(int fd, const Vector<String16>& args)
{
    return NO_ERROR;
}

status_t AudioStreamInALSA::open(int mode)
{
    AutoW lock(mParent->mLock);

    status_t status = ALSAStreamOps::open(0, mode);

    acoustic_device_t *aDev = acoustics();

    if (status == NO_ERROR && aDev)
        status = aDev->use_handle(aDev, mHandle);

    return status;
}

status_t AudioStreamInALSA::close()
{
    AutoW lock(mParent->mLock);
    ALOGD("StreamInAlsa close.\n");

    acoustic_device_t *aDev = acoustics();

    if (mHandle && aDev) aDev->cleanup(aDev);

    ALSAStreamOps::close();

    freeAllocatedBuffers();

    releasePowerLock();

    return NO_ERROR;
}

status_t AudioStreamInALSA::standby()
{
    ALOGD("StreamInAlsa standby.\n");

    status_t status = ALSAStreamOps::standby();

    freeAllocatedBuffers();

    return status;
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
    // Stupid interface wants us to have a side effect of clearing the count
    // but is defined as a const to prevent such a thing.
    ((AudioStreamInALSA *)this)->resetFramesLost();
    return count;
}

status_t AudioStreamInALSA::setAcousticParams(void *params)
{
    AutoW lock(mParent->mLock);

    acoustic_device_t *aDev = acoustics();

    return aDev ? aDev->set_params(aDev, mAcoustics, params) : (status_t)NO_ERROR;
}

//
// Return the number of bytes (not frames)
//
size_t AudioStreamInALSA::bufferSize() const
{
    int32_t iInterval;

    if (mInputSource == AUDIO_SOURCE_VOICE_COMMUNICATION) {

        iInterval = CAPTURE_BUFFER_TIME_US;

    } else {

        iInterval = CAPTURE_BUFFER_TIME_US * 4;
    }
    return base::bufferSize(iInterval);
}

status_t  AudioStreamInALSA::setParameters(const String8& keyValuePairs)
{
    AutoW lock(mParent->mLock);

    AudioParameter param = AudioParameter(keyValuePairs);
    String8 key = String8(AudioParameter::keyInputSource);
    int inputSource;
    uint32_t sampleRate;
    status_t status;
    vpc_band_t band;

    LOGD("%s in.\n", __FUNCTION__);
    status = param.getInt(key, inputSource);
    if (status == NO_ERROR) {
        mInputSource = inputSource;
        if (mParent->getVpcHwDevice() && mParent->getVpcHwDevice()->set_input_source) {
            mParent->getVpcHwDevice()->set_input_source(inputSource);
        }        
        if (inputSource == AUDIO_SOURCE_VOICE_COMMUNICATION) {
            // Check the stream sample rate to select the correct VOIP band.
            // Only the input stream holds the sample rate required by the client,
            // which allow to detect the VOIP codec sample rate.
            sampleRate = mSampleSpec.getSampleRate();
            LOGD("%s: VOIP stream sample rate is %dHz\n", __FUNCTION__, sampleRate);
            if (mParent->getVpcHwDevice()) {
                if (sampleRate <= MAX_VOIP_SAMPLE_RATE_FOR_NARROW_BAND_PROCESSING)
                    band = VPC_BAND_NARROW;
                else
                    band = VPC_BAND_WIDE;

                mParent->getVpcHwDevice()->set_band(band, AudioSystem::MODE_IN_COMMUNICATION);
            } else {
                LOGE("%s: No VPC HW device\n", __FUNCTION__);
            }
        }
        param.remove(key);
    }

    // Give a chance to parent to handle the change
    status = mParent->setStreamParameters(this, false, keyValuePairs);

    if (status != NO_ERROR) {

        LOGW("%s Bad status (%d).\n", __FUNCTION__, status);
        return status;
    }

    LOGD("%s out.\n", __FUNCTION__);
    return ALSAStreamOps::setParameters(keyValuePairs);
}

status_t AudioStreamInALSA::allocateHwBuffer()
{
    delete []mHwBuffer;
    mHwBuffer = NULL;

    snd_pcm_uframes_t periodSize;
    snd_pcm_uframes_t bufferSize;
    int err = snd_pcm_get_params(mHandle->handle, &bufferSize, &periodSize);
    if (err < 0) {

        return NO_INIT;
    }
    mHwBufferSize = static_cast<size_t>(snd_pcm_frames_to_bytes(mHandle->handle, periodSize));
    mHwBuffer = new char[mHwBufferSize];
    if (!mHwBuffer) {

        ALOGE("%s: cannot allocate resampler mHwbuffer", __FUNCTION__);
        return NO_MEMORY;
    }

    return NO_ERROR;
}

status_t AudioStreamInALSA::addAudioEffect(effect_handle_t effect)
{
    ALOGD("%s (effect=%p)", __FUNCTION__, effect);

    status_t ret = -EINVAL;
    effect_descriptor_t desc;
    echo_reference_itfe * reference = NULL;
    bool isAlreadyPresent = false;

    assert(effect != NULL);

    AutoW lock(mParent->mLock);
    int status = (*effect)->get_descriptor(effect, &desc);
    if (status == 0)
    {
        if (memcmp(&desc.type, FX_IID_AEC, sizeof(effect_uuid_t)) == 0)
        {
            ALOGD("%s (effect=%p): fine: effect is AEC", __FUNCTION__, effect);
            reference = mParent->getEchoReference(format(), channelCount(), sampleRate());
        }
        //audio effects processing is very costy in term of CPU, so useless to add the same effect more than one time
        for (Vector<AudioEffectHandle>::const_iterator i = mPreprocessorsHandlerList.begin(); i != mPreprocessorsHandlerList.end(); i++)
        {
            if(i->mPreprocessor == effect)
            {
                isAlreadyPresent = true;
                break;
            }
        }

        if (!isAlreadyPresent)
        {
            if(mPreprocessorsHandlerList.add(AudioEffectHandle(effect, reference)) < 0)
            {
                ALOGE("%s (effect=%p): unable to add effect!", __FUNCTION__, effect);
                ret = -ENOMEM;
            }
            else
            {
                ALOGD("%s (effect=%p): effect added. number of stored effects is %d", __FUNCTION__, effect, mPreprocessorsHandlerList.size());
                ret = NO_ERROR;
            }
        }
        else
        {
            ALOGW("%s (effect=%p): it is useless to add again the same effect", __FUNCTION__, effect);
            ret = NO_ERROR;
        }

    }
    else
    {
        ret = -EINVAL;
    }

    return ret;
}

status_t AudioStreamInALSA::removeAudioEffect(effect_handle_t effect)
{
    ALOGD("%s (effect=%p)", __FUNCTION__, effect);
    status_t ret = -EINVAL;

    assert(effect != NULL);

    AutoW lock(mParent->mLock);
    for (Vector<AudioEffectHandle>::iterator i = mPreprocessorsHandlerList.begin(); i != mPreprocessorsHandlerList.end(); i++)
    {
        if(i->mPreprocessor == effect)
        {
            ALOGD("%s (effect=%p): effect has been found. number of effects before erase %d", __FUNCTION__, effect, mPreprocessorsHandlerList.size());
            if (i->mEchoReference != NULL)
            {
                /* stop reading from echo reference */
                i->mEchoReference->read(i->mEchoReference, NULL);
                mParent->resetEchoReference(i->mEchoReference);
                i->mEchoReference = NULL;
            }

            mPreprocessorsHandlerList.erase(i);
            ALOGD("%s (effect=%p): number of effects after erase %d", __FUNCTION__, effect, mPreprocessorsHandlerList.size());

            ret = NO_ERROR;
            break; // it is possible to break here because it is not possible to add twice or more times the same effect to handler list.
        }
    }
    return ret;
}

void AudioStreamInALSA::getCaptureDelay(struct echo_reference_buffer * buffer)
{
    snd_pcm_sframes_t available_pcm_frames;
    long buf_delay;
    long rsmp_delay;
    long kernel_delay;
    long delay_ns;

    assert(buffer != NULL);

    clock_gettime(CLOCK_REALTIME, &buffer->time_stamp);

    available_pcm_frames = snd_pcm_avail(mHandle->handle);
    if (available_pcm_frames < 0) {

        ALOGE("%s: could not get delay", __FUNCTION__);
        available_pcm_frames = 0;
    }

    // read frames available in audio HAL input buffer
    // add number of frames being read as we want the capture time of first sample
    // in current buffer.
    buf_delay = (long)(((int64_t)(mFramesIn + mProcessingFramesIn) * CAudioUtils::CONVERT_USEC_TO_SEC) / sampleRate());

    // add delay introduced by kernel
    kernel_delay = (long)(((int64_t)available_pcm_frames * CAudioUtils::CONVERT_USEC_TO_SEC) / mHandle->sampleRate);

    /* add delay introduced by resampler */
    rsmp_delay = 0;

    delay_ns = kernel_delay + buf_delay + rsmp_delay;

    buffer->delay_ns = delay_ns;
}

int32_t AudioStreamInALSA::updateEchoReference(ssize_t frames, struct echo_reference_itfe * reference)
{
    struct echo_reference_buffer b;

    assert(reference != NULL);

    b.delay_ns = 0;

    if (mReferenceFramesIn < frames)
    {
        if (mReferenceBufferSizeInFrames < frames)
        {
            mReferenceBufferSizeInFrames = frames;
            mReferenceBuffer = (int16_t *)realloc(mReferenceBuffer,
                                                  mSampleSpec.convertFramesToBytes(mReferenceBufferSizeInFrames));
            if(mReferenceBuffer == NULL)
            {
                ALOGE(" %s(frames=%ld): realloc failed errno = %s!", __FUNCTION__, frames, strerror(errno));
                return errno;
            }
        }

        b.frame_count = frames - mReferenceFramesIn;
        b.raw = (void *)((char* )mReferenceBuffer +
                         mSampleSpec.convertFramesToBytes(mReferenceFramesIn));

        getCaptureDelay(&b);

        if (reference->read(reference, &b) == 0)
        {
            mReferenceFramesIn += b.frame_count;
        }
        else
        {
            ALOGW("%s: NOT enough frames to read ref buffer", __FUNCTION__);
        }
    }
    return b.delay_ns;
}

status_t AudioStreamInALSA::pushEchoReference(ssize_t frames, effect_handle_t preprocessor, struct echo_reference_itfe * reference)
{
    /* read frames from echo reference buffer and update echo delay
     * mReferenceFramesIn is updated with frames available in mReferenceBuffer */
    int32_t delay_us = updateEchoReference(frames, reference) / 1000;

    assert(preprocessor != NULL);
    assert(reference != NULL);

    if (mReferenceFramesIn < frames)
        frames = mReferenceFramesIn;

    status_t processingReturn = -EINVAL;

    if ((*preprocessor)->process_reverse == NULL)
    {
        ALOGW(" %s(frames %ld): process_reverse is NULL", __FUNCTION__, frames);
    }
    else
    {
        audio_buffer_t buf;

        buf.frameCount = mReferenceFramesIn;
        buf.s16 = mReferenceBuffer;

        processingReturn = (*preprocessor)->process_reverse(preprocessor,
                                                            &buf,
                                                            NULL);
        setPreprocessorEchoDelay(preprocessor, delay_us);
        mReferenceFramesIn -= buf.frameCount;

        if (mReferenceFramesIn > 0)
        {
            memcpy(mReferenceBuffer,
                   (char* )mReferenceBuffer + mSampleSpec.convertFramesToBytes(buf.frameCount),
                   mSampleSpec.convertFramesToBytes(mReferenceFramesIn));
        }
    }
    return(processingReturn);
}

status_t AudioStreamInALSA::setPreprocessorParam(effect_handle_t handle, effect_param_t *param)
{
    assert (handle != NULL);
    assert (param != NULL);

    status_t ret = -EINVAL;
    uint32_t size = sizeof(int);
    uint32_t psize = ((param->psize - 1) / sizeof(int) + 1) * sizeof(int) + param->vsize;

    ret = (*handle)->command(handle,
                             EFFECT_CMD_SET_PARAM,
                             sizeof (effect_param_t) + psize,
                             param,
                             &size,
                             &param->status);
    if (ret == 0)
    {
        ret = param->status;
    }
    return ret;
}

status_t AudioStreamInALSA::setPreprocessorEchoDelay(effect_handle_t handle,
                                                     int32_t delay_us)
{
    assert(handle != NULL);
    /** effect_param_t contains extensible field "data"
     * in our case, it is necessary to "allocate" memory to store
     * AEC_PARAM_ECHO_DELAY and delay_us as uint32_t
     * so, computation of "allocated" memory is size of
     * effect_param_t in uint32_t + 2
     */
    uint32_t buf[sizeof(effect_param_t) / sizeof(uint32_t) + 2];
    effect_param_t *param = (effect_param_t *)buf;

    param->psize = sizeof(uint32_t);
    param->vsize = sizeof(uint32_t);
    *(uint32_t *)param->data = AEC_PARAM_ECHO_DELAY;
    *((int32_t *)param->data + 1) = delay_us;

    return setPreprocessorParam(handle, param);
}

}       // namespace android
