/* AudioPlatformHardware.h
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

using namespace std;

namespace android_audio_legacy
{

typedef enum {
    HWCODEC_OIA,
    HWCODEC_1IA,
    MODEM_IA,
    BT_IA,
    FM_IA,
    MEDIA,
    VOICE,
    NB_ROUTE
} AudioRouteId;

static const int audio_routes[NB_ROUTE] = {
    1<<HWCODEC_OIA,
    1<<HWCODEC_1IA,
    1<<BT_IA,
    1<<FM_IA,
    1<<MODEM_IA,
    1<<MEDIA,
//    1<<VOICE
};

};        // namespace android

