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

#include "AudioHardwareALSA.h"

#define MAX_AGAIN_RETRY     2
#define WAIT_TIME_MS        20
#define WAIT_BEFORE_RETRY 10000 //10ms

#define LATENCY_TO_BUFFER_INTERVAL_RATIO  4

#define base ALSAStreamOps

namespace android_audio_legacy
{

AudioStreamOutALSA::AudioStreamOutALSA(AudioHardwareALSA *parent, alsa_handle_t *handle) :
    ALSAStreamOps(parent, handle, "AudioOutLock"),
    mFrameCount(0),
    mEchoReference(NULL)
{
}

AudioStreamOutALSA::~AudioStreamOutALSA()
{
    if(mEchoReference != NULL)
    {
        mEchoReference->write(mEchoReference, NULL);
    }
    close();
}

uint32_t AudioStreamOutALSA::channels() const
{
    int c = ALSAStreamOps::channels();
    return c;
}

status_t AudioStreamOutALSA::setVolume(float left, float right)
{
    return mixer()->setVolume (mHandle->curDev, left, right);
}

size_t AudioStreamOutALSA::generateSilence(size_t bytes)
{
    ALOGD("%s: on alsa device(0x%x) in mode(0x%x)", __FUNCTION__, mHandle->curDev, mHandle->curMode);

    usleep(((bytes * 1000 )/ frameSize() / sampleRate()) * 1000);
    mStandby = false;
    return bytes;
}

ssize_t AudioStreamOutALSA::write(const void *buffer, size_t bytes)
{
    AutoR lock(mParent->mLock);

    status_t err = NO_ERROR;

    acquirePowerLock();

    // Check if the audio route is available for this stream
    if(!routeAvailable()) {

        return generateSilence(bytes);
    }

    if(mStandby) {
        err = ALSAStreamOps::open(mHandle->curDev, mHandle->curMode);

        if (err < 0) {
            ALOGE("Write: Cannot open alsa device(0x%x) in mode(0x%x).", mHandle->curDev, mHandle->curMode);
            return err;
        }
        mStandby = false;
    }

    assert(mHandle->handle);

    // if we deal with a NULL sink -> do not let alsa do the job
    if (snd_pcm_type(mHandle->handle) == SND_PCM_TYPE_NULL)
    {
        return generateSilence(bytes);
    }

    if(mParent->getVpcHwDevice() && mParent->getVpcHwDevice()->mix_enable &&
            mHandle->curMode == AudioSystem::MODE_IN_CALL) {
        mParent->getVpcHwDevice()->mix_enable(true, mHandle->curDev);
    }

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

    // If applicable, push echo reference from Android effects
    if ( !mParent->getHaveAudience() ||
             ((mHandle->curMode != AudioSystem::MODE_IN_CALL) && (mHandle->curMode != AudioSystem::MODE_IN_COMMUNICATION))) {
        pushEchoReference(srcBuf, srcFrames);
    }

    status = applyAudioConversion(srcBuf, (void**)&dstBuf, srcFrames, &dstFrames);

    if (status != NO_ERROR) {

        return status;
    }

    ssize_t ret = writeFrames(dstBuf, dstFrames);

    if (ret < 0) {

        return ret;
    }

    return mSampleSpec.convertFramesToBytes(CAudioUtils::convertSrcToDstInFrames(ret, mHwSampleSpec, mSampleSpec));
}

void AudioStreamOutALSA::pushEchoReference(void *buffer, ssize_t frames)
{
    if (mEchoReference != NULL)
    {
        struct echo_reference_buffer b;
        b.raw = (void *)buffer;
        b.frame_count = frames;
        getPlaybackDelay(b.frame_count, &b);
        mEchoReference->write(mEchoReference, &b);
    }
}

ssize_t AudioStreamOutALSA::writeFrames(void* buffer, ssize_t frames)
{
    ssize_t sentFrames = 0;
    snd_pcm_sframes_t n;
    int it = 0;
    status_t err = NO_ERROR;

    do {
        n = snd_pcm_writei(mHandle->handle,
                           (char *)buffer + mHwSampleSpec.convertFramesToBytes(sentFrames),
                           frames - sentFrames);
        if( (n == -EAGAIN) || ((n >= 0) && (n + sentFrames) < frames)) {

            int wait_status = mParent->getAlsaHwDevice()->wait_pcm(mHandle);

            if (wait_status == 0) {
                int remaining_frames;
                // Need to differentiate timeout after having failed to receive some sample and having received
                // some samples but less than the required number
                if (n < 0) {
                    n = 0;
                }

                remaining_frames = frames - sentFrames - n;

                ALOGW("%s: wait_pcm timeout! Generating %fms of silence.", __FUNCTION__, mHwSampleSpec.framesToMs(sentFrames));

                // Timeout due to a potential hardware failure. We need to generate silence in this case.
                n += mHwSampleSpec.convertBytesToFrames(remaining_frames);
            }
        }
        else if (n == -EBADFD) {
            ALOGE("write err: %s, TRY REOPEN...", snd_strerror(n));
            err = mHandle->module->open(mHandle, mHandle->curDev, mHandle->curMode, mParent->getFmRxMode());
            if(err != NO_ERROR) {
                ALOGE("Open device error");
                return err;
            }
        }
        else if (n == -ENODEV) {
            ALOGE("write err: %s, bailing out", snd_strerror(n));
            return err;
        }
        else if (n < 0) {
            ALOGE("write err: %s", snd_strerror(n));
            for (unsigned int totalSleepTime = 0; totalSleepTime < mHandle->latency; totalSleepTime += WAIT_BEFORE_RETRY) {
                err = snd_pcm_recover(mHandle->handle, n, 1);
                if ((err == -EAGAIN) &&
                        (mHandle->curDev & AudioSystem::DEVICE_OUT_AUX_DIGITAL)) {
                    //When EPIPE occurs and snd_pcm_recover() function is invoked, in case of HDMI,
                    //processing of this request is done at the interrupt boundary in driver code,
                    //which would be responded by -EAGAIN till the interrupt boundary.
                    //An error handling mechanism is provided here, which sends repeated requests for
                    //recovery after a delay of 10ms each time till the "totalSleepTime" is less than
                    //"Latency" (i.e. "PERIOD_TIME * 2" for Playback in normal mode).

                    usleep(WAIT_BEFORE_RETRY);
                }
                else
                    break;
            }
            if(err != NO_ERROR) {
                ALOGE("pcm write recover error: %s", snd_strerror(n));
                return err;
            }
        }

        if(n > 0) {

            mFrameCount += CAudioUtils::convertSrcToDstInFrames(n, mHwSampleSpec, mSampleSpec);
            sentFrames += n;
        }

    } while (sentFrames < frames);

    return sentFrames;
}

status_t AudioStreamOutALSA::dump(int , const Vector<String16>& )
{
    return NO_ERROR;
}

status_t AudioStreamOutALSA::open(int mode)
{
    AutoW lock(mParent->mLock);

    return ALSAStreamOps::open(0, mode);
}

status_t AudioStreamOutALSA::close()
{
    AutoW lock(mParent->mLock);

    if(!mHandle->handle) {
        ALOGD("null\n");
    }
    if(mHandle->handle)
        snd_pcm_drain (mHandle->handle);
    ALSAStreamOps::close();

    releasePowerLock();

    return NO_ERROR;
}

status_t AudioStreamOutALSA::standby()
{
    ALOGD("%s", __FUNCTION__);

    status_t status = ALSAStreamOps::standby();

    mFrameCount = 0;

    return status;
}

#define USEC_TO_MSEC(x) ((x + 999) / 1000)

uint32_t AudioStreamOutALSA::latency() const
{
    // Android wants latency in milliseconds.
    return USEC_TO_MSEC (mHandle->latency);
}

// return the number of audio frames written by the audio dsp to DAC since
// the output has exited standby
status_t AudioStreamOutALSA::getRenderPosition(uint32_t *dspFrames)
{
    *dspFrames = mFrameCount;
    return NO_ERROR;
}

//
// Return the number of bytes (not frames)
//
size_t AudioStreamOutALSA::bufferSize() const
{
    return base::bufferSize(mHandle->latency / LATENCY_TO_BUFFER_INTERVAL_RATIO);
}

status_t  AudioStreamOutALSA::setParameters(const String8& keyValuePairs)
{
    // Give a chance to parent to handle the change
    status_t status = mParent->setStreamParameters(this, true, keyValuePairs);

    if (status != NO_ERROR) {

        return status;
    }

    return ALSAStreamOps::setParameters(keyValuePairs);
}

void AudioStreamOutALSA::addEchoReference(struct echo_reference_itfe * reference)
{
    ALOGD("%s(reference = %p): note mEchoReference = %p", __FUNCTION__, reference, mEchoReference);
    assert(reference != NULL);

    // Called from a WLocked context
    mEchoReference = reference;
}

void AudioStreamOutALSA::removeEchoReference(struct echo_reference_itfe * reference)
{
    ALOGD("%s(reference = %p): note mEchoReference = %p", __FUNCTION__, reference, mEchoReference);

    // Called from a WLocked context
    if(mEchoReference == reference)
    {
        mEchoReference->write(mEchoReference, NULL);
        mEchoReference = NULL;
    }
}

int AudioStreamOutALSA::getPlaybackDelay(ssize_t frames, struct echo_reference_buffer * buffer)
{
    snd_pcm_sframes_t available_pcm_frames = 0;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t period_size;
    long kernel_delay = 0;
    int status;

    clock_gettime(CLOCK_REALTIME, &buffer->time_stamp);

    available_pcm_frames = snd_pcm_avail(mHandle->handle);
    if (available_pcm_frames < 0) {

        ALOGE("%s: could not get delay", __FUNCTION__);
        available_pcm_frames = 0;
    }

    status = snd_pcm_get_params(mHandle->handle, &buffer_size, &period_size);
    if(status == 0) {

        // available_pcm_frames is the number of frame ready to be written in the alsa buffer
        // kernel delay is equal to the time spent to read the remaining frames
        // within the alsa buffer.
        kernel_delay = (buffer_size - available_pcm_frames) * CAudioUtils::CONVERT_USEC_TO_SEC / mHandle->sampleRate;
    }

    // adjust render time stamp with delay added by current driver buffer.
    // Add the duration of current frame as we want the render time of the last
    // sample being written.
    buffer->delay_ns = kernel_delay + (long)(((int64_t)(frames) * CAudioUtils::CONVERT_USEC_TO_SEC) / sampleRate());

    return 0;
}

}       // namespace android
