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
#include <limits>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "ALSAStreamOps"

#include <utils/Log.h>
#include <utils/String8.h>

#include <cutils/properties.h>
#include <cutils/bitops.h>
#include <media/AudioRecord.h>
#include <hardware_legacy/power.h>

#include "ALSAStreamOps.h"
#include "AudioStreamRoute.h"
#include "AudioConverter.h"
#include "AudioConversion.h"
#include "AudioResampler.h"
#include "AudioReformatter.h"
#include "AudioRemapper.h"
#include "AudioConversion.h"
#include "AudioHardwareALSA.h"

#define DEVICE_OUT_BLUETOOTH_SCO_ALL (AudioSystem::DEVICE_OUT_BLUETOOTH_SCO | AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET | AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT)


using namespace android;

namespace android_audio_legacy
{

const uint32_t ALSAStreamOps::STR_FORMAT_LENGTH = 32;

ALSAStreamOps::ALSAStreamOps(AudioHardwareALSA *parent, const char* pcLockTag) :
    mParent(parent),
    mHandle(NULL),
    mStandby(true),
    mDevices(0),
    mIsReset(false),
    mCurrentRoute(NULL),
    mNewRoute(NULL),
    mCurrentDevices(0),
    mNewDevices(0),
    mLatencyUs(0),
    mPowerLock(false),
    mPowerLockTag(pcLockTag),
    mAudioConversion(new CAudioConversion)
{
    mSampleSpec.setChannelCount(AudioHardwareALSA::DEFAULT_CHANNEL_COUNT);
    mSampleSpec.setSampleRate(AudioHardwareALSA::DEFAULT_SAMPLE_RATE);
    mSampleSpec.setFormat(AudioHardwareALSA::DEFAULT_FORMAT);
}

ALSAStreamOps::~ALSAStreamOps()
{
    setStandby(true);

    delete mAudioConversion;
}

status_t ALSAStreamOps::set(int      *format,
                            uint32_t *channels,
                            uint32_t *rate)
{
    bool bad_channels = false;
    bool bad_rate = false;
    bool bad_format = false;

    ALOGV("%s() -- IN", __FUNCTION__);

    if (channels) {

        if (*channels != 0) {

            ALOGD("%s(requested channels: 0x%x (popcount returns %d))",
                  __FUNCTION__, *channels, popcount(*channels));
            // Always accept the channels requested by the client
            // as far as the channel count is supported
            mSampleSpec.setChannelMask(*channels);

            if (popcount(*channels) > 2) {

                ALOGD("%s: channels=(0x%x, %d) not supported", __FUNCTION__, *channels, popcount(*channels));
                bad_channels = true;
            }
        }
        if ((bad_channels) || (*channels == 0)) {

            // No channels information was provided by the client
            // or not supported channels
            // Use default: stereo
            if (isOut()) {

                *channels = AudioSystem::CHANNEL_OUT_FRONT_LEFT | AudioSystem::CHANNEL_OUT_FRONT_RIGHT;
            }
            else {

                *channels = AudioSystem::CHANNEL_IN_LEFT | AudioSystem::CHANNEL_IN_RIGHT;
            }
            mSampleSpec.setChannelMask(*channels);
        }
        ALOGD("%s: set channels to 0x%x", __FUNCTION__, *channels);

        // Resampler is always working @ the channel count of the HAL
        mSampleSpec.setChannelCount(popcount(mSampleSpec.getChannelMask()));
    }

    if (rate) {

        if (*rate != 0) {

            ALOGD("%s(requested rate: %d))", __FUNCTION__, *rate);
            // Always accept the rate provided by the client
            mSampleSpec.setSampleRate(*rate);
        }
        if ( (bad_rate) || (*rate == 0) ) {

            // No rate information was provided by the client
            // or set rate error
            // Use default HAL rate
            *rate = AudioHardwareALSA::DEFAULT_SAMPLE_RATE;
            mSampleSpec.setSampleRate(*rate);
        }
        ALOGD("%s: set rate to %d", __FUNCTION__, *rate);
    }

    if (format) {

        if (*format != 0) {

            ALOGD("%s(requested format: %d))", __FUNCTION__, *format);
            // Always accept the rate provided by the client
            // as far as this rate is supported
            if (*format != AUDIO_FORMAT_PCM_16_BIT && *format != AUDIO_FORMAT_PCM_8_24_BIT) {

                ALOGD("%s: format=(0x%x) not supported", __FUNCTION__, *format);
                bad_format = true;
            }

            mSampleSpec.setFormat(*format);
        }
        if ( (bad_format) || (*format == 0) ) {

            // No format provided or set format error
            // Use default HAL format
            *format = AudioHardwareALSA::DEFAULT_FORMAT;
            mSampleSpec.setFormat(*format);
        }
        ALOGD("%s : set format to %d (%d)", __FUNCTION__, *format, this->format());
    }


    if (bad_channels || bad_rate || bad_format) {

        return BAD_VALUE;
    }

    mHwSampleSpec = mSampleSpec;

    updateLatency();

    ALOGD("%s() -- OUT", __FUNCTION__);
    return NO_ERROR;
}

status_t ALSAStreamOps::setParameters(const String8 __UNUSED &keyValuePairs)
{
    return NO_ERROR;
}

String8 ALSAStreamOps::getParameters(const String8& keys)
{
    AudioParameter param = AudioParameter(keys);
    String8 value;
    String8 key = String8(AudioParameter::keyRouting);

    if (param.get(key, value) == NO_ERROR) {

        param.addInt(key, static_cast<int>(getCurrentDevices()));
    }

    LOGV("getParameters() %s", param.toString().string());
    return param.toString();
}

//
// Return the number of bytes (not frames)
// number of bytes returned takes sample rate into account
//
// @params: uiDivider: dividing factor of the latency of ringbuffer
//
size_t ALSAStreamOps::getBufferSize(uint32_t uiDivider) const
{
    ALOGD("%s: latency = %d divider = %d", __FUNCTION__, mLatencyUs, uiDivider);

    size_t size = mSampleSpec.convertUsecToframes(mLatencyUs) / uiDivider;

    size = CAudioUtils::alignOn16(size);

    size_t bytes = mSampleSpec.convertFramesToBytes(size);
    ALOGD("%s: %d (in bytes) for %s stream", __FUNCTION__, bytes, isOut() ? "output" : "input");

    return bytes;
}

uint32_t ALSAStreamOps::latency() const
{
    return CAudioUtils::convertUsecToMsec(mLatencyUs);
}

void ALSAStreamOps::updateLatency(uint32_t uiFlags)
{
    pcm_config pcmConf = mParent->getDefaultPcmConfig(isOut(), uiFlags);
    uint64_t latency = (uint64_t)CAudioUtils::USEC_TO_SEC * pcmConf.period_count *
                                            pcmConf.period_size  / pcmConf.rate;
    LOG_ALWAYS_FATAL_IF(latency > std::numeric_limits<uint32_t>::max());
    mLatencyUs = latency;
}

status_t ALSAStreamOps::setStandby(bool bIsSet)
{
    return bIsSet ? mParent->stopStream(this) : mParent->startStream(this);
}

//
// Route availability for a stream means a route has been
// associate with this stream...
//
bool ALSAStreamOps::isRouteAvailable() const
{
    return mCurrentRoute != NULL;
}

//
// Called from Route Manager Context -> WLocked
//
status_t ALSAStreamOps::attachRoute()
{
    ALOGD("%s %s stream", __FUNCTION__, isOut()? "output" : "input");

    CSampleSpec ssSrc;
    CSampleSpec ssDst;

    //
    // Set the new pcm device and sample spec given by the audio stream route
    //
    mHandle = mNewRoute->getPcmDevice(isOut());
    mHwSampleSpec = mNewRoute->getSampleSpec(isOut());

    ssSrc = isOut() ? mSampleSpec : mHwSampleSpec;
    ssDst = isOut() ? mHwSampleSpec : mSampleSpec;

    status_t err = configureAudioConversion(ssSrc, ssDst);
    if (err != NO_ERROR) {

        ALOGE("%s: could not initialize suitable audio conversion chain (err=%d)", __FUNCTION__, err);
        return err;
    }

    // Open successful - Update current route
    mCurrentRoute = mNewRoute;
    mCurrentDevices = mNewDevices;

    return NO_ERROR;
}

//
// Called from Route Manager Context -> WLocked
//
status_t ALSAStreamOps::detachRoute()
{
    ALOGD("%s %s stream", __FUNCTION__, isOut()? "output" : "input");

    // Clear current route pointer
    mCurrentRoute = NULL;
    mCurrentDevices = 0;
    mHandle = NULL;

    return NO_ERROR;
}

status_t ALSAStreamOps::configureAudioConversion(const CSampleSpec& ssSrc, const CSampleSpec& ssDst)
{
    return mAudioConversion->configure(ssSrc, ssDst);
}

status_t ALSAStreamOps::getConvertedBuffer(void* dst, const uint32_t outFrames, AudioBufferProvider *pBufferProvider)
{
    return mAudioConversion->getConvertedBuffer(dst, outFrames, pBufferProvider);
}

status_t ALSAStreamOps::applyAudioConversion(const void* src, void** dst, uint32_t inFrames, uint32_t* outFrames)
{
    return mAudioConversion->convert(src, dst, inFrames, outFrames);
}

//
// Called from Route Manager Context -> WLocked
//
// This function set the route pointer to the new route
// It also set the new PCM device
//
void ALSAStreamOps::setNewRoute(CAudioStreamRoute *pRoute)
{
    // No need to check Route, NULL pointer accepted
    // and considered as unrouting command
    mNewRoute = pRoute;
}

//
// Called from Route Manager Context -> WLocked
//
// reset the new route to NULL
//
void ALSAStreamOps::resetRoute()
{
    mNewRoute = NULL;
}

void ALSAStreamOps::setNewDevices(uint32_t uiNewDevices)
{
    mNewDevices = uiNewDevices;
}

void ALSAStreamOps::setCurrentDevices(uint32_t uiCurrentDevices)
{
    mCurrentDevices = uiCurrentDevices;
}

//
// Called from locked context
//
bool ALSAStreamOps::isStarted()
{
    return !mStandby;
}

//
// Called from locked context
//
void ALSAStreamOps::setStarted(bool bIsStarted)
{
    mStandby = !bIsStarted;
}

}       // namespace android
