/* ALSAStreamOps.h
 **
 ** Copyright 2008-2009, Wind River Systems
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

#pragma once

#include "AudioHardwareALSA.h"
#include "AudioBufferProvider.h"

using namespace android;


namespace android_audio_legacy
{

class ALSAStreamOps
{
public:
    ALSAStreamOps(AudioHardwareALSA *parent, const char* pcLockTag);
    virtual            ~ALSAStreamOps();

    status_t            set(int *format, uint32_t *channels, uint32_t *rate);

    status_t            setParameters(const String8& keyValuePairs);
    String8             getParameters(const String8& keys);

    inline uint32_t     sampleRate() const { return mSampleSpec.getSampleRate();}
    size_t              bufferSize() const;
    inline int          format() const {return mSampleSpec.getFormat();}
    inline uint32_t     channelCount() const {return mSampleSpec.getChannelCount();}
    inline uint32_t     channels() const {return mSampleSpec.getChannelMask();}

    // From AudioStreamIn/Out: indicates if the stream has a route pointer
    bool                routeAvailable();
    // From
    virtual status_t    doOpen();
    virtual status_t    doClose();
    status_t            setStandby(bool bIsSet);

    virtual bool        isOut() const = 0 ;


    void                setNewRoute(CAudioStreamRoute *route);
    void                resetRoute();

    uint32_t            getNewDevice();
    void                setNewDevice(uint32_t newDevice);
    uint32_t            getCurrentDevice();
    int                 getNewMode();
    void                setNewMode(int newMode);

    // Get the current stream state (true=playing, false=standby|stopped)
    bool                isStarted();
    void                setStarted(bool bIsStarted);

protected:
    friend class AudioHardwareALSA;
    ALSAStreamOps(const ALSAStreamOps &);
    ALSAStreamOps& operator = (const ALSAStreamOps &);

    void                acquirePowerLock();
    void                releasePowerLock();

    vpc_device_t *vpc();
    acoustic_device_t *acoustics();

    status_t            applyAudioConversion(const void* src, void** dst, uint32_t inFrames, uint32_t* outFrames);
    status_t            getConvertedBuffer(void *dst, const uint32_t outFrames, AudioBufferProvider* pBufferProvider);

    uint32_t            latency() const;

    AudioHardwareALSA *     mParent;
    alsa_handle_t *         mHandle;

    Mutex                   mLock;
    bool                    mStandby;
    uint32_t                mDevices;
    CSampleSpec             mSampleSpec;
    CSampleSpec             mHwSampleSpec;

private:
    void        storeAndResetPmDownDelay();
    void        restorePmDownDelay();

    void        writeSysEntry(const char* filePath, int value);
    int         readSysEntry(const char* filePath);

    // Configure the audio conversion chain
    status_t configureAudioConversion(const CSampleSpec& ssSrc, const CSampleSpec& ssDst);

    int         headsetPmDownDelay;
    int         speakerPmDownDelay;
    int         voicePmDownDelay;

    bool        isResetted;
    CAudioStreamRoute*       mCurrentRoute;
    CAudioStreamRoute*       mNewRoute;

    bool                    mPowerLock;
    const char*             mPowerLockTag;

    // Audio Conversion utility class
    CAudioConversion* mAudioConversion;
};



};        // namespace android
