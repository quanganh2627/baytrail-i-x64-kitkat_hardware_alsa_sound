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
#include "AudioConverter.h"
#include "AudioResampler.h"
#include "AudioReformatter.h"
#include "AudioRemapper.h"
#include "AudioConversion.h"

#define DEVICE_OUT_BLUETOOTH_SCO_ALL (AudioSystem::DEVICE_OUT_BLUETOOTH_SCO | AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET | AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT)

#define USEC_PER_SEC             (1000000)

using namespace android;

namespace android_audio_legacy
{

// ----------------------------------------------------------------------------

static const char* heasetPmDownDelaySysFile = "//sys//devices//ipc//msic_audio//Medfield Headset//pmdown_time";
static const char* speakerPmDownDelaySysFile = "//sys//devices//ipc//msic_audio//Medfield Speaker//pmdown_time";
static const char* voicePmDownDelaySysFile = "//sys//devices//ipc//msic_audio//Medfield Voice//pmdown_time";

ALSAStreamOps::ALSAStreamOps(AudioHardwareALSA *parent, alsa_handle_t *handle, const char* pcLockTag) :
    mParent(parent),
    mHandle(handle),
    mStandby(false),
    mDevices(0),
    isResetted(false),
    mAudioRoute(NULL),
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
    AutoW lock(mParent->mLock);

    close();

    delete mAudioConversion;
}

acoustic_device_t *ALSAStreamOps::acoustics()
{
    return mParent->getAcousticHwDevice();
}
vpc_device_t *ALSAStreamOps::vpc()
{
   return mParent->getVpcHwDevice();
}

ALSAMixer *ALSAStreamOps::mixer()
{
    return mParent->mMixer;
}

status_t ALSAStreamOps::set(int      *format,
                            uint32_t *channels,
                            uint32_t *rate)
{
    bool bad_channels = false;
    bool bad_rate = false;
    bool bad_format = false;

    LOGD("%s(format:%d channels:0x%x (popCount returns %d) rate:%d) and mHandle->sampleRate=%d, mHandle->channels=%d",
         __FUNCTION__, *format, *channels, CAudioUtils::popCount(*channels), *rate, mHandle->sampleRate, mHandle->channels);

    if (channels) {

        if (*channels != 0) {

            // Always accept the channels requested by the client
            // as far as the channel count is supported
            mSampleSpec.setChannelMask(*channels);

            if (CAudioUtils::popCount(*channels) > 2) {

                LOGD("%s: channels=(0x%x, %d) not supported", __FUNCTION__, *channels, CAudioUtils::popCount(*channels));
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
        LOGD("%s: set channels to 0x%x", __FUNCTION__, *channels);

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
        LOGD("%s: set rate to %d", __FUNCTION__, *rate);
    }

    if (format) {

        if (*format != 0) {

            // Always accept the rate provided by the client
            // as far as this rate is supported
            if (*format != AUDIO_FORMAT_PCM_16_BIT && *format != AUDIO_FORMAT_PCM_8_24_BIT) {

                LOGD("%s: format=(0x%x) not supported", __FUNCTION__, *format);
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
        LOGD("%s : set format to %d", __FUNCTION__, *format);
    }

    return (bad_channels || bad_rate || bad_format) ? BAD_VALUE : NO_ERROR;
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

//
// Return the number of bytes (not frames)
//
size_t ALSAStreamOps::bufferSize(int32_t iInterval) const
{
    size_t size = (int64_t) iInterval * sampleRate() / USEC_PER_SEC;

    // take resampling into account and return the closest majoring
    // multiple of 16 frames, as audioflinger expects audio buffers to
    // be a multiple of 16 frames.
    size = CAudioUtils::alignOn16(size);

    size_t bytes = mSampleSpec.convertFramesToBytes(size);
    LOGD("%s: %d (in bytes) for %s stream", __FUNCTION__, bytes, isOut() ? "output" : "input");

    return bytes;
}

void ALSAStreamOps::close()
{
    // Call unset stream from route (if still routed)
    if(mAudioRoute) {
        mAudioRoute->unsetStream(this, mHandle->curMode);
        mAudioRoute = NULL;
    }
}

void ALSAStreamOps::doClose()
{
    ALOGD("ALSAStreamOps::doClose");

    if(mHandle->handle)
    {
        //if BT SCO path is used in normal mode: disable bt sco path
        if(isBluetoothScoNormalInUse())
        {
            mParent->getVpcHwDevice()->set_bt_sco_path(VPC_ROUTE_CLOSE);
        }
    }

    if(mParent->getVpcHwDevice() && mParent->getVpcHwDevice()->mix_disable)
        mParent->getVpcHwDevice()->mix_disable(isOut());

    mParent->getAlsaHwDevice()->close(mHandle);

    releasePowerLock();
}

void ALSAStreamOps::doStandby()
{
    ALOGD("%s",__FUNCTION__);

    standby();
}

status_t ALSAStreamOps::standby()
{
    AutoW lock(mParent->mLock);
    ALOGD("%s",__FUNCTION__);

    if(mHandle->handle)
    {
        snd_pcm_drain (mHandle->handle);

        //if BT SCO path is used in normal mode: disable bt sco path
        if(isBluetoothScoNormalInUse())
        {
            mParent->getVpcHwDevice()->set_bt_sco_path(VPC_ROUTE_CLOSE);
        }
    }
    if(mParent->getVpcHwDevice() && mParent->getVpcHwDevice()->mix_disable)
        mParent->getVpcHwDevice()->mix_disable(isOut());



    if(mParent->getAlsaHwDevice() && mParent->getAlsaHwDevice()->standby)
        mParent->getAlsaHwDevice()->standby(mHandle);

    releasePowerLock();

    mStandby = true;

    return NO_ERROR;
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
status_t ALSAStreamOps::doOpen(uint32_t devices, int mode)
{
    assert(routeAvailable());
    assert(!mHandle->handle);

    status_t err = BAD_VALUE;
    err = mParent->getAlsaHwDevice()->open(mHandle, devices, mode, mParent->getFmRxMode());

    if(err == NO_ERROR)
    {
        //if BT SCO path is used in normal mode: enable bt sco path
        if(isBluetoothScoNormalInUse())
        {
            mParent->getVpcHwDevice()->set_bt_sco_path(VPC_ROUTE_OPEN);
        }

        mHwSampleSpec.setFormat(CAudioUtils::convertSndToHalFormat(mHandle->format));
        mHwSampleSpec.setSampleRate(mHandle->sampleRate);
        mHwSampleSpec.setChannelCount(mHandle->channels);

        CSampleSpec ssSrc = isOut() ? mSampleSpec : mHwSampleSpec;
        CSampleSpec ssDst = isOut() ? mHwSampleSpec : mSampleSpec;
        configureAudioConversion(ssSrc, ssDst);
    }
    LOGD("ALSAStreamOps::doOpen status=%d", err);
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
    if (mParent->getHaveAudience()) {
        // On board with Audience, both CSV and VoIP call are routed through VPC
        if (((mode == AudioSystem::MODE_IN_COMMUNICATION) || (mode == AudioSystem::MODE_IN_CALL)) &&
                isOut() &&
                mParent->getVpcHwDevice()) {
            ALOGD("%s: With Audience. current mode: MODE_IN_COMMUNICATION|MODE_IN_CALL ", __FUNCTION__);
            mParent->getVpcHwDevice()->route(VPC_ROUTE_OPEN);
        }
    } else {
        // On board without Audience, CSV on all accessories and VoIP on BT SCO are
        // routed through VPC
        if ((mode == AudioSystem::MODE_IN_CALL) &&
                isOut() &&
                mParent->getVpcHwDevice()) {
            ALOGD("%s: No audience: Current mode MODE_IN_CALL", __FUNCTION__);
            mParent->getVpcHwDevice()->route(VPC_ROUTE_OPEN);
        } else if ((mode == AudioSystem::MODE_IN_COMMUNICATION) &&
                   (devices & DEVICE_OUT_BLUETOOTH_SCO_ALL) &&
                   isOut() &&
                   mParent->getVpcHwDevice()){
            ALOGD("%s: No audience. Current mode MODE_IN_COMMUNICATION, current device: BT", __FUNCTION__);
            mParent->getVpcHwDevice()->set_bt_sco_path(VPC_ROUTE_OPEN);
        }
    }
}

void ALSAStreamOps::vpcUnroute(uint32_t curDev, int curMode)
{
    if (mParent->getHaveAudience()) {
        // On board with Audience, both CSV and VoIP call are unrouted through VPC
        if (((curMode == AudioSystem::MODE_IN_COMMUNICATION) || (curMode == AudioSystem::MODE_IN_CALL)) &&
                isOut() &&
                mParent->getVpcHwDevice()) {
            ALOGD("%s: With Audience. current mode: MODE_IN_COMMUNICATION|MODE_IN_CALL ", __FUNCTION__);
            mParent->getVpcHwDevice()->route(VPC_ROUTE_CLOSE);
        }
    } else {
        // On board without Audience, CSV on all accessories and VoIP on BT SCO are
        // routed unthrough VPC
        if ((curMode == AudioSystem::MODE_IN_CALL) &&
                isOut() &&
                mParent->getVpcHwDevice()) {
            ALOGD("%s: No audience: Current mode MODE_IN_CALL", __FUNCTION__);
            mParent->getVpcHwDevice()->route(VPC_ROUTE_CLOSE);
        } else if ((curMode == AudioSystem::MODE_IN_COMMUNICATION) &&
                   (curDev & DEVICE_OUT_BLUETOOTH_SCO_ALL) &&
                   isOut() &&
                   mParent->getVpcHwDevice()){
            ALOGD("%s: No audience. Current mode MODE_IN_COMMUNICATION, current device: BT", __FUNCTION__);
            mParent->getVpcHwDevice()->set_bt_sco_path(VPC_ROUTE_CLOSE);
        }
    }
}

status_t ALSAStreamOps::setRoute(AudioRoute *audioRoute, uint32_t devices, int mode)
{
    ALOGD("setRoute mode=%d", mode);
    if((mAudioRoute == audioRoute) && (mHandle->curDev == devices) && (mHandle->curMode == mode) &&
            (mParent->getFmRxMode() == mParent->getPrevFmRxMode()) &&
            !mParent->isReconsiderRoutingForced()) {
        ALOGD("setRoute: stream already attached to the route, identical conditions");
        return NO_ERROR;
    }

    bool restore_needed = false;
    if((mParent->getFmRxMode() != mParent->getPrevFmRxMode() ||
                (mHandle->curMode != mode &&
                 (mode == AudioSystem::MODE_IN_CALL ||
          mHandle->curMode == AudioSystem::MODE_IN_CALL))) &&
           isOut()) {
        storeAndResetPmDownDelay();
        restore_needed = true;
    }

    // unset stream from previous route (if any)
    if(mAudioRoute != NULL) {
        // unset the streams in the mode in which streams
        // were running until now (usefull for tied attribute)...
        mAudioRoute->unsetStream(this, mHandle->curMode);
    }

    mAudioRoute = audioRoute;
    mDevices = devices;

    // SetRoute is called with a NULL route, it only expects for an "unset" of the route
    if(!mAudioRoute)
        return NO_ERROR;

    // set stream to new route
    int ret = mAudioRoute->setStream(this, mode);

    if (ret != NO_ERROR) {

      ALOGD("%s: error while routing the stream..., ret = %d", __FUNCTION__, ret);
        // Error while routing the stream to its route, unset it!!!
        mAudioRoute->unsetStream(this, mode);
        mAudioRoute = NULL;
    }

    if(restore_needed) {
        restorePmDownDelay();
    }

    return ret;
}

status_t ALSAStreamOps::doRoute(int mode)
{
    status_t err = NO_ERROR;
    ALOGD("doRoute mode=%d", mode);

    vpcRoute(mDevices, mode);

    err = doOpen(mDevices, mode);
    if (err < 0) {
        LOGE("Cannot open alsa device(0x%x) in mode (%d)", mDevices, mode);
        return err;
    }

    if(mStandby) {
        ALOGD("doRoute standby mode -> standby the device immediately after routing");
        doStandby();
    }
    return NO_ERROR;
}

status_t ALSAStreamOps::undoRoute()
{
    ALOGD("doRoute undoRoute");

    int curMode = mHandle->curMode;
    int curDev = mHandle->curDev;
    doClose();

    vpcUnroute(curDev, curMode);

    return NO_ERROR;
}

int ALSAStreamOps::readSysEntry(const char* filePath)
{
    int fd;
    if ((fd = ::open(filePath, O_RDONLY)) < 0)
    {
        LOGE("Could not open file %s", filePath);
        return 0;
    }
    char buff[20];
    int val = 0;
    uint32_t count;
    if( (count = ::read(fd, buff, sizeof(buff)-1)) < 1 )
    {
        LOGE("Could not read file");
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
        LOGE("Could not open file %s", filePath);
        return ;
    }
    char buff[20];
    uint32_t count;
    snprintf(buff, sizeof(buff), "%d", value);
    if ((count = ::write(fd, buff, strlen(buff))) != strlen(buff))
        LOGE("could not write ret=%d", count);
    else
        ALOGD("written %d bytes = %s", count, buff);
    ::close(fd);
}

void ALSAStreamOps::storeAndResetPmDownDelay()
{
    ALOGD("storeAndResetPmDownDelay");
    if (!isResetted)
    {
        ALOGD("storeAndResetPmDownDelay not resetted");
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
    ALOGD("restorePmDownDelay");
    if(isResetted)
    {
        ALOGD("restorePmDownDelay restoring");
        writeSysEntry(heasetPmDownDelaySysFile, headsetPmDownDelay);
        writeSysEntry(speakerPmDownDelaySysFile, speakerPmDownDelay);
        writeSysEntry(voicePmDownDelaySysFile, voicePmDownDelay);

        isResetted = false;
    }
    else
        ALOGD("restorePmDownDelay -> nothing to do");
}

bool ALSAStreamOps::isDeviceBluetoothSCO(uint32_t devices)
{
    if(isOut())
    {
        return (bool)(devices & DEVICE_OUT_BLUETOOTH_SCO_ALL);
    }
    else
    {
        return (devices == AudioSystem::DEVICE_IN_BLUETOOTH_SCO_HEADSET);
    }
}

bool ALSAStreamOps::isBluetoothScoNormalInUse()
{
    return (mParent->getVpcHwDevice() && mParent->getVpcHwDevice()->set_bt_sco_path &&
            isDeviceBluetoothSCO(mHandle->curDev) && (mHandle->curMode == AudioSystem::MODE_NORMAL));
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

void ALSAStreamOps::configureAudioConversion(const CSampleSpec& ssSrc, const CSampleSpec& ssDst)
{
    mAudioConversion->configure(ssSrc, ssDst);
}

status_t ALSAStreamOps::getConvertedBuffer(void* dst, const uint32_t outFrames, AudioBufferProvider *pBufferProvider)
{
    return mAudioConversion->getConvertedBuffer(dst, outFrames, pBufferProvider);
}

status_t ALSAStreamOps::applyAudioConversion(const void* src, void** dst, uint32_t inFrames, uint32_t* outFrames)
{
    return mAudioConversion->convert(src, dst, inFrames, outFrames);
}

}       // namespace android
