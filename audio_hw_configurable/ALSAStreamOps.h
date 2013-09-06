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

#include "AudioBufferProvider.h"
#include "SampleSpec.h"
#include <utils/String8.h>
#include "Utils.h"

/**
 * For debug purposes only, property-driven (dynamic)
 */
#include "HALAudioDump.h"

namespace android_audio_legacy
{

class AudioHardwareALSA;
class CAudioStreamRoute;
class CAudioConversion;
struct acoustic_device_t;
struct alsa_handle_t;

class ALSAStreamOps
{
public:
    virtual            ~ALSAStreamOps();

    android::status_t   set(int* format, uint32_t* channels, uint32_t* rate);

    android::status_t   setParameters(const android::String8& keyValuePairs);
    android::String8    getParameters(const android::String8& keys);

    inline uint32_t     sampleRate() const { return mSampleSpec.getSampleRate(); }

    /**
     * Get the buffer size to help upper layer to calibrate the transfer with Audio HAL.
     * Number of bytes returned takes stream sample rate into account
     * Ensure also the buffer is a multiple of 16 frames (AudioFlinger requirement).
     *
     * @param[in] flags: outputFlags for output stream, inputSource for input stream.
     *
     * @return size of a period in bytes.
     */
    size_t              getBufferSize(uint32_t flags = 0) const;
    inline int          format() const { return mSampleSpec.getFormat(); }
    inline uint32_t     channelCount() const { return mSampleSpec.getChannelCount(); }
    inline uint32_t     channels() const { return mSampleSpec.getChannelMask(); }

    // From AudioStreamIn/Out: indicates if the stream has a route pointer
    bool                isRouteAvailable() const;
    // From
    virtual android::status_t    attachRoute();
    virtual android::status_t    detachRoute();

    android::status_t   setStandby(bool bIsSet);

    virtual bool        isOut() const = 0;

    void                setNewRoute(CAudioStreamRoute* attachRoute);

    /**
     * Check if the stream has a new route assigned.
     * A stream might be eligible for a stream route. If so, the new route pointer will be set.
     * This function will inform if the stream has been assigned or not to a route.
     *
     * @return true if the stream has already a route assigned to it.
     */
    inline bool         isRouteAssignedToStream() const { return mNewRoute != NULL; }
    void                resetRoute();

    uint32_t            getNewDevices() const { return mNewDevices; }
    void                setNewDevices(uint32_t uiNewDevices);
    uint32_t            getCurrentDevices() const { return mCurrentDevices; }
    void                setCurrentDevices(uint32_t uiCurrentDevices);
    CAudioStreamRoute*  getCurrentRoute() const { return mCurrentRoute; }

    /**
     * Get the current stream state
     *
     * @return boolean indicating the stream state (true=playing, false=standby|stopped)
     */
    bool                isStarted();

    /**
     * Set the current stream state
     *
     * @param[in] isStarted boolean used to set the stream state
     *            (true=playing, false=standby|stopped)
     */
    void                setStarted(bool isStarted);

    /** Applicability mask.
     * It depends on the direction of the stream.
     * @return applicability Mask
     */
    virtual uint32_t    getApplicabilityMask() const = 0;

    /**
     * Get audio dump object before conversion for debug purposes
     *
     * @return a CHALAudioDump object before conversion
     */
    CHALAudioDump *getDumpObjectBeforeConv() const;


    /**
     * Get audio dump objects after conversion for debug purposes
     *
     * @return a CHALAudioDump object after conversion
     */
    CHALAudioDump *getDumpObjectAfterConv() const;

protected:
    ALSAStreamOps(AudioHardwareALSA* parent, const char* pcLockTag);
    friend class AudioHardwareALSA;
    ALSAStreamOps(const ALSAStreamOps &);
    ALSAStreamOps& operator = (const ALSAStreamOps &);

    android::status_t applyAudioConversion(const void* src, void** dst, uint32_t inFrames, uint32_t* outFrames);
    android::status_t getConvertedBuffer(void* dst, const uint32_t outFrames, android::AudioBufferProvider* pBufferProvider);

    uint32_t            latency() const;
    void                updateLatency(uint32_t uiFlags = 0);

    /**
     * Init audio dump if dump properties are activated to create the dump object(s).
     * Triggered when the stream is started.
     */
    void                initAudioDump();

    AudioHardwareALSA*      mParent;
    pcm*                    mHandle;

    bool                    mStandby;
    uint32_t                mDevices;
    CSampleSpec             mSampleSpec;
    CSampleSpec             mHwSampleSpec;

    /**
     * Audio dump object used if one of the dump property before
     * conversion is true (check init.rc file)
     */
    CHALAudioDump         *dumpBeforeConv;

    /**
     * Audio dump object used if one of the dump property after
     * conversion is true (check init.rc file)
     */
    CHALAudioDump         *dumpAfterConv;

private:
    // Configure the audio conversion chain
    android::status_t configureAudioConversion(const CSampleSpec& ssSrc, const CSampleSpec& ssDst);

    int         headsetPmDownDelay;
    int         speakerPmDownDelay;
    int         voicePmDownDelay;

    bool        mIsReset;
    CAudioStreamRoute*       mCurrentRoute;
    CAudioStreamRoute*       mNewRoute;

    uint32_t                mCurrentDevices;
    uint32_t                mNewDevices;

    uint32_t                mLatencyUs;

    bool                    mPowerLock;
    const char*             mPowerLockTag;

    // Audio Conversion utility class
    CAudioConversion* mAudioConversion;

    static const uint32_t STR_FORMAT_LENGTH;

    /**
     * Array of property names before conversion
     */
    static const std::string dumpBeforeConvProps[CUtils::ENbDirections];


    /**
     * Array of property names after conversion
     */
    static const std::string dumpAfterConvProps[CUtils::ENbDirections];
};

};        // namespace android
