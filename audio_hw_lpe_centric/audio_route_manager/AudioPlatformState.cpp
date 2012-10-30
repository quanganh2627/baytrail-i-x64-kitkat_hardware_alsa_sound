/* AudioPlatformState.cpp
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

#define LOG_TAG "AudioPlatformState"

#include <hardware_legacy/AudioHardwareBase.h>
#include <utils/Log.h>
#include "AudioPlatformState.h"
#include "VolumeKeys.h"
#include "AudioRouteManager.h"


namespace android_audio_legacy
{

CAudioPlatformState::CAudioPlatformState(CAudioRouteManager* pAudioRouteManager) :
    _bModemAudioAvailable(false),
    _bModemAlive(false),
    _iAndroidMode(AudioSystem::MODE_NORMAL),
    _iFmMode(0),
    _iTtyMode(TTY_OFF),
    _bIsHacModeEnabled(false),
    _bIsAcousticOnBTEnabled(false),
    _bIsBtEnabled(false),
    _uiInputSource(0),
    _iHwMode(AudioSystem::MODE_NORMAL),
    _bPlatformStateHasChanged(false),
    _pAudioRouteManager(pAudioRouteManager)
{
    _uiDevices[INPUT] = 0;
    _uiDevices[OUTPUT] = 0;
}

CAudioPlatformState::~CAudioPlatformState()
{

}

// Set the modem status
void CAudioPlatformState::setModemAlive(bool bIsAlive)
{
    if (_bModemAlive == bIsAlive) {

        return ;
    }

    _bModemAlive = bIsAlive;

    // Any state change of the modem will require the changed flag to be set
    _bPlatformStateHasChanged = true;

    updateHwMode();
}

// Set the modem Audio available
void CAudioPlatformState::setModemAudioAvailable(bool bIsAudioAvailable)
{
    if (_bModemAudioAvailable == bIsAudioAvailable) {

        return ;
    }
    _bModemAudioAvailable = bIsAudioAvailable;

    // Do not force the platform state change flag.
    // let the update Hw mode raise the flag or not
    updateHwMode();
}

// Set Android Telephony Mode
void CAudioPlatformState::setMode(int iMode)
{
    if (iMode == _iAndroidMode) {

        return ;
    }
    _iAndroidMode = iMode;

    updateHwMode();
}

// Set FM mode
void CAudioPlatformState::setFmMode(int fmMode)
{
    if (fmMode == _iFmMode) {

        return ;
    }
    _bPlatformStateHasChanged = true;
    _iFmMode = fmMode;
}

// Set TTY mode
void CAudioPlatformState::setTtyMode(ETty iTtyMode)
{
    if (iTtyMode == _iTtyMode) {

        return ;
    }
    _bPlatformStateHasChanged = true;
    _iTtyMode = iTtyMode;
}

// Set HAC mode
void CAudioPlatformState::setHacMode(bool bEnabled)
{
    if (bEnabled == _bIsHacModeEnabled) {

        return ;
    }
    _bPlatformStateHasChanged = true;
    _bIsHacModeEnabled = bEnabled;
}

void CAudioPlatformState::setBtEnabled(bool bIsBTEnabled)
{
    if (bIsBTEnabled == _bIsBtEnabled) {

        return ;
    }
    _bPlatformStateHasChanged = true;
    _bIsBtEnabled = bIsBTEnabled;
}

// Set BT_NREC
void CAudioPlatformState::setBtNrEc(bool bIsAcousticSupportedOnBT)
{
    if (bIsAcousticSupportedOnBT == _bIsAcousticOnBTEnabled) {

        return ;
    }
    _bPlatformStateHasChanged = true;
    _bIsAcousticOnBTEnabled = bIsAcousticSupportedOnBT;
}

// Set devices
void CAudioPlatformState::setDevices(uint32_t devices, bool bIsOut)
{
    if (devices == _uiDevices[bIsOut]) {

        return ;
    }
    _bPlatformStateHasChanged = true;
    _uiDevices[bIsOut] = devices;
}

// Set devices
void CAudioPlatformState::setInputSource(uint32_t inputSource)
{
    if (_uiInputSource == inputSource) {

        return ;
    }
    _uiInputSource = inputSource;

    updateHwMode();
}

void CAudioPlatformState::updateHwMode()
{
    if (checkHwMode()) {

        LOGD("%s: mode has changed to hwMode=%d", __FUNCTION__, getHwMode());

        _bPlatformStateHasChanged = true;
    }
}

//
// This function checks the "Hardware" that implies the audio route
// to be used.
// It matches the android telephony mode except if:
//      - InRingtoneMode:
//      - Multiple outputdevice selected during IN CALL or IN COMM mode
//                => mode NORMAL forced
//
bool CAudioPlatformState::checkHwMode()
{
    LOGD("%s", __FUNCTION__);

    int tmpHwMode = _iAndroidMode;

    switch(_iAndroidMode) {

    case AudioSystem::MODE_NORMAL:
        break;

    case AudioSystem::MODE_RINGTONE:

        //            if (isModemAudioAvailable()) {

        //                tmpHwMode = AudioSystem::MODE_NORMAL;
        //            }
        break;

    case AudioSystem::MODE_IN_CALL:

        if (popCount(getDevices(OUTPUT)) > 1 || !isModemAlive() || !isModemAudioAvailable()) {

            tmpHwMode = AudioSystem::MODE_NORMAL;

        }
        break;

    case AudioSystem::MODE_IN_COMMUNICATION:

        if (popCount(getDevices(OUTPUT)) > 1) {

            tmpHwMode = AudioSystem::MODE_NORMAL;
        }

        break;
    }

    if (tmpHwMode != getHwMode()) {

        _iHwMode = tmpHwMode;

        if (_iHwMode == AudioSystem::MODE_IN_CALL || _iHwMode == AudioSystem::MODE_IN_COMMUNICATION) {

       //     CVolumeKeys::wakeupEnable();
        } else {

       //     CVolumeKeys::wakeupDisable();
        }

        return true;
    }
    return false;
}

void CAudioPlatformState::clearPlatformState()
{
    _bPlatformStateHasChanged = false;
}

bool CAudioPlatformState::hasActiveStream(bool bIsOut)
{
    return _pAudioRouteManager->hasActiveStream(bIsOut);
}

}       // namespace android
