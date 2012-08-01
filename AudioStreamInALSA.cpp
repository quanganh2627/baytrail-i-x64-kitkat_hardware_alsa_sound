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

namespace android_audio_legacy
{

AudioStreamInALSA::AudioStreamInALSA(AudioHardwareALSA *parent,
                                     alsa_handle_t *handle,
                                     AudioSystem::audio_in_acoustics audio_acoustics) :
    ALSAStreamOps(parent, handle, "AudioInLock"),
    mFramesLost(0),
    mAcoustics(audio_acoustics),
    mHwBuffer(NULL)
{
    acoustic_device_t *aDev = acoustics();

    if (aDev) aDev->set_params(aDev, mAcoustics, NULL);
}

AudioStreamInALSA::~AudioStreamInALSA()
{
    freeAllocatedBuffers();

    close();
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

size_t AudioStreamInALSA::generateSilence(void *buffer, size_t bytes)
{
    // Simulate audio input timing and send zeroed buffer
    usleep(((bytes * 1000 )/ frameSize() / sampleRate()) * 1000);
    memset(buffer, 0, bytes);
    mStandby = false;
    return bytes;
}

ssize_t AudioStreamInALSA::readHwFrames(void *buffer, ssize_t frames)
{
    snd_pcm_sframes_t n;
    ssize_t received_frames = 0;
    status_t err;

    while( received_frames < frames) {

        n = snd_pcm_readi(mHandle->handle,
                          (char *)buffer + snd_pcm_frames_to_bytes(mHandle->handle, received_frames),
                          frames - received_frames);
        if( (n == -EAGAIN) || ((n >= 0) && (n + received_frames) < frames)) {

            snd_pcm_wait(mHandle->handle, 1000);
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
    if (mSampleSpec == mHwSampleSpec) {

        return readHwFrames(buffer, frames);
    }

    ssize_t n;
    ssize_t received_frames = 0;
    ssize_t received_out_frames = 0;
    ssize_t buffer_index = 0;

    ssize_t framesDst = CAudioUtils::convertSrcToDstInFrames(frames, mSampleSpec, mHwSampleSpec);
    ssize_t maxDstFrames =  static_cast<size_t>(snd_pcm_bytes_to_frames(mHandle->handle, mHwBufferSize));
    status_t status;

    while(received_frames < framesDst) {

        ssize_t hwFramesToRead = (maxDstFrames < (framesDst - received_frames)? maxDstFrames : (framesDst - received_frames));
        n = readHwFrames(mHwBuffer, hwFramesToRead);

        if (n < 0) {

            ALOGE("pcm read recover error: %s", snd_strerror(n));
            return n;
        }

        char *outBuf = (char*)buffer + buffer_index;
        size_t dstFrames = 0;

        applyAudioConversion(mHwBuffer, (void**)&outBuf, n, &dstFrames);

        buffer_index += CAudioUtils::convertFramesToBytes(dstFrames, mSampleSpec);

        received_frames += n;
        received_out_frames += dstFrames;
    }
    return received_out_frames;
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
    ssize_t frames = CAudioUtils::convertBytesToFrames(bytes, mSampleSpec);

    received_frames = readFrames(buffer, frames);

    if (received_frames < 0) {

        ALOGE("%s(buffer=%p, bytes=%ld) will return %ld (strerror \"%s\")", __FUNCTION__, buffer, bytes, received_frames, snd_strerror(received_frames));
        return received_frames;
    }

    ssize_t readBytes = CAudioUtils::convertFramesToBytes(received_frames, mSampleSpec);

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

status_t  AudioStreamInALSA::setParameters(const String8& keyValuePairs)
{
    // Give a chance to parent to handle the change
    status_t status = mParent->setStreamParameters(this, false, keyValuePairs);

    if (status != NO_ERROR) {

        return status;
    }

    return ALSAStreamOps::setParameters(keyValuePairs);
}

bool AudioStreamInALSA::isOut()
{
    return false;
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
    mHwBufferSize = static_cast<size_t>(snd_pcm_frames_to_bytes(mHandle->handle, bufferSize));
    mHwBuffer = new char[mHwBufferSize];
    if (!mHwBuffer) {

        ALOGE("%s: cannot allocate resampler mHwbuffer", __FUNCTION__);
        return NO_MEMORY;
    }

    return NO_ERROR;
}

}       // namespace android
