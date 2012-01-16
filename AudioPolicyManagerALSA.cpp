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

#define LOG_TAG "AudioPolicyManagerALSA"
//#define LOG_NDEBUG 0
#include <utils/Log.h>
#include "AudioPolicyManagerALSA.h"
#include <media/mediarecorder.h>

namespace android_audio_legacy {

// ----------------------------------------------------------------------------
// AudioPolicyManagerALSA
// ----------------------------------------------------------------------------

// ---  class factory

extern "C" AudioPolicyInterface* createAudioPolicyManager(AudioPolicyClientInterface *clientInterface)
{
    return new AudioPolicyManagerALSA(clientInterface);
}

extern "C" void destroyAudioPolicyManager(AudioPolicyInterface *interface)
{
    delete interface;
}

audio_io_handle_t AudioPolicyManagerALSA::getInput(int inputSource,
                                                   uint32_t samplingRate,
                                                   uint32_t format,
                                                   uint32_t channels,
                                                   AudioSystem::audio_in_acoustics acoustics)
{
    uint32_t device = getDeviceForInputSource(inputSource);

    // If one input (device in capture) is used then the policy shall refuse to any record
    // application to acquire another input. Unless the ref count is null, that means that
    // there is an input created but not used, and we can safely return its input handle.
    if (!mInputs.isEmpty()) {
        if(mInputs.valueAt(0)->mRefCount > 0) {
            LOGW("getInput() mPhoneState : %d, device 0x%x, already one input used with other source, return invalid audio input handle!",
                 mPhoneState, device);
            return 0;
        }
    }

    // Call base implementation
    return AudioPolicyManagerBase::getInput(inputSource, samplingRate, format, channels, acoustics);
}

float AudioPolicyManagerALSA::computeVolume(int stream, int index, audio_io_handle_t output, uint32_t device)
{
    float volume = 1.0;

    volume = AudioPolicyManagerBase::computeVolume(stream, index, output, device);

    // Attenuate media streams by 12dB during voice over IP call. For CSV voice call,
    // this attenuation is applied in the 3G modem.
    if (mPhoneState == AudioSystem::MODE_IN_COMMUNICATION &&
        getStrategy((AudioSystem::stream_type)stream) == STRATEGY_MEDIA) {
        volume *= pow(10.0, -IN_COMMUNICATION_MEDIA_ATTENUATION_IN_DB/10.0);
        LOGV("computeVolume, limit media streams volume during voice over IP calls to %.3f", volume);
    }

    return volume;
}


AudioPolicyManagerALSA::AudioPolicyManagerALSA(AudioPolicyClientInterface *clientInterface)
    : AudioPolicyManagerBase(clientInterface)
{
}

AudioPolicyManagerALSA::~AudioPolicyManagerALSA()
{
}

}; // namespace android
