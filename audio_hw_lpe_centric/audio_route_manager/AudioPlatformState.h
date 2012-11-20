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

#define INPUT        false
#define OUTPUT       true
#define ADD_EVENT(eventName) E##eventName = 1 << eventName

namespace android_audio_legacy
{

#define TTY_DOWNLINK    0x1
#define TTY_UPLINK      0x2

class CAudioRouteManager;

class CAudioPlatformState
{
public:
    enum EventName_t {
        AndroidModeChange,
        HwModeChange,
        FmModeChange,
        FmHwModeChange,
        ModemStateChange,
        ModemAudioStatusChange,
        HacModeChange,
        TtyDirectionChange,
        BtEnableChange,
        BtHeadsetNrEcChange,
        BandTypeChange,
        InputDevicesChange,
        OutputDevicesChange,
        InputSourceChange,
        StreamEvent,

        NbEvents
    };


    enum EventType_t {
        EAllEvents                  = -1,
        ADD_EVENT(AndroidModeChange),
        ADD_EVENT(HwModeChange),
        ADD_EVENT(FmModeChange),
        ADD_EVENT(FmHwModeChange),
        ADD_EVENT(ModemStateChange),
        ADD_EVENT(ModemAudioStatusChange),
        ADD_EVENT(HacModeChange),
        ADD_EVENT(TtyDirectionChange),
        ADD_EVENT(BtEnableChange),
        ADD_EVENT(BtHeadsetNrEcChange),
        ADD_EVENT(BandTypeChange),
        ADD_EVENT(InputDevicesChange),
        ADD_EVENT(OutputDevicesChange),
        ADD_EVENT(InputSourceChange),
        ADD_EVENT(StreamEvent)
    };

    enum BandType_t {
        ENarrowBand,
        EWideBand,

        ENbBandType
    };

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
    void setFmRxMode(int fmMode);

    // Get FM mode
    int getFmRxMode() const { return _iFmRxMode; }

    // Get FM HW mode
    int getFmRxHwMode() const { return _iFmRxHwMode; }

    // Get the HW mode
    int getHwMode() const { return _iHwMode; }

    // Set TTY mode
    void setTtyDirection(int iTtyDirection);

    // Get TTY mode
    int getTtyDirection() const { return _iTtyDirection; }

    // Set HAC mode
    void setHacMode(bool bEnabled);

    // Get HAC Mode
    bool isHacEnabled() const { return _bIsHacModeEnabled; }

    // Set BT_NREC
    void setBtHeadsetNrEc(bool bIsAcousticSupportedOnBT);

    // Get BT NREC
    bool isBtHeadsetNrEcEnabled() const { return _bBtHeadsetNrEcEnabled; }

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

    void setBandType(BandType_t eBandType);

    BandType_t getBandType() { return _eBandType; }

    void setPlatformStateEvent(int iEvent);

    void clearPlatformStateEvents();

    bool hasPlatformStateChanged(int iEvents = EAllEvents) const;

    // update the HW mode
    void updateHwMode();

    void enableVolumeKeys(bool bEnable);

private:
    // Check if the Hw mode has changed
    bool checkHwMode();

    // Check if Fm Hw mode has changed
    void checkAndSetFmRxHwMode();

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
    int _iFmRxMode;
    int _iFmRxHwMode;

    // TTY Mode
    int _iTtyDirection;

    // HAC mode set
    bool _bIsHacModeEnabled;

    // BTNREC set
    bool _bBtHeadsetNrEcEnabled;

    // BT Enabled flag
    bool _bIsBtEnabled;

    // Input/output Devices bit field
    uint32_t _uiDevices[2];

    uint32_t _uiInputSource;

    uint32_t _uiRoutes[];

    // Hw Mode: translate the use case, indeed it implies the audio route to follow
    int32_t _iHwMode;

    BandType_t _eBandType;

    uint32_t _uiPlatformEventChanged;

    int _iVolumeKeysRefCount;

    CAudioRouteManager* _pAudioRouteManager;
};
// ----------------------------------------------------------------------------

};        // namespace android

