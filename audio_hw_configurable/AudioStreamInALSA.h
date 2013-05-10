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
    typedef std::list<effect_handle_t>::iterator AudioEffectsListIterator;

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

    /**
     * Allows Flinger to request to add an effect on the stream.
     * Adds an audio effect on the input stream chain according to
     * the audio_effect.conf file.
     * Limited to Echo Cancellation, Noise Suppression and Automatic Gain Control.
     *
     * @param[in] structure of the effect to add.
     *
     * @return status_t OK upon succes, error code otherwise.
     */
    virtual status_t addAudioEffect(effect_handle_t effect);

    /**
     * Allows Flinger to request to remove an effect from the stream.
     * Removes an audio effect on the input stream chain.
     * Limited to Echo Cancellation, Noise Suppression and Automatic Gain Control.
     *
     * @param[in] structure of the effect to remove.
     *
     * @return status_t OK upon succes, error code otherwise.
     */
    virtual status_t removeAudioEffect(effect_handle_t effect);

    /**
     * Request to add effect on the stream in routing locked context.
     * Called by the route manager with routing lock handled.
     * It adds an audio effect request on the input stream.
     *
     * @param[in] structure of the effect to add.
     *
     * @return status_t OK upon succes, error code otherwise.
     */
    virtual status_t addAudioEffectRequest(effect_handle_t effect);

    /**
     * Request to remove an effect from the stream in routing locked context.
     * Called by the route manager with routing lock handled.
     * It requests to remove an audio effect from the input stream chain.
     *
     * @param[in] structure of the effect to add.
     *
     * @return status_t OK upon succes, error code otherwise.
     */
    virtual status_t removeAudioEffectRequest(effect_handle_t effect);

    /**
     * Add effect on the stream in routing locked context.
     * Called by the route manager with routing lock handled.
     * It adds an audio effect on the input stream.
     *
     * @param[in] structure of the effect to add.
     *
     * @return status_t OK upon succes, error code otherwise.
     */
    status_t addAudioEffect_l(effect_handle_t effect, struct echo_reference_itfe* reference = NULL);

    /**
     * Removes an effect from the stream in routing locked context.
     * Called by the route manager with routing lock handled.
     * It removes an audio effect from the input stream chain.
     *
     * @param[in] structure of the effect to add.
     *
     * @return status_t OK upon succes, error code otherwise.
     */
    status_t removeAudioEffect_l(effect_handle_t effect);

    // From ALSAStreamOps: to perform extra open/close actions
    virtual android::status_t    attachRoute();
    virtual android::status_t    detachRoute();

    // From AudioBufferProvider
    virtual android::status_t getNextBuffer(android::AudioBufferProvider::Buffer* buffer, int64_t pts = kInvalidPTS);
    virtual void releaseBuffer(android::AudioBufferProvider::Buffer* buffer);

    int                 getInputSource() const { return mInputSource; }
    void                setInputSource(int iInputSource);

    /**
     * Applicability mask.
     * For an input stream, applicability mask is the ID of the input source
     * @return ID of input source
     */
    virtual uint32_t    getApplicabilityMask() const { return 1 << getInputSource(); }

private:
    class AudioEffectHandle
    {
    public:
        effect_handle_t mPreprocessor;
        struct echo_reference_itfe* mEchoReference;
        AudioEffectHandle():
            mPreprocessor(NULL), mEchoReference(NULL) {}
        AudioEffectHandle(effect_handle_t effect, struct echo_reference_itfe* reference):
            mPreprocessor(effect), mEchoReference(reference) {}
        ~AudioEffectHandle() {}
    };

    // Function to be used as the predicate in find_if call.
    struct MatchEffect: public std::binary_function<AudioEffectHandle, effect_handle_t, bool> {

        bool operator()(const AudioEffectHandle& effectHandle,
                           const effect_handle_t& effect) const {

            return effectHandle.mPreprocessor == effect;
        }
    };
    std::list<effect_handle_t>mRequestedEffects;

    AudioStreamInALSA(const AudioStreamInALSA &);
    AudioStreamInALSA& operator = (const AudioStreamInALSA &);
    void                resetFramesLost();
    size_t              generateSilence(void* buffer, size_t bytes);

    ssize_t             readHwFrames(void* buffer, size_t frames);

    ssize_t             readFrames(void* buffer, size_t frames);

    void                freeAllocatedBuffers();

    android::status_t   allocateProcessingMemory(ssize_t frames);

    inline android::status_t     allocateHwBuffer();

    ssize_t             processFrames(void* buffer, ssize_t frames);

    int                 doProcessFrames(const void* buffer, ssize_t frames,
                                               ssize_t* processed_frames,
                                               ssize_t* processing_frames_in);

    status_t            pushEchoReference(ssize_t frames, effect_handle_t preprocessor,
                                          struct echo_reference_itfe* reference);

    int32_t             updateEchoReference(ssize_t frames, struct echo_reference_itfe* reference);

    status_t            setPreprocessorEchoDelay(effect_handle_t handle, int32_t delay_us);

    status_t            setPreprocessorParam(effect_handle_t handle, effect_param_t *param);

    void                getCaptureDelay(struct echo_reference_buffer* buffer);

    status_t            checkAndAddAudioEffects();
    status_t            checkAndRemoveAudioEffects();

    unsigned int        mFramesLost;
    AudioSystem::audio_in_acoustics mAcoustics;

    uint32_t            mInputSource;

    ssize_t mFramesIn;

    /**
     * This variable represents the number of frames of in mProcessingBuffer.
     */
    ssize_t mProcessingFramesIn;

    /**
     * This variable is a dynamic buffer and contains raw data read from input device.
     * It is used as input buffer before application of SW accoustics effects.
     */
    int16_t* mProcessingBuffer;

    /**
     * This variable represents the size in frames of in mProcessingBuffer.
     */
    ssize_t mProcessingBufferSizeInFrames;

    /**
     * This variable represents the number of frames of in mReferenceBuffer.
     */
    ssize_t mReferenceFramesIn;

    /**
     * This variable is a dynamic buffer and contains the data used as reference for AEC and
     * which are read from AudioEffectHandle::mEchoReference.
     */
    int16_t* mReferenceBuffer;

    /**
     * This variable represents the size in frames of in mReferenceBuffer.
     */
    ssize_t mReferenceBufferSizeInFrames;

    /**
     * It is vector which contains the handlers to accoustics effects.
     */
    Vector <AudioEffectHandle> mPreprocessorsHandlerList;

    char* mHwBuffer;
    ssize_t mHwBufferSize;

    static const uint32_t HIGH_LATENCY_TO_BUFFER_INTERVAL_RATIO;
    static const uint32_t LOW_LATENCY_TO_BUFFER_INTERVAL_RATIO;
};

};        // namespace android
