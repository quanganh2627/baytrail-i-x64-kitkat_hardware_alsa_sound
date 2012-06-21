/* AudioHardwareALSA.h
 **
 ** Copyright 2008-2009, Wind River Systems
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#ifndef ANDROID_AUDIO_HARDWARE_ALSA_H
#define ANDROID_AUDIO_HARDWARE_ALSA_H

#include <utils/List.h>
#include <list>
#include <string>
#include <vector>
#include <hardware_legacy/AudioHardwareBase.h>

#include <alsa/asoundlib.h>

#include <hardware/hardware.h>
#include <vpc_hardware.h>
#include <fm_module.h>

#include <utils/threads.h>

#ifdef USE_INTEL_SRC
#include "AudioResamplerALSA.h"
#endif

#include "AudioHardwareALSACommon.h"
#include "ATNotifier.h"

class CParameterMgrPlatformConnector;
class ISelectionCriterionTypeInterface;
class ISelectionCriterionInterface;
class CATManager;
class CCallStatUnsollicitedATCommand;
class CProgressUnsollicitedATCommand;

using namespace std;

namespace android_audio_legacy
{
class CParameterMgrPlatformConnectorLogger;
using android::RWLock;
using android::List;
using android::Mutex;
typedef RWLock::AutoRLock AutoR;
typedef RWLock::AutoWLock AutoW;
class AudioHardwareALSA;
class AudioRouteManager;
class AudioRoute;

const uint32_t DEVICE_OUT_BLUETOOTH_SCO_ALL = AudioSystem::DEVICE_OUT_BLUETOOTH_SCO | AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET | AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT;


class ALSAMixer
{
public:
    ALSAMixer(AudioHardwareALSA *hardwareAlsa);
    virtual                ~ALSAMixer();

    bool                    isValid() {
        return ((!!mMixer[SND_PCM_STREAM_PLAYBACK]) && (!!mMixer[SND_PCM_STREAM_CAPTURE]));
    }
    status_t                setMasterVolume(float volume);
    status_t                setMasterGain(float gain);

    status_t                setVolume(uint32_t device, float left, float right);
    status_t                setGain(uint32_t device, float gain);

    status_t                setCaptureMuteState(uint32_t device, bool state);
    status_t                getCaptureMuteState(uint32_t device, bool *state);
    status_t                setPlaybackMuteState(uint32_t device, bool state);
    status_t                getPlaybackMuteState(uint32_t device, bool *state);

private:
    ALSAMixer(const ALSAMixer &);
    ALSAMixer& operator = (const ALSAMixer &);
    snd_mixer_t *           mMixer[SND_PCM_STREAM_LAST+1];
    AudioHardwareALSA *     mHardwareAlsa;
};

class ALSAControl
{
public:
    ALSAControl(const char *device = "hw:00");
    virtual                ~ALSAControl();

    status_t                get(const char *name, unsigned int &value, int index = 0);
    status_t                set(const char *name, unsigned int value, int index = -1);

    status_t                set(const char *name, const char *);

private:
    ALSAControl(const ALSAControl &);
    ALSAControl& operator = (const ALSAControl &);
    snd_ctl_t *             mHandle;
};

class ALSAStreamOps
{
public:
    ALSAStreamOps(AudioHardwareALSA *parent, alsa_handle_t *handle, const char* pcLockTag);
    virtual            ~ALSAStreamOps();

    status_t            set(int *format, uint32_t *channels, uint32_t *rate);

    status_t            setParameters(const String8& keyValuePairs);
    String8             getParameters(const String8& keys);

    uint32_t            sampleRate() const;
    size_t              bufferSize() const;
    int                 format() const;
    uint32_t            channels() const;

    status_t            open(uint32_t devices, int mode);
    void                close();
    void                doClose();
    void                doStandby();
    status_t            standby();

    virtual bool        isOut() = 0;
    status_t            setRoute(AudioRoute *audioRoute, uint32_t devices, int mode);

    status_t            doRoute(int mode);

    status_t            undoRoute();

    bool                routeAvailable();

protected:
    friend class AudioHardwareALSA;
    ALSAStreamOps(const ALSAStreamOps &);
    ALSAStreamOps& operator = (const ALSAStreamOps &);

    void                acquirePowerLock();
    void                releasePowerLock();

    vpc_device_t *vpc();
    acoustic_device_t *acoustics();
    ALSAMixer *mixer();

    AudioHardwareALSA *     mParent;
    alsa_handle_t *         mHandle;

    Mutex                   mLock;
    bool                    mStandby;
    uint32_t                mDevices;

private:
    void        vpcRoute(uint32_t devices, int mode);
    void        vpcUnroute(uint32_t curDev, int curMode);
    void        storeAndResetPmDownDelay();
    void        restorePmDownDelay();

    void        writeSysEntry(const char* filePath, int value);
    int         readSysEntry(const char* filePath);
    bool        isDeviceBluetoothSCO(uint32_t devices);
    bool        isBluetoothScoNormalInUse();

    int         headsetPmDownDelay;
    int         speakerPmDownDelay;
    int         voicePmDownDelay;

    bool        isResetted;
    AudioRoute*             mAudioRoute;

    bool                    mPowerLock;
    const char*             mPowerLockTag;
};

// ----------------------------------------------------------------------------

class AudioStreamOutALSA : public AudioStreamOut, public ALSAStreamOps
{
public:
    AudioStreamOutALSA(AudioHardwareALSA *parent, alsa_handle_t *handle);
    virtual            ~AudioStreamOutALSA();

    virtual uint32_t    sampleRate() const
    {
        return ALSAStreamOps::sampleRate();
    }

    virtual size_t      bufferSize() const
    {
        return ALSAStreamOps::bufferSize();
    }

    virtual uint32_t    channels() const;

    virtual int         format() const
    {
        return ALSAStreamOps::format();
    }

    virtual uint32_t    latency() const;

    virtual ssize_t     write(const void *buffer, size_t bytes);
    virtual status_t    dump(int fd, const Vector<String16>& args);

    status_t            setVolume(float left, float right);

    virtual status_t    standby();

    virtual status_t    setParameters(const String8& keyValuePairs);

    virtual String8     getParameters(const String8& keys) {
        return ALSAStreamOps::getParameters(keys);
    }

    // return the number of audio frames written by the audio dsp to DAC since
    // the output has exited standby
    virtual status_t    getRenderPosition(uint32_t *dspFrames);

    virtual bool        isOut();

    status_t            open(int mode);
    status_t            close();

private:
    AudioStreamOutALSA(const AudioStreamOutALSA &);
    AudioStreamOutALSA& operator = (const AudioStreamOutALSA &);
    size_t              generateSilence(size_t bytes);

    uint32_t            mFrameCount;
};

class AudioStreamInALSA : public AudioStreamIn, public ALSAStreamOps
{
public:
    AudioStreamInALSA(AudioHardwareALSA *parent,
                      alsa_handle_t *handle,
                      AudioSystem::audio_in_acoustics audio_acoustics);
    virtual            ~AudioStreamInALSA();

    virtual uint32_t    sampleRate() const
    {
        return ALSAStreamOps::sampleRate();
    }

    virtual size_t      bufferSize() const
    {
        return ALSAStreamOps::bufferSize();
    }

    virtual uint32_t    channels() const
    {
        return ALSAStreamOps::channels();
    }

    virtual int         format() const
    {
        return ALSAStreamOps::format();
    }

    virtual ssize_t     read(void* buffer, ssize_t bytes);
    virtual status_t    dump(int fd, const Vector<String16>& args);

    virtual status_t    setGain(float gain);

    virtual status_t    standby();

    virtual status_t    setParameters(const String8& keyValuePairs);

    virtual String8     getParameters(const String8& keys)
    {
        return ALSAStreamOps::getParameters(keys);
    }

    // Return the amount of input frames lost in the audio driver since the last call of this function.
    // Audio driver is expected to reset the value to 0 and restart counting upon returning the current value by this function call.
    // Such loss typically occurs when the user space process is blocked longer than the capacity of audio driver buffers.
    // Unit: the number of input audio frames
    virtual unsigned int  getInputFramesLost() const;

    virtual bool        isOut();

    status_t            setAcousticParams(void* params);

    status_t            open(int mode);
    status_t            close();
    virtual status_t addAudioEffect(effect_handle_t effect) { return NO_ERROR; };
    virtual status_t removeAudioEffect(effect_handle_t effect) { return NO_ERROR; };

private:
    AudioStreamInALSA(const AudioStreamInALSA &);
    AudioStreamInALSA& operator = (const AudioStreamInALSA &);
    void                resetFramesLost();
    size_t              generateSilence(void *buffer, size_t bytes);

    unsigned int        mFramesLost;
    AudioSystem::audio_in_acoustics mAcoustics;
};

class AudioHardwareALSA : public AudioHardwareBase, public IATNotifier
{
    enum RoutingEvent {
        EModeChange,
        ECallStatusChange,
        EModemStateChange
    };

    typedef list<AudioStreamOutALSA*>::iterator CAudioStreamOutALSAListIterator;
    typedef list<AudioStreamOutALSA*>::const_iterator CAudioStreamOutALSAListConstIterator;

    typedef list<AudioStreamInALSA*>::iterator CAudioStreamInALSAListIterator;
    typedef list<AudioStreamInALSA*>::const_iterator CAudioStreamInALSAListConstIterator;

    typedef list<alsa_handle_t*> ALSAHandleList;

public:
    AudioHardwareALSA();
    virtual            ~AudioHardwareALSA();

    /**
     * check to see if the audio hardware interface has been initialized.
     * return status based on values defined in include/utils/Errors.h
     */
    virtual status_t    initCheck();

    /** set the audio volume of a voice call. Range is between 0.0 and 1.0 */
    virtual status_t    setVoiceVolume(float volume);

    /**
     * set the audio volume for all audio activities other than voice call.
     * Range between 0.0 and 1.0. If any value other than NO_ERROR is returned,
     * the software mixer will emulate this capability.
     */
    virtual status_t    setMasterVolume(float volume);

    /**
     * setMode is called when the audio mode changes. NORMAL mode is for
     * standard audio playback, RINGTONE when a ringtone is playing, and IN_CALL
     * when a call is in progress.
     */
    virtual status_t    setMode(int mode);

        virtual status_t    setFmRxMode(int mode);

    // mic mute
    virtual status_t    setMicMute(bool state);
    virtual status_t    getMicMute(bool* state);

    // set/get global audio parameters
    virtual status_t    setParameters(const String8& keyValuePairs);
    //virtual String8     getParameters(const String8& keys);

    // set Stream Parameters
    virtual status_t    setStreamParameters(ALSAStreamOps* pStream, bool bForOutput, const String8& keyValuePairs);


    // Returns audio input buffer size according to parameters passed or 0 if one of the
    // parameters is not supported
    virtual size_t    getInputBufferSize(uint32_t sampleRate, int format, int channels);

    /** This method creates and opens the audio hardware output stream */
    virtual AudioStreamOut* openOutputStream(
        uint32_t devices,
        int *format=0,
        uint32_t *channels=0,
        uint32_t *sampleRate=0,
        status_t *status=0);
    virtual    void        closeOutputStream(AudioStreamOut* out);

    /** This method creates and opens the audio hardware input stream */
    virtual AudioStreamIn* openInputStream(
        uint32_t devices,
        int *format,
        uint32_t *channels,
        uint32_t *sampleRate,
        status_t *status,
        AudioSystem::audio_in_acoustics acoustics);
    virtual    void        closeInputStream(AudioStreamIn* in);

    /**This method dumps the state of the audio hardware */
    //virtual status_t dumpState(int fd, const Vector<String16>& args);

    static AudioHardwareInterface* create();

    int                 mode()
    {
        return mMode;
    }

    /* from AudioModemStateOberser: notified on modem status changes */
    virtual bool onUnsollicitedReceived(CUnsollicitedATCommand* pUnsollicitedCmd) ;
    virtual bool onAnsynchronousError(const CATcommand* pATCmd, int errorType);
    virtual void onModemStateChanged();

protected:
    virtual status_t    dump(int fd, const Vector<String16>& args);


    // Cast Hw device from mHwDeviceArray to the corresponding hw device type
    alsa_device_t* getAlsaHwDevice() const;
    vpc_device_t* getVpcHwDevice() const;
    fm_device_t* getFmHwDevice() const;
    acoustic_device_t* getAcousticHwDevice() const;

    bool isReconsiderRoutingForced() { return mForceReconsiderInCallRoute; }

    friend class AudioStreamOutALSA;
    friend class AudioStreamInALSA;
    friend class ALSAStreamOps;
    friend class ALSAMixer;

    ALSAMixer *         mMixer;

    ALSAHandleList      mDeviceList;
    int getFmRxMode() { return mFmRxMode; }
    int getPrevFmRxMode() { return mPrevFmRxMode; }

#ifdef USE_INTEL_SRC
    AudioResamplerALSA *mResampler;
#endif

private:
    AudioHardwareALSA(const AudioHardwareALSA &);
    AudioHardwareALSA& operator = (const AudioHardwareALSA &);

    // Force a re-routing of MSIC Voice route on MM route
    void reconsiderRouting();

    // State machine of route accessibility
    void applyRouteAccessibilityRules(RoutingEvent aRoutEvent);

    // Check modem audio path upon XProgress/XCallStat reception
    void onModemXCmdReceived();

    // Translate the mode and force route flag into a new mode
    int audioMode();

    // set the force MM route flag
    void forceMediaRoute(bool isForced);

    // Returns true if audio mode is in call or communication
    bool isInCallOrComm(int audMode);

    RWLock                mLock;
    bool mMicMuteState;

    enum HW_DEVICE {
        ALSA_HW_DEV = 0,
        ACOUSTIC_HW_DEV,
        VPC_HW_DEV,
        FM_HW_DEV,
        NB_HW_DEV
    };

    struct hw_module {
        const char* module_id;
        const char* module_name;
    };

    static const hw_module hw_module_list[NB_HW_DEV];

        // Defines tuning parameters in PFW XML config files and default values
        // ALSA PLATFORM CONFIGURATION
        enum ALSA_CONF_DIRECTION {
                ALSA_CONF_DIRECTION_IN,
                ALSA_CONF_DIRECTION_OUT,

                ALSA_CONF_NB_DIRECTIONS

        };
    static const char* const gapcDefaultSampleRates[ALSA_CONF_NB_DIRECTIONS];

    static const uint32_t DEFAULT_SAMPLE_RATE;

        // MODEM I2S PORTS
        enum IFX_IS2S_PORT {
                IFX_I2S1_PORT,
                IFX_I2S2_PORT,

                IFX_NB_I2S_PORT
        };
    static const char* const gapcModemPortClockSelection[IFX_NB_I2S_PORT];
    static const uint32_t DEFAULT_IFX_CLK_SELECT;

private:
    // PFW type value pairs type
    struct SSelectionCriterionTypeValuePair
    {
        int iNumerical;
        const char* pcLiteral;
    };
    // Used to fill types for PFW
    void fillSelectionCriterionType(ISelectionCriterionTypeInterface* pSelectionCriterionType, const SSelectionCriterionTypeValuePair* pSelectionCriterionTypeValuePairs, uint32_t uiNbEntries) const;

    // unsigned integer parameter value retrieval
    uint32_t getIntegerParameterValue(const string& strParameterPath, bool bSigned, uint32_t uiDefaultValue) const;

    // Mode type
    static const SSelectionCriterionTypeValuePair mModeValuePairs[];
    static const uint32_t mNbModeValuePairs;
    // Selected Input Device type
    static const SSelectionCriterionTypeValuePair mSelectedInputDeviceValuePairs[];
    static const uint32_t mNbSelectedInputDeviceValuePairs;
    // Selected Output Device type
    static const SSelectionCriterionTypeValuePair mSelectedOutputDeviceValuePairs[];
    static const uint32_t mNbSelectedOutputDeviceValuePairs;

    // The connector
    CParameterMgrPlatformConnector* mParameterMgrPlatformConnector;
    // Logger
    CParameterMgrPlatformConnectorLogger* mParameterMgrPlatformConnectorLogger;
    // Criteria Types
    ISelectionCriterionTypeInterface* mModeType;
    ISelectionCriterionTypeInterface* mInputDeviceType;
    ISelectionCriterionTypeInterface* mOutputDeviceType;
    // Criteria
    ISelectionCriterionInterface* mSelectedMode;
    ISelectionCriterionInterface* mSelectedInputDevice;
    ISelectionCriterionInterface* mSelectedOutputDevice;

    AudioRouteManager  *mAudioRouteMgr;
    CATManager *mATManager;
    CProgressUnsollicitedATCommand* mXProgressCmd;
    CCallStatUnsollicitedATCommand* mXCallstatCmd;

    // Modem Call state
    bool mModemCallActive;

    // Modem State
    bool mModemAvailable;

    // MSIC voice Route forced on MM flag
    bool mMSICVoiceRouteForcedOnMMRoute;

    // Output Streams list
    list<AudioStreamOutALSA*> mStreamOutList;

    // Input Streams list
    list<AudioStreamInALSA*> mStreamInList;

    // HW device array
    vector<hw_device_t*> mHwDeviceArray;

    // Reconsider Route force flag
    bool mForceReconsiderInCallRoute;

    // Current TTY Device
    vpc_tty_t mCurrentTtyDevice;

    // Current HAC Setting
    vpc_hac_set_t mCurrentHACSetting;

    //Current BT state
    bool mIsBluetoothEnabled;
};

// ----------------------------------------------------------------------------

};        // namespace android
#endif    // ANDROID_AUDIO_HARDWARE_ALSA_H
