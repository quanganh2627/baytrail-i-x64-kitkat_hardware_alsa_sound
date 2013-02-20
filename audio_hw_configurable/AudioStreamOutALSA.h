/* AudioStreamOutALSA.h
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

namespace android_audio_legacy
{

class AudioStreamOutALSA : public AudioStreamOut, public ALSAStreamOps
{
public:
    AudioStreamOutALSA(AudioHardwareALSA *parent);
    virtual            ~AudioStreamOutALSA();

    virtual uint32_t    sampleRate() const
    {
        return ALSAStreamOps::sampleRate();
    }

    virtual size_t      bufferSize() const;

    virtual uint32_t    channels() const;

    virtual int         format() const
    {
        return ALSAStreamOps::format();
    }

    virtual uint32_t    latency() const;

    virtual ssize_t     write(const void* buffer, size_t bytes);
    virtual status_t    dump(int fd, const Vector<String16>& args);

    status_t            setVolume(float left, float right);

    virtual status_t    standby();

    virtual status_t    setParameters(const String8& keyValuePairs);

    virtual String8     getParameters(const String8& keys)
    {
        return ALSAStreamOps::getParameters(keys);
    }

    // return the number of audio frames written by the audio dsp to DAC since
    // the output has exited standby
    virtual status_t    getRenderPosition(uint32_t* dspFrames);

    virtual bool        isOut() const { return true; }

    status_t            open(int mode);
    status_t            close();

    // From ALSAStreamOps - specific output stream routing actions
    virtual status_t    attachRoute();

    virtual status_t    flush();

    uint32_t            getFlags() const { return mFlags; }
    void                setFlags(uint32_t uiFlags);

    /* Applicability mask.
     * For an output stream, applicability mask is the output flags
     * @return stream flags
     */
    virtual uint32_t    getApplicabilityMask() const { return getFlags(); }

private:
    AudioStreamOutALSA(const AudioStreamOutALSA&);
    AudioStreamOutALSA& operator = (const AudioStreamOutALSA&);

    size_t              generateSilence(size_t bytes);

    ssize_t             writeFrames(void* buffer, ssize_t frames);

    void                updatePeriodTime();

    uint32_t            mFrameCount;


    uint32_t            mFlags;

    static const uint32_t MAX_AGAIN_RETRY;
    static const uint32_t WAIT_TIME_MS;
    static const uint32_t WAIT_BEFORE_RETRY_US;
    static const uint32_t LATENCY_TO_BUFFER_INTERVAL_RATIO;
    static const uint32_t USEC_PER_MSEC;
    static const uint32_t DEEP_PLAYBACK_PERIOD_TIME_US;
    static const uint32_t PLAYBACK_PERIOD_TIME_US;
};

};        // namespace android
