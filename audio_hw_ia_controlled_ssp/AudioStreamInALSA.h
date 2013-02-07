/* AudioStreamInALSA.h
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
#include "ALSAStreamOps.h"
#include "AudioBufferProvider.h"


namespace android_audio_legacy
{

class AudioStreamInALSA : public AudioStreamIn, public ALSAStreamOps,  public android::AudioBufferProvider
{
public:
    AudioStreamInALSA(AudioHardwareALSA *parent,
                      AudioSystem::audio_in_acoustics audio_acoustics);
    virtual            ~AudioStreamInALSA();

    virtual uint32_t    sampleRate() const
    {
        return ALSAStreamOps::sampleRate();
    }

    virtual size_t      bufferSize() const;

    virtual uint32_t    channels() const
    {
        return ALSAStreamOps::channels();
    }

    virtual int         format() const
    {
        return ALSAStreamOps::format();
    }

    virtual ssize_t     read(void* buffer, ssize_t bytes);
    virtual android::status_t    dump(int fd, const android::Vector<String16>& args);

    virtual android::status_t    setGain(float gain);

    virtual android::status_t    standby();

    virtual android::status_t    setParameters(const android::String8& keyValuePairs);

    virtual android::String8     getParameters(const android::String8& keys)
    {
        return ALSAStreamOps::getParameters(keys);
    }

    // Return the amount of input frames lost in the audio driver since the last call of this function.
    // Audio driver is expected to reset the value to 0 and restart counting upon returning the current value by this function call.
    // Such loss typically occurs when the user space process is blocked longer than the capacity of audio driver buffers.
    // Unit: the number of input audio frames
    virtual unsigned int  getInputFramesLost() const;

    virtual bool        isOut() const { return false; }

    android::status_t            open(int mode);
    android::status_t            close();
    virtual android::status_t addAudioEffect(effect_handle_t __UNUSED effect) { return NO_ERROR; }
    virtual android::status_t removeAudioEffect(effect_handle_t __UNUSED effect) { return NO_ERROR; }

    // From ALSAStreamOps: to perform extra open/close actions
    virtual android::status_t    attachRoute();
    virtual android::status_t    detachRoute();

    // From AudioBufferProvider
    virtual android::status_t getNextBuffer(android::AudioBufferProvider::Buffer* buffer, int64_t pts = kInvalidPTS);
    virtual void releaseBuffer(android::AudioBufferProvider::Buffer* buffer);

    int                 getInputSource() const { return mInputSource; }
    void                setInputSource(int iInputSource);

    /* Applicability mask.
     * For an input stream, applicability mask is the ID of the input source
     * @return ID of input source
     */
    virtual uint32_t    getApplicabilityMask() const { return 1 << getInputSource(); }

private:
    AudioStreamInALSA(const AudioStreamInALSA &);
    AudioStreamInALSA& operator = (const AudioStreamInALSA &);
    void                resetFramesLost();
    size_t              generateSilence(void* buffer, size_t bytes);

    ssize_t             readHwFrames(void* buffer, size_t frames);

    ssize_t             readFrames(void* buffer, size_t frames);

    void                freeAllocatedBuffers();

    android::status_t   allocateProcessingMemory(size_t frames);

    inline android::status_t     allocateHwBuffer();

    unsigned int        mFramesLost;
    AudioSystem::audio_in_acoustics mAcoustics;

    uint32_t            mInputSource;

    char* mHwBuffer;
    ssize_t mHwBufferSize;

    static const uint32_t HIGH_LATENCY_TO_BUFFER_INTERVAL_RATIO;
    static const uint32_t LOW_LATENCY_TO_BUFFER_INTERVAL_RATIO;
    static const uint32_t CAPTURE_PERIOD_TIME_US;
};

};        // namespace android
