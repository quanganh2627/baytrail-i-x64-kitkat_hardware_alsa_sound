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
#include <fm_module.h>

#include <utils/threads.h>
#include "AudioUtils.h"
#include "SampleSpec.h"
#include "Utils.h"

class CParameterMgrPlatformConnector;
class ISelectionCriterionTypeInterface;
class ISelectionCriterionInterface;

namespace android_audio_legacy
{

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
    virtual android::status_t    initCheck();

    /** set the audio volume of a voice call. Range is between 0.0 and 1.0 */
    virtual android::status_t    setVoiceVolume(float volume);

    /**
     * set the audio volume for all audio activities other than voice call.
     * Range between 0.0 and 1.0. If any value other than NO_ERROR is returned,
     * the software mixer will emulate this capability.
     */
    virtual android::status_t    setMasterVolume(float volume);

    // mic mute
    virtual android::status_t    setMicMute(bool state);
    virtual android::status_t    getMicMute(bool* state);

    // set/get global audio parameters
    virtual android::status_t    setParameters(const String8& keyValuePairs);
    //virtual String8     getParameters(const String8& keys);

    // set Stream Parameters
    virtual android::status_t    setStreamParameters(ALSAStreamOps* pStream, const String8 &keyValuePairs);


    // Returns audio input buffer size according to parameters passed or 0 if one of the
    // parameters is not supported
    virtual size_t    getInputBufferSize(uint32_t sampleRate, int format, int channels);

    /** This method creates and opens the audio hardware output stream */
    virtual AudioStreamOut* openOutputStream(
            uint32_t devices,
            int* format = NULL,
            uint32_t* channels = NULL,
            uint32_t* sampleRate = NULL,
            status_t* status = NULL);
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
    android::status_t startStream(ALSAStreamOps* pStream);

    android::status_t stopStream(ALSAStreamOps *pStream);

protected:
    virtual status_t    dump(int fd, const Vector<String16>& args);


    // Cast Hw device from mHwDeviceArray to the corresponding hw device type
    fm_device_t* getFmHwDevice();

    friend class AudioStreamOutALSA;
    friend class AudioStreamInALSA;
    friend class ALSAStreamOps;
    friend class CAudioRouteManager;
    friend class CAudioAutoRoutingLock;
    friend class CAudioConverter;

private:
    AudioHardwareALSA(const AudioHardwareALSA &);
    AudioHardwareALSA& operator = (const AudioHardwareALSA &);

    // Lock the routing
    void lockRouting();

    // Unlock the routing
    void unlockRouting();

    bool mMicMuteState;

    enum HW_DEVICE {
        FM_HW_DEV = 0,

        NB_HW_DEV
    };

    struct hw_module {
        const char* module_id;
        const char* module_name;
    };

    static const hw_module hw_module_list[NB_HW_DEV];

    static const uint32_t DEFAULT_SAMPLE_RATE;
    static const uint32_t DEFAULT_CHANNEL_COUNT;
    static const uint32_t DEFAULT_FORMAT;

    static const int32_t VOICE_GAIN_MAX;
    static const int32_t VOICE_GAIN_MIN;
    static const uint32_t VOICE_GAIN_OFFSET;
    static const uint32_t VOICE_GAIN_SLOPE;

    static const char* const DEFAULT_GAIN_PROP_NAME;
    static const float DEFAULT_GAIN_VALUE;
    static const char* const AUDIENCE_IS_PRESENT_PROP_NAME;
    static const bool AUDIENCE_IS_PRESENT_DEFAULT_VALUE;
    static const char* const FM_SUPPORTED_PROP_NAME;
    static const bool FM_SUPPORTED_PROP_DEFAULT_VALUE;
    static const char* const FM_IS_ANALOG_PROP_NAME;
    static const bool FM_IS_ANALOG_DEFAUT_VALUE;

private:
    CAudioRouteManager* mRouteMgr;

    // HW device array
    std::vector<hw_device_t*> mHwDeviceArray;
};

};        // namespace android
#endif    // ANDROID_AUDIO_HARDWARE_ALSA_H
