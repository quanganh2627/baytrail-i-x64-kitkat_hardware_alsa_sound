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
    _iFmRxMode(0),
    _iFmRxHwMode(0),
    _iTtyDirection(0),
    _bIsHacModeEnabled(false),
    _bBtHeadsetNrEcEnabled(false),
    _bIsBtEnabled(false),
    _uiInputSource(0),
    _iHwMode(AudioSystem::MODE_NORMAL),
    _eBandType(ENarrowBand),
    _bIsSharedI2SGlitchSafe(false),
    _uiPlatformEventChanged(false),
    _iVolumeKeysRefCount(0),
    _pAudioRouteManager(pAudioRouteManager)
{
    _uiDevices[INPUT] = 0;
    _uiDevices[OUTPUT] = 0;
}

CAudioPlatformState::~CAudioPlatformState()
{

}

bool CAudioPlatformState::hasPlatformStateChanged(int iEvents) const
{
    return !!(_uiPlatformEventChanged & iEvents);
}

void CAudioPlatformState::setPlatformStateEvent(int iEvent)
{
    _uiPlatformEventChanged |= iEvent;
}

// Set the modem status
void CAudioPlatformState::setModemAlive(bool bIsAlive)
{
    if (_bModemAlive == bIsAlive) {

        return ;
    }

    _bModemAlive = bIsAlive;

    if (!_bModemAlive) {

        // Modem is down, the audio link as well...
        _bModemAudioAvailable = false;
    }

    // Any state change of the modem will require the changed flag to be set
    setPlatformStateEvent(EModemStateChange);

    updateHwMode();
}

// Set the modem Audio available
void CAudioPlatformState::setModemAudioAvailable(bool bIsAudioAvailable)
{
    if (_bModemAudioAvailable == bIsAudioAvailable) {

        return ;
    }
    _bModemAudioAvailable = bIsAudioAvailable;
    setPlatformStateEvent(EModemAudioStatusChange);

    // Do not force the platform state change flag.
    // let the update Hw mode raise the flag or not
    updateHwMode();
}

//
// PLATFORM DEPENDANT FUNCTION:
// shared bus may have some conditions in which
// it may not be available
//
// This function indicates if the shared I2S bus can
// be used (avoid glitches, electrical conflicts)
// 2 conditions:
//      -Modem must be up
//      -A call is being set (Android Mode already changed) but
//          modem audio status is still inactive
//
bool CAudioPlatformState::isSharedI2SBusAvailable()
{
    return isModemAlive() && _bIsSharedI2SGlitchSafe;
}

// Set Android Telephony Mode
void CAudioPlatformState::setMode(int iMode)
{
    if (iMode == _iAndroidMode) {

        return ;
    }
    _iAndroidMode = iMode;
    setPlatformStateEvent(EAndroidModeChange);

    updateHwMode();

    checkAndSetFmRxHwMode();
}

// Set FM mode
void CAudioPlatformState::setFmRxMode(int iFmRxMode)
{
    if (_iFmRxMode == iFmRxMode) {

        return ;
    }
    _iFmRxMode = iFmRxMode;
    setPlatformStateEvent(EFmModeChange);

    checkAndSetFmRxHwMode();
}

//
// This function checks if the FM RX HW mode can be changed
// (upon the Mode requested by the user and Android Mode),
// changes it and returns true if it has changed.
// Note that all request to switch off the radio must be considered
// whatever the android mode whereas request to set the FM on will
// only be granted in NORMAL android mode.
//
void CAudioPlatformState::checkAndSetFmRxHwMode()
{
    int bNewFmRxHwMode = AudioSystem::MODE_FM_OFF;

    if (getFmRxMode() == AudioSystem::MODE_FM_ON) {

        if (getMode() == AudioSystem::MODE_NORMAL) {

            // Set FmRxHw mode only in NORMAL android mode
            bNewFmRxHwMode = AudioSystem::MODE_FM_ON;
        }
    }

    if (getFmRxHwMode() != bNewFmRxHwMode) {

        ALOGD("%s: changed to %d", __FUNCTION__, bNewFmRxHwMode);
        // The mode has changed, force the rerouting flag
        // and request to reset PMDOWN time during the routing
        // process to avoid glitches
        _iFmRxHwMode = bNewFmRxHwMode;
        setPlatformStateEvent(EFmHwModeChange);
    }
}

// Set TTY mode
void CAudioPlatformState::setTtyDirection(int iTtyDirection)
{
    if (iTtyDirection == _iTtyDirection) {

        return ;
    }
    setPlatformStateEvent(ETtyDirectionChange);
    _iTtyDirection = iTtyDirection;
}

// Set HAC mode
void CAudioPlatformState::setHacMode(bool bEnabled)
{
    if (bEnabled == _bIsHacModeEnabled) {

        return ;
    }
    setPlatformStateEvent(EHacModeChange);
    _bIsHacModeEnabled = bEnabled;
}

void CAudioPlatformState::setBtEnabled(bool bIsBTEnabled)
{
    if (bIsBTEnabled == _bIsBtEnabled) {

        return ;
    }
    setPlatformStateEvent(EBtEnableChange);
    _bIsBtEnabled = bIsBTEnabled;
}

// Set BT_NREC
void CAudioPlatformState::setBtHeadsetNrEc(bool bIsAcousticSupportedOnBT)
{
    if (bIsAcousticSupportedOnBT == _bBtHeadsetNrEcEnabled) {

        return ;
    }
    setPlatformStateEvent(EBtHeadsetNrEcChange);
    _bBtHeadsetNrEcEnabled = bIsAcousticSupportedOnBT;
}

// Set devices
void CAudioPlatformState::setDevices(uint32_t devices, bool bIsOut)
{
    if (devices == _uiDevices[bIsOut]) {

        return ;
    }
    if (bIsOut)
        setPlatformStateEvent(EOutputDevicesChange);
    else
        setPlatformStateEvent(EInputDevicesChange);

    _uiDevices[bIsOut] = devices;
}

// Set devices
void CAudioPlatformState::setInputSource(uint32_t inputSource)
{
    if (_uiInputSource == inputSource) {

        return ;
    }
    _uiInputSource = inputSource;
    setPlatformStateEvent(EInputSourceChange);

    updateHwMode();
}

void CAudioPlatformState::setBandType(BandType_t eBandType)
{
    if (_eBandType == eBandType) {

        return ;
    }
    _eBandType = eBandType;
    setPlatformStateEvent(EBandTypeChange);
}

void CAudioPlatformState::updateHwMode()
{
    if (checkHwMode()) {

        ALOGD("%s: mode has changed to hwMode=%d", __FUNCTION__, getHwMode());

        setPlatformStateEvent(EHwModeChange);
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
    ALOGD("%s", __FUNCTION__);

    int tmpHwMode = _iAndroidMode;
    bool bWasSharedI2SGlitchSafe = _bIsSharedI2SGlitchSafe;
    _bIsSharedI2SGlitchSafe = true;
    bool bHwModeHasChanged = false;

    switch(_iAndroidMode) {

    case AudioSystem::MODE_NORMAL:
        break;

    case AudioSystem::MODE_RINGTONE:

        break;

    case AudioSystem::MODE_IN_CALL:

        if (popCount(getDevices(OUTPUT)) > 1 || !isModemAlive() || !isModemAudioAvailable()) {

            tmpHwMode = AudioSystem::MODE_NORMAL;
            _bIsSharedI2SGlitchSafe = false;

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

        bHwModeHasChanged = true;
    }
    if (_bIsSharedI2SGlitchSafe != bWasSharedI2SGlitchSafe) {

        setPlatformStateEvent(ESharedI2SStateChange);
    }
    return bHwModeHasChanged;
}

void CAudioPlatformState::clearPlatformStateEvents()
{
    _uiPlatformEventChanged = 0;
}

void CAudioPlatformState::enableVolumeKeys(bool bEnable)
{
    if (bEnable) {

        _iVolumeKeysRefCount += 1;
    } else {

        _iVolumeKeysRefCount -= 1;
    }
    assert(_iVolumeKeysRefCount >= 0);

    if (!_iVolumeKeysRefCount == 1) {

        CVolumeKeys::wakeupDisable();

    } else if (_iVolumeKeysRefCount == 1) {

        CVolumeKeys::wakeupEnable();
    }
}

void CAudioPlatformState::setDirectStreamEvent()
{
    setPlatformStateEvent(EStreamEvent);
}

}       // namespace android
