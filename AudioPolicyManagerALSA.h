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

// Attenuation applied to STRATEGY_MEDIA streams during VoIP call : 12dB
#define IN_COMMUNICATION_MEDIA_ATTENUATION_IN_DB 12.0

class AudioPolicyManagerALSA: public AudioPolicyManagerBase
{

public:
    AudioPolicyManagerALSA(AudioPolicyClientInterface *clientInterface);
    virtual ~AudioPolicyManagerALSA();

    // Gets audio input handle from current input source and parameters
    virtual audio_io_handle_t getInput(int inputSource,
                                       uint32_t samplingRate,
                                       uint32_t format,
                                       uint32_t channels,
                                       AudioSystem::audio_in_acoustics acoustics);

    virtual float computeVolume(int stream, int index, audio_io_handle_t output, uint32_t device);
    virtual void setForceUse(AudioSystem::force_use usage, AudioSystem::forced_config config);
    virtual uint32_t getDeviceForStrategy(routing_strategy strategy, bool fromCache = true);
};

};
