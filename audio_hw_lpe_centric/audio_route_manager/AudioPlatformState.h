/* AudioPlatformState.h
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

// TBR
#include "vpc_hardware.h"
// End of TBR

#define INPUT        false
#define OUTPUT       true

namespace android_audio_legacy
{

enum ETty { TTY_OFF, TTY_FULL, TTY_VCO, TTY_HCO };

class CAudioRouteManager;

class CAudioPlatformState
{
public:

    CAudioPlatformState(CAudioRouteManager* pAudioRouteManager);
    virtual           ~CAudioPlatformState();

    // Get the modem status
    bool isModemAlive() const { return _bModemAlive; }

    // Set the modem status
    void setModemAlive(bool bIsAlive);

    // Get the modem audio call status
    bool isModemAudioAvailable() const { return _bModemAudioAvailable; }

    // Set the modem Audio available
    void setModemAudioAvailable(bool bIsAudioAvailable);

    // Set telephony mode
    void setMode(int iMode);

    // Get telephony mode
    int getMode() const { return _iAndroidMode; }

    // Set FM mode
    void setFmMode(int fmMode);

    // Get FM mode
    int getFmMode() const { return _iFmMode; }

    // Get the HW mode
    int getHwMode() const { return _iHwMode; }

    // Set TTY mode
    void setTtyMode(ETty iTtyMode);

    // Get TTY mode
    int getTtyMode() const { return _iTtyMode; }

    // Set HAC mode
    void setHacMode(bool bEnabled);

    // Get HAC Mode
    bool isHacEnabled() const { return _bIsHacModeEnabled; }

    // Set BT_NREC
    void setBtNrEc(bool bIsAcousticSupportedOnBT);

    // Get BT NREC
    bool isAcousticSupportedOnBT() const { return _bIsAcousticOnBTEnabled; }

    // Set BT Enabled flag
    void setBtEnabled(bool bIsBTEnabled);

    // Get BT Enabled flag
    bool isBtEnabled() const { return _bIsBtEnabled; }

    // Get devices
    uint32_t getDevices(bool bIsOut) const { return _uiDevices[bIsOut]; }

    // Set devices
    void setDevices(uint32_t devices, bool bIsOut);

    // Get input source
    uint32_t getInputSource() const { return _uiInputSource; }

    // Set devices
    void setInputSource(uint32_t inputSource);

    void clearPlatformState();

    bool hasPlatformStateChanged() const { return _bPlatformStateHasChanged; }

    // update the HW mode
    void updateHwMode();

    bool hasActiveStream(bool bIsOut);

private:

    // Check if the Hw mode has changed
    bool checkHwMode();

    static inline uint32_t popCount(uint32_t u)
    {
        u = ((u&0x55555555) + ((u>>1)&0x55555555));
        u = ((u&0x33333333) + ((u>>2)&0x33333333));
        u = ((u&0x0f0f0f0f) + ((u>>4)&0x0f0f0f0f));
        u = ((u&0x00ff00ff) + ((u>>8)&0x00ff00ff));
        u = ( u&0x0000ffff) + (u>>16);
        return u;
    }
    // Modem Call state
    bool _bModemAudioAvailable;

    // Modem State
    bool _bModemAlive;

    // Android Telephony mode cache
    int _iAndroidMode;

    // FM mode
    int _iFmMode;

    // TTY Mode
    int _iTtyMode;

    // HAC mode set
    bool _bIsHacModeEnabled;

    // BTNREC set
    bool _bIsAcousticOnBTEnabled;

    // BT Enabled flag
    bool _bIsBtEnabled;

    // Input/output Devices bit field
    uint32_t _uiDevices[2];

    uint32_t _uiInputSource;

    uint32_t _uiRoutes[];

    // Hw Mode: translate the use case, indeed it implies the audio route to follow
    int32_t _iHwMode;

    bool _bPlatformStateHasChanged;

    CAudioRouteManager* _pAudioRouteManager;
};
// ----------------------------------------------------------------------------

};        // namespace android

