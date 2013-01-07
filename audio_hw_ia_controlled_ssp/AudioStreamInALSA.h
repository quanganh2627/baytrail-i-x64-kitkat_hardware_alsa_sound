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

using namespace std;
using namespace android;

namespace android_audio_legacy
{

class AudioStreamInALSA : public AudioStreamIn, public ALSAStreamOps,  public AudioBufferProvider
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
    virtual status_t    dump(int fd, const Vector<String16>& args);

    virtual status_t    setGain(float gain);

    virtual status_t    standby();

    virtual status_t    setParameters(const String8& keyValuePairs);

    virtual String8     getParameters(const String8& keys)
    {
        return ALSAStreamOps::getParameters(keys);
    }

    // Return the amount of input frames lost in the audio driver since the last call of this function.
    // Audio driver is expected to reset the value to 0 and restart counting upon returning the current value by this function call.
    // Such loss typically occurs when the user space process is blocked longer than the capacity of audio driver buffers.
    // Unit: the number of input audio frames
    virtual unsigned int  getInputFramesLost() const;

    virtual bool        isOut() const { return false; }

    status_t            open(int mode);
    status_t            close();
    virtual status_t addAudioEffect(effect_handle_t effect) { return NO_ERROR; };
    virtual status_t removeAudioEffect(effect_handle_t effect) { return NO_ERROR; };


    // From ALSAStreamOps: to perform extra open/close actions
    virtual status_t    doOpen();
    virtual status_t    doClose();

    virtual int         getInputSource() const { return mInputSource; }

    // From AudioBufferProvider
    virtual status_t getNextBuffer(AudioBufferProvider::Buffer* buffer, int64_t pts = kInvalidPTS);
    virtual void releaseBuffer(AudioBufferProvider::Buffer* buffer);

    virtual void        setInputSource(int iInputSource);

private:
    AudioStreamInALSA(const AudioStreamInALSA &);
    AudioStreamInALSA& operator = (const AudioStreamInALSA &);
    void                resetFramesLost();
    size_t              generateSilence(void *buffer, size_t bytes);

    ssize_t             readHwFrames(void* buffer, ssize_t frames);

    ssize_t             readFrames(void* buffer, ssize_t frames);

    void                freeAllocatedBuffers();

    status_t            allocateProcessingMemory(ssize_t frames);

    inline status_t     allocateHwBuffer();

    status_t            setAcousticParams(void *params);

    unsigned int        mFramesLost;
    AudioSystem::audio_in_acoustics mAcoustics;

    uint32_t            mInputSource;

    char* mHwBuffer;
    ssize_t mHwBufferSize;

    static const uint32_t HIGH_LATENCY_TO_BUFFER_INTERVAL_RATIO;
    static const uint32_t LOW_LATENCY_TO_BUFFER_INTERVAL_RATIO;
};

};        // namespace android
