/* AudioHardwareALSA.cpp
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

extern "C"
{
    //
    // Function for dlsym() to look up for creating a new AudioHardwareInterface.
    //
    android::AudioHardwareInterface *createAudioHardware(void) {
        return android::AudioHardwareALSA::create();
    }
}         // extern "C"

namespace android
{
Mutex ow_lock;
Mutex or_lock;

void owr_lock(alsa_handle_t *handle)
{
    snd_pcm_stream_t direction = (handle->devices & AudioSystem::DEVICE_OUT_ALL) ?
                                 SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE;
    if (direction == SND_PCM_STREAM_PLAYBACK)
        ow_lock.lock();
    else
        or_lock.lock();
}

void owr_unlock(alsa_handle_t *handle)
{
    snd_pcm_stream_t direction = (handle->devices & AudioSystem::DEVICE_OUT_ALL) ?
                                 SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE;
    if (direction == SND_PCM_STREAM_PLAYBACK)
        ow_lock.unlock();
    else
        or_lock.unlock();
}

// ----------------------------------------------------------------------------

static void ALSAErrorHandler(const char *file,
                             int line,
                             const char *function,
                             int err,
                             const char *fmt,
                             ...)
{
    char buf[BUFSIZ];
    va_list arg;
    int l;

    va_start(arg, fmt);
    l = snprintf(buf, BUFSIZ, "%s:%i:(%s) ", file, line, function);
    vsnprintf(buf + l, BUFSIZ - l, fmt, arg);
    buf[BUFSIZ-1] = '\0';
    LOG(LOG_ERROR, "ALSALib", buf);
    va_end(arg);
}

AudioHardwareInterface *AudioHardwareALSA::create() {
    return new AudioHardwareALSA();
}

AudioHardwareALSA::AudioHardwareALSA() :
    mALSADevice(0),
    mAcousticDevice(0),
    mvpcdevice(0),
    mlpedevice(0)
{
    snd_lib_error_set_handler(&ALSAErrorHandler);
    mMixer = new ALSAMixer;
    status_t err_a = BAD_VALUE;

    hw_module_t *module;
    int err = hw_get_module(ALSA_HARDWARE_MODULE_ID,
                            (hw_module_t const**)&module);

    if (err == 0) {
        hw_device_t* device;
        err = module->methods->open(module, ALSA_HARDWARE_NAME, &device);
        if (err == 0) {
            mALSADevice = (alsa_device_t *)device;
            mALSADevice->init(mALSADevice, mDeviceList);
        } else
            LOGE("ALSA Module could not be opened!!!");
    } else
        LOGE("ALSA Module not found!!!");

    err = hw_get_module(ACOUSTICS_HARDWARE_MODULE_ID,
    (hw_module_t const**)&module);
    if (err == 0) {
        hw_device_t* device;
        err = module->methods->open(module,
        ACOUSTICS_HARDWARE_NAME, &device);
        if (err == 0)
            mAcousticDevice = (acoustic_device_t *)device;
        else
            LOGE("Acoustics Module not found.");
    }

    err = hw_get_module(VPC_HARDWARE_MODULE_ID,
    (hw_module_t const**)&module);

    if (err == 0) {
        hw_device_t* device;
        err = module->methods->open(module,VPC_HARDWARE_NAME, &device);
        if (err == 0){
            LOGD("VPC MODULE OK.");
            mvpcdevice = (vpc_device_t *) device;
            err = mvpcdevice->init();
            if (err)
                LOGE("audience init FAILED");
            }
        else
            LOGE("VPC Module not found");
    }

    err = hw_get_module(LPE_HARDWARE_MODULE_ID,
    (hw_module_t const**)&module);

    if (err == 0) {
        hw_device_t* device;
        err = module->methods->open(module,LPE_HARDWARE_NAME, &device);
        if (err == 0){
            LOGD("LPE MODULE OK.");
            mlpedevice = (lpe_device_t *) device;
            err = mlpedevice->init();
            if (err)
                LOGE("LPE init FAILED");
            }
        else
            LOGE("LPE Module not found");
    }
}

AudioHardwareALSA::~AudioHardwareALSA()
{
    if (mMixer) delete mMixer;
    if (mALSADevice)
        mALSADevice->common.close(&mALSADevice->common);
    if (mAcousticDevice)
        mAcousticDevice->common.close(&mAcousticDevice->common);
    if (mvpcdevice)
        mvpcdevice->common.close(&mvpcdevice->common);
    if (mlpedevice)
        mlpedevice->common.close(&mlpedevice->common);
}

status_t AudioHardwareALSA::initCheck()
{
    if (mALSADevice && mMixer && mMixer->isValid())
        return NO_ERROR;
    else
        return NO_INIT;
}

status_t AudioHardwareALSA::setVoiceVolume(float volume)
{
    // The voice volume is used by the VOICE_CALL audio stream.
    if (mvpcdevice)
        return mvpcdevice->amcvolume(volume);
    else
        return INVALID_OPERATION;
}

status_t AudioHardwareALSA::setMasterVolume(float volume)
{
    if (mMixer)
        return mMixer->setMasterVolume(volume);
    else
        return INVALID_OPERATION;
}

status_t AudioHardwareALSA::setMode(int mode)
{
    status_t status = NO_ERROR;
    status_t err_a = BAD_VALUE;

    if (mode != mMode) {
        status = AudioHardwareBase::setMode(mode);

        if (status == NO_ERROR) {
            // take care of mode change.
            for(ALSAHandleList::iterator it = mDeviceList.begin();
                    it != mDeviceList.end(); ++it) {
                owr_lock(&(*it));
                status = mALSADevice->route(&(*it), it->curDev, mode);
                owr_unlock(&(*it));
                if (status != NO_ERROR)
                    break;
                if (mvpcdevice) {
                    err_a = mvpcdevice->amcontrol(mode, it->curDev);
                    if (err_a) {
                        LOGE("set mode for vpc called with bad devices");
                    }
                }
                if (mlpedevice) {
                    err_a = mlpedevice->lpecontrol(mode, it->curDev);
                    if (err_a) {
                        LOGE("set mode for lpe called with bad devices");
                    }
                }
            }
        }
    }

    return status;
}

AudioStreamOut *
AudioHardwareALSA::openOutputStream(uint32_t devices,
                                    int *format,
                                    uint32_t *channels,
                                    uint32_t *sampleRate,
                                    status_t *status)
{
    AutoMutex lock(mLock);

    LOGD("openOutputStream called for devices: 0x%08x", devices);

    status_t err = BAD_VALUE;
    status_t err_a = BAD_VALUE;
    AudioStreamOutALSA *out = 0;

    if (devices & (devices - 1)) {
        if (status) *status = err;
        LOGD("openOutputStream called with bad devices");
        return out;
    }

    // Find the appropriate alsa device
    for(ALSAHandleList::iterator it = mDeviceList.begin();
            it != mDeviceList.end(); ++it)
        if (it->devices & devices) {
            owr_lock(&(*it));
            err = mALSADevice->open(&(*it), devices, mode());
            owr_unlock(&(*it));
            if (err) break;
            out = new AudioStreamOutALSA(this, &(*it));
            err = out->set(format, channels, sampleRate);
            if (mvpcdevice) {
                err_a = mvpcdevice->amcontrol(mode(), devices);
                if (err_a) {
                    LOGE("openOutputStream called with bad devices");
                }
            }
            break;
        }

    if (status) *status = err;
    return out;
}

void
AudioHardwareALSA::closeOutputStream(AudioStreamOut* out)
{
    AutoMutex lock(mLock);
    delete out;
}

AudioStreamIn *
AudioHardwareALSA::openInputStream(uint32_t devices,
                                   int *format,
                                   uint32_t *channels,
                                   uint32_t *sampleRate,
                                   status_t *status,
                                   AudioSystem::audio_in_acoustics acoustics)
{
    AutoMutex lock(mLock);

    status_t err = BAD_VALUE;
    AudioStreamInALSA *in = 0;
    status_t err_a = BAD_VALUE;

    if (devices & (devices - 1)) {
        if (status) *status = err;
        return in;
    }

    // Find the appropriate alsa device
    for(ALSAHandleList::iterator it = mDeviceList.begin();
            it != mDeviceList.end(); ++it)
        if (it->devices & devices) {
            owr_lock(&(*it));
            err = mALSADevice->open(&(*it), devices, mode());
            owr_unlock(&(*it));
            if (err) break;
            in = new AudioStreamInALSA(this, &(*it), acoustics);
            err = in->set(format, channels, sampleRate);
            if (mlpedevice) {
                err_a = mlpedevice->lpecontrol(mode(), devices);
                if (err_a) {
                    LOGE("openOutputStream called with bad devices");
                }
            }
            break;
        }

    if (status) *status = err;
    return in;
}

void
AudioHardwareALSA::closeInputStream(AudioStreamIn* in)
{
    AutoMutex lock(mLock);
    delete in;
}

status_t AudioHardwareALSA::setMicMute(bool state)
{
    if (mMixer)
        return mMixer->setCaptureMuteState(AudioSystem::DEVICE_OUT_EARPIECE, state);

    return NO_INIT;
}

status_t AudioHardwareALSA::getMicMute(bool *state)
{
    if (mMixer)
        return mMixer->getCaptureMuteState(AudioSystem::DEVICE_OUT_EARPIECE, state);

    return NO_ERROR;
}

size_t AudioHardwareALSA::getInputBufferSize(uint32_t sampleRate, int format, int channelCount)
{
    switch (sampleRate) {
    case 8000:
    case 11025:
    case 12000:
    case 16000:
    case 22050:
    case 24000:
    case 32000:
    case 44100:
    case 48000:
        break;
    default:
        LOGW("getInputBufferSize bad sampling rate: %d", sampleRate);
        return 0;
    }
    if (format != AudioSystem::PCM_16_BIT) {
        LOGW("getInputBufferSize bad format: %d", format);
        return 0;
    }
    if ((channelCount < 1) || (channelCount > 2)) {
        LOGW("getInputBufferSize bad channel count: %d", channelCount);
        return 0;
    }

    // Returns 20ms buffer size per channel
    return (sampleRate / 25) * channelCount;
}

status_t AudioHardwareALSA::dump(int fd, const Vector<String16>& args)
{
    return NO_ERROR;
}

}       // namespace android
