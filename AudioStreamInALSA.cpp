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

#define LOG_TAG "AudioHardwareALSA"
#include <utils/Log.h>
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
    mAcoustics(audio_acoustics)
{
    acoustic_device_t *aDev = acoustics();

    if (aDev) aDev->set_params(aDev, mAcoustics, NULL);
}

AudioStreamInALSA::~AudioStreamInALSA()
{
    close();
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
            LOGE("Read: Cannot open alsa device(0x%x) in mode (%d)", mHandle->curDev, mHandle->curMode);
            return err;
        }
        mStandby = false;
    }

    // if we deal with a NULL sink -> do not let alsa do the job
    if (snd_pcm_type(mHandle->handle) == SND_PCM_TYPE_NULL)
    {
        return generateSilence(buffer, bytes);
    }

    if(mParent->mvpcdevice->mix_enable && mHandle->curMode == AudioSystem::MODE_IN_CALL) {
        mParent->mvpcdevice->mix_enable(false, mHandle->curDev);
    }

    acoustic_device_t *aDev = acoustics();

    // If there is an acoustics module read method, then it overrides this
    // implementation (unlike AudioStreamOutALSA write).
    if (aDev && aDev->read) {
        return aDev->read(aDev, buffer, bytes);
    }

    snd_pcm_sframes_t n;
    ssize_t            received = 0;

    do {
        n = snd_pcm_readi(mHandle->handle,
                           (char *)buffer + received,
                           snd_pcm_bytes_to_frames(mHandle->handle, bytes - received));

        if(n == -EAGAIN || (n >= 0 && static_cast<ssize_t>(snd_pcm_frames_to_bytes(mHandle->handle, n)) + received < bytes)) {
            snd_pcm_wait(mHandle->handle, 1000);
        }
        else if (n == -EBADFD) {
            LOGE("read err: %s, TRY REOPEN...", snd_strerror(n));
            err = mHandle->module->open(mHandle, mHandle->curDev, mHandle->curMode);
            if(err != NO_ERROR) {
                LOGE("Open device error");
                return err;
            }
        }
        else if (n < 0) {
            LOGE("read err: %s", snd_strerror(n));
            err = snd_pcm_recover(mHandle->handle, n, 1);
            if(err != NO_ERROR) {
                LOGE("pcm read recover error: %s", snd_strerror(n));
                return err;
            }
        }

        if(n > 0) {
            received += static_cast<ssize_t>(snd_pcm_frames_to_bytes(mHandle->handle, n));
        }
    } while(received < bytes);

    if(mParent->mMicMuteState ) {
        memset(buffer, 0, received);
    }

    return received;
}

status_t AudioStreamInALSA::dump(int fd, const Vector<String16>& args)
{
    return NO_ERROR;
}

status_t AudioStreamInALSA::open(int mode)
{
    AutoW lock(mParent->mLock);

    status_t status = ALSAStreamOps::open(NULL, mode);

    acoustic_device_t *aDev = acoustics();

    if (status == NO_ERROR && aDev)
        status = aDev->use_handle(aDev, mHandle);

    return status;
}

status_t AudioStreamInALSA::close()
{
    AutoW lock(mParent->mLock);
    LOGD("StreamInAlsa close.\n");

    acoustic_device_t *aDev = acoustics();

    if (mHandle && aDev) aDev->cleanup(aDev);

    ALSAStreamOps::close();

    releasePowerLock();

    return NO_ERROR;
}

status_t AudioStreamInALSA::standby()
{
    LOGD("StreamInAlsa standby.\n");

    status_t status = ALSAStreamOps::standby();

    return status;
}

void AudioStreamInALSA::resetFramesLost()
{
    AutoW lock(mParent->mLock);
    mFramesLost = 0;
}

unsigned int AudioStreamInALSA::getInputFramesLost() const
{
    unsigned int count = mFramesLost;
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

}       // namespace android
