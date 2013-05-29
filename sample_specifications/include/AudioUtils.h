/*
 ** Copyright 2013 Intel Corporation
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

#include <tinyalsa/asoundlib.h>
#include <stdint.h>
#include <sys/types.h>


namespace android_audio_legacy {

class CSampleSpec;

class CAudioUtils
{
public:
    static uint32_t alignOn16(uint32_t u);

    static ssize_t convertSrcToDstInBytes(ssize_t bytes, const CSampleSpec& ssSrc, const CSampleSpec& ssDst);

    static ssize_t convertSrcToDstInFrames(ssize_t frames, const CSampleSpec& ssSrc, const CSampleSpec& ssDst);

    // This fonction translates the format from ALSA lib
    // to AudioSystem enum
    static int convertTinyToHalFormat(pcm_format format);
    static pcm_format convertHalToTinyFormat(int format);
    static int getCardIndexByName(const char* name);

    static uint32_t convertUsecToMsec(uint32_t uiTimeUsec);

    static bool isAudioInputDevice(uint32_t uiDevices);

    /**
      * Constante used during convert of frames to delays in micro-seconds (us).
      * It is used for delays computation of AEC effect
      */
    static const uint32_t USEC_TO_SEC = 1000000;
};

}; // namespace android

