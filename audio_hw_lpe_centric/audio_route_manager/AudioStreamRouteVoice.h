/* AudioStreamRouteVoice.h
 **
 ** Copyright 2012 Intel Corporation
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **      http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#pragma once

#include "AudioStreamRoute.h"

namespace android_audio_legacy
{

class CAudioStreamRouteVoice : public CAudioStreamRoute
{
public:
    CAudioStreamRouteVoice(uint32_t uiRouteId,
                          int iOutputDeviceId,
                          int iInputDeviceId,
                          int iCardId,
                          const pcm_config& outputPcmConfig,
                          const pcm_config& inputPcmConfig,
                          CAudioPlatformState* platformState);

    virtual bool isApplicable(uint32_t devices, int mode, bool bIsOut);
};

// ----------------------------------------------------------------------------

};        // namespace android

