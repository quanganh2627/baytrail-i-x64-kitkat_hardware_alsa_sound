/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdint.h>
#include <math.h>
#include <sys/types.h>
#include <utils/Timers.h>
#include <utils/Errors.h>
#include <utils/KeyedVector.h>
#include <hardware_legacy/AudioPolicyManagerBase.h>


namespace android_audio_legacy {

// ----------------------------------------------------------------------------


class AudioPolicyManagerALSA: public AudioPolicyManagerBase
{

public:
    AudioPolicyManagerALSA(AudioPolicyClientInterface *clientInterface);
    virtual ~AudioPolicyManagerALSA();

    // Set the device availability depending on its connection state
    virtual status_t setDeviceConnectionState(AudioSystem::audio_devices device,
                                              AudioSystem::device_connection_state state,
                                              const char *device_address);

    virtual status_t startOutput(audio_io_handle_t output,
                                 AudioSystem::stream_type stream,
                                 int session);

    // Gets audio input handle from current input source and parameters
    virtual audio_io_handle_t getInput(int inputSource,
                                       uint32_t samplingRate,
                                       uint32_t format,
                                       uint32_t channels,
                                       AudioSystem::audio_in_acoustics acoustics);

    status_t startInput(audio_io_handle_t input);

    virtual status_t setStreamVolumeIndex(AudioSystem::stream_type stream,
                                          int index,
                                          audio_devices_t device);

    virtual float computeVolume(int stream,
                                int index,
                                audio_devices_t device);

    virtual audio_devices_t getDeviceForStrategy(routing_strategy strategy, bool fromCache = true);

    /**
     * Get the device for a given input source
     *
     * @param[in] the input source used
     *
     * @return enum input device
     */
    virtual audio_devices_t getDeviceForInputSource(int inputSource);

    /**
      * This function delegates to parent implementation the release of input stream and then
      * it checks if it is necessary to restart a previously stopped input stream.
      *
      * @param [in] input handle of input stream to release
      */
    virtual void releaseInput(audio_io_handle_t input);

 private:
    // Is voice volume applied after mixing while in mode IN_COMM ?
    bool mVoiceVolumeAppliedAfterMixInComm;
    // Is voice volume applied after mixing while in mode IN_CALL ?
    bool mVoiceVolumeAppliedAfterMixInCall;
    // Music attenuation in dB while in call (csv or voip)
    float mInCallMusicAttenuation_dB;

    /// audio_policy.conf file custom properies
    //  Is voice volume applied after mixing while in mode IN_COMM ?
    static const String8 mVoiceVolumeAppliedAfterMixInCommPropName;
    // Is voice volume applied after mixing while in mode IN_CALL ?
    static const String8 mVoiceVolumeAppliedAfterMixInCallPropName;
    // Music attenuation in dB while in call (csv or voip)
    static const String8 mInCallMusicAttenuation_dBPropName;

    // true if current platform implements a back microphone
    inline bool hasBackMicrophone() const { return mAvailableInputDevices & AudioSystem::DEVICE_IN_BACK_MIC; }
    // true if current platform implements an earpiece
    inline bool hasEarpiece() const { return mAttachedOutputDevices & AudioSystem::DEVICE_OUT_EARPIECE; }

    /**
     * Get raw stream volume level for selected device.
     * Computes the ratio between current volume index and whole volume index range.
     *
     * @param[in] stream the audio stream for which raw volume level is computed.
     * @param[in] device audio device on which computed raw volume level is applied.
     *
     * @return Raw volume ratio between 0.0 and 1.0
     */
    float getRawVolumeLevel(AudioSystem::stream_type stream, audio_devices_t device) const;

    /**
     * Check if voice volume is applied after mixing media streams.
     * Returns based on audio Policy configuration if voice volume is applied
     * after music/tones are mixed with voice call stream; only significant for
     * current established call (CSV or VoIP). Asserts if called out-of-call.
     *
     * @return true if voice volume is set after mixing for current call, false otherwise.
     */
    bool isVoiceVolumeSetAfterMix() const;

    /**
     * Set volume on given stream.
     * Compute and apply stream volume on all outputs according to connected device.
     *
     * @param[in] stream the audio stream for which volume is computed and applied
     * @param[in] device audio device on which volume is applied for stream;
     *            AUDIO_DEVICE_OUT_DEFAULT implies setting volume on all devices.
     *
     * @return OK on success
     */
    status_t applyVolumeOnStream(AudioSystem::stream_type stream, audio_devices_t device);

};

};
