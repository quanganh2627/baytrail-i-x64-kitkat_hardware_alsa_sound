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

#define LOG_TAG "AudioHardwareALSA"
#include <utils/Log.h>
#include <utils/String8.h>

#include <cutils/properties.h>
#include <media/AudioRecord.h>
#include <hardware_legacy/power.h>

#include "AudioHardwareALSA.h"

#ifndef ALSA_DEFAULT_SAMPLE_RATE
#define ALSA_DEFAULT_SAMPLE_RATE 44100 // in Hz
#endif

#define MAX_AGAIN_RETRY     2
#define WAIT_TIME_MS        20

namespace android_audio_legacy
{

// ----------------------------------------------------------------------------

static const int DEFAULT_SAMPLE_RATE = ALSA_DEFAULT_SAMPLE_RATE;

// ----------------------------------------------------------------------------

AudioStreamOutALSA::AudioStreamOutALSA(AudioHardwareALSA *parent, alsa_handle_t *handle) :
    ALSAStreamOps(parent, handle, "AudioOutLock"),
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
    return mixer()->setVolume (mHandle->curDev, left, right);
}

size_t AudioStreamOutALSA::generateSilence(size_t bytes)
{
    LOGD("%s: on alsa device(0x%x) in mode(0x%x)", __FUNCTION__, mHandle->curDev, mHandle->curMode);

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
            LOGE("Write: Cannot open alsa device(0x%x) in mode(0x%x).", mHandle->curDev, mHandle->curMode);
            return err;
        }
        mStandby = false;
    }

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

    snd_pcm_sframes_t n;
    size_t            sent = 0;
    int it = 0;

#ifdef USE_INTEL_SRC
    int32_t outBytes;
    char *buf;

    if (mHandle->expectedSampleRate != mHandle->sampleRate) {
        if (mParent->mResampler->setSampleRate(mHandle->sampleRate,
                                 mHandle->expectedSampleRate)) {
            mParent->mResampler->resample((void**)&buf, &outBytes, buffer, bytes);
            buffer = buf;
            bytes = outBytes;
        }
        else
            LOGE("write: resampler error, could not set sample rate");
    }
#endif

    do {
        n = snd_pcm_writei(mHandle->handle,
                           (char *)buffer + sent,
                           snd_pcm_bytes_to_frames(mHandle->handle, bytes - sent));

        if(n == -EAGAIN || (n >= 0 && static_cast<ssize_t>(snd_pcm_frames_to_bytes(mHandle->handle, n)) + sent < bytes)) {
            it++;
            if (it > MAX_AGAIN_RETRY){
                LOGE("write err: EAGAIN breaking...");
                return n;
            }
            snd_pcm_wait(mHandle->handle, WAIT_TIME_MS);
        }
        else if (n == -EBADFD) {
            LOGE("write err: %s, TRY REOPEN...", snd_strerror(n));
            err = mHandle->module->open(mHandle, mHandle->curDev, mHandle->curMode);
            if(err != NO_ERROR) {
                LOGE("Open device error");
                return err;
            }
        }
        else if (n == -ENODEV) {
             LOGE("write err: %s, bailing out", snd_strerror(n));
             return err;
        }
        else if (n < 0) {
            LOGE("write err: %s", snd_strerror(n));
            err = snd_pcm_recover(mHandle->handle, n, 1);
            if(err != NO_ERROR) {
                LOGE("pcm write recover error: %s", snd_strerror(n));
                return err;
            }
        }

        if(n > 0) {
#ifdef USE_INTEL_SRC
            mFrameCount += n / mParent->mResampler->mOutSampleRate *
                               mParent->mResampler->mInSampleRate;
#else
            mFrameCount += n;
#endif

            sent += static_cast<ssize_t>(snd_pcm_frames_to_bytes(mHandle->handle, n));
        }

    } while (sent < bytes);

    return sent;
}

status_t AudioStreamOutALSA::dump(int fd, const Vector<String16>& args)
{
    return NO_ERROR;
}

status_t AudioStreamOutALSA::open(int mode)
{
    AutoW lock(mParent->mLock);

    return ALSAStreamOps::open(NULL, mode);
}

status_t AudioStreamOutALSA::close()
{
    AutoW lock(mParent->mLock);

    if(!mHandle->handle) {
        LOGD("null\n");
    }
    if(mHandle->handle)
        snd_pcm_drain (mHandle->handle);
    ALSAStreamOps::close();

    releasePowerLock();

    return NO_ERROR;
}

status_t AudioStreamOutALSA::standby()
{
    LOGD("%s", __FUNCTION__);

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

status_t  AudioStreamOutALSA::setParameters(const String8& keyValuePairs)
{
    // Give a chance to parent to handle the change
    status_t status = mParent->setStreamParameters(this, true, keyValuePairs);

    if (status != NO_ERROR) {

        return status;
    }

    return ALSAStreamOps::setParameters(keyValuePairs);
}

bool AudioStreamOutALSA::isOut()
{
    return true;
}

}       // namespace android
