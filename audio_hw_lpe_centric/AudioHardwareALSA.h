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

#include <tinyalsa/asoundlib.h>

#include <hardware/hardware.h>
#include <vpc_hardware.h>
#include <fm_module.h>

#include <utils/threads.h>

#include "AudioHardwareALSACommon.h"
#include "AudioUtils.h"
#include "SampleSpec.h"

class CParameterMgrPlatformConnector;
class ISelectionCriterionTypeInterface;
class ISelectionCriterionInterface;

using namespace std;
using namespace android;

namespace android_audio_legacy
{

using android::List;
using android::Mutex;

class CParameterMgrPlatformConnectorLogger;
class AudioHardwareALSA;
class CAudioRouteManager;
class CAudioRoute;
class CAudioStreamRoute;
class CAudioAutoRoutingLock;
class CAudioResampler;
class CAudioConverter;
class CAudioConversion;
class AudioStreamOutALSA;
class AudioStreamInALSA;
class ALSAStreamOps;

const uint32_t DEVICE_OUT_BLUETOOTH_SCO_ALL = AudioSystem::DEVICE_OUT_BLUETOOTH_SCO | AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET | AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT;


class AudioHardwareALSA : public AudioHardwareBase
{
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

    virtual status_t    setFmRxMode(int mode);

    // mic mute
    virtual status_t    setMicMute(bool state);
    virtual status_t    getMicMute(bool* state);

    // set/get global audio parameters
    virtual status_t    setParameters(const String8& keyValuePairs);
    //virtual String8     getParameters(const String8& keys);

    // set Stream Parameters
    virtual status_t    setStreamParameters(ALSAStreamOps* pStream, const String8 &keyValuePairs);


    // Returns audio input buffer size according to parameters passed or 0 if one of the
    // parameters is not supported
    virtual size_t    getInputBufferSize(uint32_t sampleRate, int format, int channels);

    /** This method creates and opens the audio hardware output stream */
    virtual AudioStreamOut* openOutputStream(
            uint32_t devices,
            int* format=0,
            uint32_t* channels=0,
            uint32_t* sampleRate=0,
            status_t* status=0);
    virtual    void        closeOutputStream(AudioStreamOut* out);

    /** This method creates and opens the audio hardware input stream */
    virtual AudioStreamIn* openInputStream(
            uint32_t devices,
            int* format,
            uint32_t* channels,
            uint32_t* sampleRate,
            status_t* status,
            AudioSystem::audio_in_acoustics acoustics);
    virtual    void        closeInputStream(AudioStreamIn* in);

    /**This method dumps the state of the audio hardware */
    //virtual status_t dumpState(int fd, const Vector<String16>& args);

    static AudioHardwareInterface* create();

    int                 mode()
    {
        return mMode;
    }

    // Reconsider the routing
    status_t startStream(ALSAStreamOps* pStream);

    status_t stopStream(ALSAStreamOps *pStream);

protected:
    virtual status_t    dump(int fd, const Vector<String16>& args);


    // Cast Hw device from mHwDeviceArray to the corresponding hw device type
    alsa_device_t* getAlsaHwDevice() const;

    fm_device_t* getFmHwDevice() const;
    acoustic_device_t* getAcousticHwDevice() const;

    friend class AudioStreamOutALSA;
    friend class AudioStreamInALSA;
    friend class ALSAStreamOps;
    friend class CAudioRouteManager;
    friend class CAudioAutoRoutingLock;
    friend class CAudioConverter;

    int getFmRxMode() { return mFmRxMode; }
    int getPrevFmRxMode() { return mPrevFmRxMode; }

private:
    AudioHardwareALSA(const AudioHardwareALSA &);
    AudioHardwareALSA& operator = (const AudioHardwareALSA &);

    // Lock the routing
    void lockRouting();

    // Unlock the routing
    void unlockRouting();

    // unsigned integer parameter value retrieval
    uint32_t getIntegerParameterValue(const string& strParameterPath, bool bSigned, uint32_t uiDefaultValue) const;

    bool mMicMuteState;

    enum HW_DEVICE {
        ALSA_HW_DEV = 0,
//        ACOUSTIC_HW_DEV,
//        FM_HW_DEV,
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
    static const char* gpcVoiceVolume;
    static const uint32_t DEFAULT_SAMPLE_RATE;
    static const uint32_t DEFAULT_CHANNEL_COUNT;
    static const uint32_t DEFAULT_FORMAT;

        // MODEM I2S PORTS
        enum IFX_IS2S_PORT {
                IFX_I2S1_PORT,
                IFX_I2S2_PORT,

                IFX_NB_I2S_PORT
        };
    static const char* const gapcModemPortClockSelection[IFX_NB_I2S_PORT];
    static const uint32_t DEFAULT_IFX_CLK_SELECT;

    static const char* const mDefaultGainPropName;
    static const float mDefaultGainValue;
    static const char* const mAudienceIsPresentPropName;
    static const bool mAudienceIsPresentDefaultValue;
    static const char* const mModemEmbeddedPropName;
    static const bool mModemEmbeddedDefaultValue;

private:
    CAudioRouteManager* mRouteMgr;

    // HW device array
    vector<hw_device_t*> mHwDeviceArray;
};

// ----------------------------------------------------------------------------

};        // namespace android
#endif    // ANDROID_AUDIO_HARDWARE_ALSA_H
