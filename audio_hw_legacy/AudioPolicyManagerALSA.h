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
                                                        audio_io_handle_t output,
                                                        audio_devices_t device);

    virtual audio_devices_t getDeviceForStrategy(routing_strategy strategy, bool fromCache = true);
    virtual audio_devices_t getDeviceForInputSource(int inputSource);
 private:
    // true if current platform implements a back microphone
    inline bool hasBackMicrophone() const { return mAvailableInputDevices & AudioSystem::DEVICE_IN_BACK_MIC; }
    // true if current platform implements an earpiece
    inline bool hasEarpiece() const { return mAttachedOutputDevices & AudioSystem::DEVICE_OUT_EARPIECE; }
};

};
