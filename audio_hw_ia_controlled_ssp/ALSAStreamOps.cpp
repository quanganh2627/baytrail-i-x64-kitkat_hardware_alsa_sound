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

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "ALSAStreamOps"
//#define LOG_NDEBUG 0
#include <utils/Log.h>
#include <utils/String8.h>

#include <cutils/properties.h>
#include <media/AudioRecord.h>
#include <hardware_legacy/power.h>

#include "ALSAStreamOps.h"
#include "AudioStreamRoute.h"
#include "AudioConverter.h"
#include "AudioResampler.h"
#include "AudioReformatter.h"
#include "AudioRemapper.h"
#include "AudioConversion.h"

#define DEVICE_OUT_BLUETOOTH_SCO_ALL (AudioSystem::DEVICE_OUT_BLUETOOTH_SCO | AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET | AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT)

#define USEC_PER_SEC            (1000000)
#define USEC_PER_MSEC           (1000)

using namespace android;

namespace android_audio_legacy
{

// ----------------------------------------------------------------------------

static const char* heasetPmDownDelaySysFile = "//sys//devices//ipc//msic_audio//Medfield Headset//pmdown_time";
static const char* speakerPmDownDelaySysFile = "//sys//devices//ipc//msic_audio//Medfield Speaker//pmdown_time";
static const char* voicePmDownDelaySysFile = "//sys//devices//ipc//msic_audio//Medfield Voice//pmdown_time";

ALSAStreamOps::ALSAStreamOps(AudioHardwareALSA *parent, const char* pcLockTag) :
    mParent(parent),
    mHandle(new alsa_handle_t),
    mStandby(true),
    mDevices(0),
    mFlags(AUDIO_OUTPUT_FLAG_NONE),
    isResetted(false),
    mCurrentRoute(NULL),
    mNewRoute(NULL),
    mCurrentDevices(0),
    mNewDevices(0),
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
    ALSAStreamOps::setStandby(true);

    delete mHandle;

    delete mAudioConversion;
}

acoustic_device_t *ALSAStreamOps::acoustics()
{
    return NULL;
}

status_t ALSAStreamOps::set(int      *format,
                            uint32_t *channels,
                            uint32_t *rate)
{
    bool bad_channels = false;
    bool bad_rate = false;
    bool bad_format = false;

    ALOGD("%s(format:%d channels:0x%x (popCount returns %d) rate:%d)",
          __FUNCTION__,
          (format? *format : 0),
          (channels? *channels : 0),
          (*channels? CAudioUtils::popCount(*channels) : 0),
          (*rate? *rate : 0));

    if (channels) {

        if (*channels != 0) {

            // Always accept the channels requested by the client
            // as far as the channel count is supported
            mSampleSpec.setChannelMask(*channels);

            if (CAudioUtils::popCount(*channels) > 2) {

                ALOGD("%s: channels=(0x%x, %d) not supported", __FUNCTION__, *channels, CAudioUtils::popCount(*channels));
                bad_channels = true;
            }
        }
        if ( (bad_channels) || (*channels == 0) ) {

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
        mSampleSpec.setChannelCount(CAudioUtils::popCount(mSampleSpec.getChannelMask()));
    }

    if (rate) {

        if (*rate != 0) {

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
        ALOGD("%s : set format to %d", __FUNCTION__, *format);
    }

    status_t status = (bad_channels || bad_rate || bad_format) ? BAD_VALUE : NO_ERROR;

    if (status == NO_ERROR) {

        if (mParent->getAlsaHwDevice()) {

            mParent->getAlsaHwDevice()->initStream(mHandle,
                                                   isOut(),
                                                   mSampleSpec.getSampleRate(),
                                                   mSampleSpec.getChannelCount(),
                                                   CAudioUtils::convertHalToTinyFormat(mSampleSpec.getFormat()));
            mHwSampleSpec = mSampleSpec;
        }
    }
    return status;
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
        param.addInt(key, (int)getCurrentDevice());
    }

    LOGV("getParameters() %s", param.toString().string());
    return param.toString();
}

//
// Return the number of bytes (not frames)
// number of bytes returned takes sample rate into account
//
size_t ALSAStreamOps::bufferSize() const
{
    size_t bytes;
    size_t size;

    ALOGD("%s: %d %d", __FUNCTION__, mHandle->config.period_size, mHandle->config.period_count);
    size = CAudioUtils::convertSrcToDstInFrames(mHandle->config.period_size *  mHandle->config.period_count / 4, mHwSampleSpec, mSampleSpec);

    size = CAudioUtils::alignOn16(size);

    bytes = mSampleSpec.convertFramesToBytes(size);
    ALOGD("%s: %d (in bytes) for %s stream", __FUNCTION__, bytes, isOut() ? "output" : "input");

    return bytes;
}

uint32_t ALSAStreamOps::latency() const
{
    // Android wants latency in milliseconds.

    int latency = mHwSampleSpec.framesToMs(mHandle->config.period_size *  mHandle->config.period_count);
    return latency / USEC_PER_MSEC;
}

status_t ALSAStreamOps::setStandby(bool bIsSet)
{
    status_t status = NO_ERROR;

    if (bIsSet) {
        if (isStarted()) {

            status = mParent->stopStream(this);
        }
    } else {
        if (!isStarted()) {

            status = mParent->startStream(this);
        }
    }
    return status;
}

//
// Route availability for a stream means a route has been
// associate with this stream...
//
bool ALSAStreamOps::isRouteAvailable()
{
    return !!mCurrentRoute;
}

//
// Called from Route Manager Context -> WLocked
//
status_t ALSAStreamOps::doOpen()
{
    ALOGD("%s", __FUNCTION__);

    status_t err = NO_ERROR;
    CSampleSpec ssSrc;
    CSampleSpec ssDst;

    assert(!mHandle->handle);

    acquirePowerLock();

    err = mParent->getAlsaHwDevice()->open(mHandle,
                                           mNewRoute->getCardId(),
                                           mNewRoute->getPcmDeviceId(isOut()),
                                           mNewRoute->getPcmConfig(isOut()));
    if (err != NO_ERROR) {

        ALOGE("%s: Cannot open tinyalsa (%d,%d) device for %s stream", __FUNCTION__,
                                                                       mNewRoute->getCardId(),
                                                                       mNewRoute->getPcmDeviceId(isOut()),
                                                                       isOut()? "output" : "input");

        goto fail_open;
    }

    //
    // Set the new HW sample spec given by the audio route borrowed
    //
    mHwSampleSpec.setFormat(CAudioUtils::convertTinyToHalFormat(mHandle->config.format));
    mHwSampleSpec.setSampleRate(mHandle->config.rate);
    mHwSampleSpec.setChannelCount(mHandle->config.channels);

    ssSrc = isOut() ? mSampleSpec : mHwSampleSpec;
    ssDst = isOut() ? mHwSampleSpec : mSampleSpec;

    err = configureAudioConversion(ssSrc, ssDst);
    if (err != NO_ERROR) {

        ALOGE("%s: could not initialize suitable audio conversion chain (err=%d)", __FUNCTION__, err);
        goto fail_open;
    }

    // Open successful - Update current route
    mCurrentRoute = mNewRoute;

    return NO_ERROR;

fail_open:
    releasePowerLock();
    return err;
}

//
// Called from Route Manager Context -> WLocked
//
status_t ALSAStreamOps::doClose()
{
    ALOGD("%s %s stream", __FUNCTION__, isOut()? "output" : "input");

    assert(mHandle->handle);

    mParent->getAlsaHwDevice()->close(mHandle);

    releasePowerLock();

    // Clear current route pointer
    mCurrentRoute = NULL;

    return NO_ERROR;
}

int ALSAStreamOps::readSysEntry(const char* filePath)
{
    int fd;
    if ((fd = ::open(filePath, O_RDONLY)) < 0)
    {
        ALOGE("Could not open file %s", filePath);
        return 0;
    }
    char buff[20];
    int val = 0;
    uint32_t count;
    if( (count = ::read(fd, buff, sizeof(buff)-1)) < 1 )
    {
        ALOGE("Could not read file");
    }
    else
    {
        // Zero terminate string
        buff[count] = '\0';
        ALOGD("read %d bytes = %s", count, buff);
        val = strtol(buff, NULL, 0);
    }

    ::close(fd);
    return val;
}

void ALSAStreamOps::writeSysEntry(const char* filePath, int value)
{
    int fd;

    if ((fd = ::open(filePath, O_WRONLY)) < 0)
    {
        ALOGE("Could not open file %s", filePath);
        return ;
    }
    char buff[20];
    uint32_t count;
    snprintf(buff, sizeof(buff), "%d", value);
    if ((count = ::write(fd, buff, strlen(buff))) != strlen(buff))
        ALOGE("could not write ret=%d", count);
    else
        ALOGD("written %d bytes = %s", count, buff);
    ::close(fd);
}

void ALSAStreamOps::storeAndResetPmDownDelay()
{
    ALOGD("%s", __FUNCTION__);

    if (!isResetted) {

        LOGD("storeAndResetPmDownDelay not resetted");
        headsetPmDownDelay = readSysEntry(heasetPmDownDelaySysFile);
        speakerPmDownDelay = readSysEntry(speakerPmDownDelaySysFile);
        voicePmDownDelay = readSysEntry(voicePmDownDelaySysFile);

        writeSysEntry(heasetPmDownDelaySysFile, 0);
        writeSysEntry(speakerPmDownDelaySysFile, 0);
        writeSysEntry(voicePmDownDelaySysFile, 0);

        isResetted = true;
    }
    else
        ALOGD("storeAndResetPmDownDelay already resetted -> nothing to do");
}

void ALSAStreamOps::restorePmDownDelay()
{
    ALOGD("%s", __FUNCTION__);

    if (isResetted) {

        ALOGD("restorePmDownDelay restoring");
        writeSysEntry(heasetPmDownDelaySysFile, headsetPmDownDelay);
        writeSysEntry(speakerPmDownDelaySysFile, speakerPmDownDelay);
        writeSysEntry(voicePmDownDelaySysFile, voicePmDownDelay);

        isResetted = false;
    }
    else
        ALOGD("restorePmDownDelay -> nothing to do");
}

void ALSAStreamOps::acquirePowerLock()
{
    if (!mPowerLock) {

        acquire_wake_lock (PARTIAL_WAKE_LOCK, mPowerLockTag);
        mPowerLock = true;
    }
}

void ALSAStreamOps::releasePowerLock()
{
    if (mPowerLock) {

        release_wake_lock (mPowerLockTag);
        mPowerLock = false;
    }
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

void ALSAStreamOps::setNewDevice(uint32_t uiNewDevice)
{
    mNewDevices = uiNewDevice;
}

void ALSAStreamOps::setCurrentDevice(uint32_t uiCurrentDevice)
{
    mCurrentDevices = uiCurrentDevice;
}

void ALSAStreamOps::setFlags(audio_output_flags_t uiFlags)
{
    mFlags = uiFlags;
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
