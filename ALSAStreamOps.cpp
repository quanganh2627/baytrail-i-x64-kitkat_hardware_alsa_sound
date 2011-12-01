/* ALSAStreamOps.cpp
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
//#define LOG_NDEBUG 0
#include <utils/Log.h>
#include <utils/String8.h>

#include <cutils/properties.h>
#include <media/AudioRecord.h>
#include <hardware_legacy/power.h>

#include "AudioHardwareALSA.h"
#include "AudioRoute.h"

#define DEVICE_OUT_BLUETOOTH_SCO_ALL (AudioSystem::DEVICE_OUT_BLUETOOTH_SCO | AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET | AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT)

#define DEFAULT_SAMPLE_RATE     44100
#define DEFAULT_BUFFER_SIZE     (DEFAULT_SAMPLE_RATE/ 5)

namespace android_audio_legacy
{

// ----------------------------------------------------------------------------

ALSAStreamOps::ALSAStreamOps(AudioHardwareALSA *parent, alsa_handle_t *handle) :
    mParent(parent),
    mHandle(handle),
    mPowerLock(false),
    mStandby(false),
    mDevices(0),
    mAudioRoute(NULL)
{
}

ALSAStreamOps::~ALSAStreamOps()
{
    AutoW lock(mParent->mLock);

    close();
}

// use emulated popcount optimization
// http://www.df.lth.se/~john_e/gems/gem002d.html
static inline uint32_t popCount(uint32_t u)
{
    u = ((u&0x55555555) + ((u>>1)&0x55555555));
    u = ((u&0x33333333) + ((u>>2)&0x33333333));
    u = ((u&0x0f0f0f0f) + ((u>>4)&0x0f0f0f0f));
    u = ((u&0x00ff00ff) + ((u>>8)&0x00ff00ff));
    u = ( u&0x0000ffff) + (u>>16);
    return u;
}

acoustic_device_t *ALSAStreamOps::acoustics()
{
    return mParent->mAcousticDevice;
}
vpc_device_t *ALSAStreamOps::vpc()
{
   return mParent->mvpcdevice;
}
lpe_device_t *ALSAStreamOps::lpe()
{
   return mParent->mlpedevice;
}

ALSAMixer *ALSAStreamOps::mixer()
{
    return mParent->mMixer;
}

status_t ALSAStreamOps::set(int      *format,
                            uint32_t *channels,
                            uint32_t *rate)
{
    if (channels && *channels != 0) {
        if (mHandle->channels != popCount(*channels))
            return BAD_VALUE;
    } else if (channels) {
        *channels = 0;
        if (mHandle->devices & AudioSystem::DEVICE_OUT_ALL)
            switch(mHandle->channels) {
            case 4:
                *channels |= AudioSystem::CHANNEL_OUT_BACK_LEFT;
                *channels |= AudioSystem::CHANNEL_OUT_BACK_RIGHT;
                // Fall through...
            default:
            case 2:
                *channels |= AudioSystem::CHANNEL_OUT_FRONT_RIGHT;
                // Fall through...
            case 1:
                *channels |= AudioSystem::CHANNEL_OUT_FRONT_LEFT;
                break;
            }
        else
            switch(mHandle->channels) {
            default:
            case 2:
                *channels |= AudioSystem::CHANNEL_IN_RIGHT;
                // Fall through...
            case 1:
                *channels |= AudioSystem::CHANNEL_IN_LEFT;
                break;
            }
    }

    if (rate && *rate > 0) {
        if (mHandle->sampleRate != *rate)
            return BAD_VALUE;
    } else if (rate)
        *rate = mHandle->sampleRate;

    snd_pcm_format_t iformat = mHandle->format;

    if (format) {
        switch(*format) {
        case AudioSystem::FORMAT_DEFAULT:
            break;

        case AudioSystem::PCM_16_BIT:
            iformat = SND_PCM_FORMAT_S16_LE;
            break;

        case AudioSystem::PCM_8_BIT:
            iformat = SND_PCM_FORMAT_S8;
            break;

        default:
            LOGE("Unknown PCM format %i. Forcing default", *format);
            break;
        }

        if (mHandle->format != iformat)
            return BAD_VALUE;

        switch(iformat) {
        default:
        case SND_PCM_FORMAT_S16_LE:
            *format = AudioSystem::PCM_16_BIT;
            break;
        case SND_PCM_FORMAT_S8:
            *format = AudioSystem::PCM_8_BIT;
            break;
        }
    }

    return NO_ERROR;
}

status_t ALSAStreamOps::setParameters(const String8& keyValuePairs)
{
    return NO_ERROR;
}

String8 ALSAStreamOps::getParameters(const String8& keys)
{
    AudioParameter param = AudioParameter(keys);
    String8 value;
    String8 key = String8(AudioParameter::keyRouting);

    if (param.get(key, value) == NO_ERROR) {
        param.addInt(key, (int)mHandle->curDev);
    }

    LOGV("getParameters() %s", param.toString().string());
    return param.toString();
}

uint32_t ALSAStreamOps::sampleRate() const
{
    return mHandle->sampleRate;
}

//
// Return the number of bytes (not frames)
//
size_t ALSAStreamOps::bufferSize() const
{
    snd_pcm_uframes_t bufferSize = mHandle->bufferSize;
    snd_pcm_uframes_t periodSize;

    size_t bytes;

    if(mHandle->handle) {
        snd_pcm_get_params(mHandle->handle, &bufferSize, &periodSize);

        bytes = static_cast<size_t>(snd_pcm_frames_to_bytes(mHandle->handle, bufferSize));
    } else
        bytes = DEFAULT_BUFFER_SIZE;
    // Not sure when this happened, but unfortunately it now
    // appears that the bufferSize must be reported as a
    // power of 2. This might be for OSS compatibility.
    for (size_t i = 1; (bytes & ~i) != 0; i<<=1)
        bytes &= ~i;

    return bytes;
}

int ALSAStreamOps::format() const
{
    int pcmFormatBitWidth;
    int audioSystemFormat;

    snd_pcm_format_t ALSAFormat = mHandle->format;

    pcmFormatBitWidth = snd_pcm_format_physical_width(ALSAFormat);
    switch(pcmFormatBitWidth) {
    case 8:
        audioSystemFormat = AudioSystem::PCM_8_BIT;
        break;

    default:
        LOG_FATAL("Unknown AudioSystem bit width %i!", pcmFormatBitWidth);

    case 16:
        audioSystemFormat = AudioSystem::PCM_16_BIT;
        break;
    }

    return audioSystemFormat;
}

uint32_t ALSAStreamOps::channels() const
{
    unsigned int count = mHandle->channels;
    uint32_t channels = 0;

    if (mHandle->curDev & AudioSystem::DEVICE_OUT_ALL)
        switch(count) {
        case 4:
            channels |= AudioSystem::CHANNEL_OUT_BACK_LEFT;
            channels |= AudioSystem::CHANNEL_OUT_BACK_RIGHT;
            // Fall through...
        default:
        case 2:
            channels |= AudioSystem::CHANNEL_OUT_FRONT_RIGHT;
            // Fall through...
        case 1:
            channels |= AudioSystem::CHANNEL_OUT_FRONT_LEFT;
            break;
        }
    else
        switch(count) {
        default:
        case 2:
            channels |= AudioSystem::CHANNEL_IN_RIGHT;
            // Fall through...
        case 1:
            channels |= AudioSystem::CHANNEL_IN_LEFT;
            break;
        }

    return channels;
}

void ALSAStreamOps::close()
{
 //   mParent->mALSADevice->close(mHandle);
    // Call unset stream from route instead
    if(mAudioRoute) {
        mAudioRoute->unsetStream(this, mParent->mode());
        mAudioRoute = NULL;
    }
}

void ALSAStreamOps::doClose()
{
    LOGD("ALSAStreamOps::doClose");
    mParent->mALSADevice->close(mHandle);
}

void ALSAStreamOps::doStandby()
{
    LOGD("ALSAStreamOps::doStandby");
    mParent->mALSADevice->standby(mHandle);
    mStandby = true;
}

//
// Set playback or capture PCM device.  It's possible to support audio output
// or input from multiple devices by using the ALSA plugins, but this is
// not supported for simplicity.
//
// The AudioHardwareALSA API does not allow one to set the input routing.
//
// If the "routes" value does not map to a valid device, the default playback
// device is used.
//
status_t ALSAStreamOps::open(uint32_t devices, int mode)
{
    assert(routeAvailable());
    assert(!mHandle->handle);

    status_t err = BAD_VALUE;
    err = mParent->mALSADevice->open(mHandle, devices, mode);


    LOGD("ALSAStreamOps::open");
    if(NO_ERROR == err) {
        if (mParent && mParent->mlpedevice && mParent->mlpedevice->lpecontrol) {
            err = mParent->mlpedevice->lpecontrol(mode,mHandle->curDev);
            if (err) {
                LOGE("setparam for lpe called with bad devices");
            }
        }
    }
    return err;
}

bool ALSAStreamOps::routeAvailable()
{
    if(mAudioRoute)
        return mAudioRoute->available(isOut());

    return false;
}

void ALSAStreamOps::vpcRoute(uint32_t devices, int mode)
{
    LOGD("vpcRoute");
    if((mode == AudioSystem::MODE_IN_COMMUNICATION) && (devices & DEVICE_OUT_BLUETOOTH_SCO_ALL)) {
        LOGD("vpcRoute BT Playback INCOMMUNICATION");
        mParent->mvpcdevice->route(VPC_ROUTE_OPEN);
    }
}

status_t ALSAStreamOps::setRoute(AudioRoute *audioRoute, uint32_t devices, int mode)
{
    LOGD("setRoute mode=%d", mode);
    if((mAudioRoute == audioRoute) && (mHandle->curDev == devices) && (mHandle->curMode == mode)) {
        LOGD("setRoute: stream already attached to the route, identical conditions");
        return NO_ERROR;
    }
    // unset stream from previous route (if any)
    if(mAudioRoute != NULL)
        mAudioRoute->unsetStream(this, mode);

    mAudioRoute = audioRoute;
    mDevices = devices;

    vpcRoute(mDevices, mode);

    // set stream to new route
    return mAudioRoute->setStream(this, mode);
}

status_t ALSAStreamOps::doRoute(int mode)
{
    LOGD("doRoute mode=%d", mode);
    open(mDevices, mode);

    if(mStandby) {
        LOGD("doRoute standby mode -> standby the device immediately after routing");
        doStandby();
    }
    return NO_ERROR;
}

status_t ALSAStreamOps::undoRoute()
{
    LOGD("doRoute undoRoute");
    doClose();

    return NO_ERROR;
}

}       // namespace android
