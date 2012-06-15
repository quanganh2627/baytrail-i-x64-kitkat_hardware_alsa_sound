/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "AudioPolicyManagerALSA"
//#define LOG_NDEBUG 0
#include <utils/Log.h>
#include "AudioPolicyManagerALSA.h"
#include <media/mediarecorder.h>
#include <BooleanProperty.h>

#define baseClass AudioPolicyManagerBase


namespace android_audio_legacy {

// ----------------------------------------------------------------------------
// AudioPolicyManagerALSA
// ----------------------------------------------------------------------------

// ---  class factory

extern "C" AudioPolicyInterface* createAudioPolicyManager(AudioPolicyClientInterface *clientInterface)
{
    return new AudioPolicyManagerALSA(clientInterface);
}

extern "C" void destroyAudioPolicyManager(AudioPolicyInterface *interface)
{
    delete interface;
}

audio_io_handle_t AudioPolicyManagerALSA::getInput(int inputSource,
                                                   uint32_t samplingRate,
                                                   uint32_t format,
                                                   uint32_t channels,
                                                   AudioSystem::audio_in_acoustics acoustics)
{
    uint32_t device = getDeviceForInputSource(inputSource);

    audio_io_handle_t activeInput = getActiveInput();

    // If one input (device in capture) is used then the policy shall refuse to any record
    // application to acquire another input, unless a VoIP call or a voice call record preempts.
    // Or unless the ref count is null, that means that there is an input created but not used,
    // and we can safely return its input handle.
    if (!mInputs.isEmpty() && activeInput) {
        AudioInputDescriptor *inputDesc = mInputs.valueFor(activeInput);

        uint32_t deviceMediaRecMic = (AudioSystem::DEVICE_IN_BUILTIN_MIC | AudioSystem::DEVICE_IN_BACK_MIC |
                                      AudioSystem::DEVICE_IN_BLUETOOTH_SCO_HEADSET | AudioSystem::DEVICE_IN_WIRED_HEADSET);

        // If an application uses already an input and the requested input is from a VoIP call
        // or a CSV call record, stop the current active input to enable requested input start.
        if(((inputDesc->mDevice & deviceMediaRecMic) &&
            (inputDesc->mInputSource != AUDIO_SOURCE_VOICE_COMMUNICATION)) &&
           ((device & AudioSystem::DEVICE_IN_VOICE_CALL) ||
            (inputSource == AUDIO_SOURCE_VOICE_COMMUNICATION))) {
              LOGI("Stop current active input %d because of higher priority input %d !", inputDesc->mInputSource, inputSource);
              baseClass::stopInput(activeInput);
        }
        else {
            LOGW("getInput() mPhoneState : %d, device 0x%x, already one input used with other source, return invalid audio input handle!", mPhoneState, device);
            return 0;
        }
    }

    return baseClass::getInput(inputSource, samplingRate, format, channels, acoustics);
}

float AudioPolicyManagerALSA::computeVolume(int stream,
                                            int index,
                                            audio_io_handle_t output,
                                            audio_devices_t device)
{
    float volume = 1.0;

    // For CSV voice call, DTMF stream attenuation is only applied in the modem
    if ( (stream == AudioSystem::DTMF) && (mPhoneState == AudioSystem::MODE_IN_CALL) ) {
        return volume;
    }

    // Compute SW attenuation
    volume = baseClass::computeVolume(stream, index, output, device);


    return volume;
}


audio_devices_t AudioPolicyManagerALSA::getDeviceForStrategy(routing_strategy strategy, bool fromCache)
{

    uint32_t device = 0;

    device = baseClass::getDeviceForStrategy(strategy, fromCache);

    switch (strategy) {
        case STRATEGY_PHONE:
            // in voice call, the ouput device can not be DGTL_DOCK_HEADSET, AUX_DIGITAL (i.e. HDMI) or  ANLG_DOCK_HEADSET
            if ( ( device == AudioSystem::DEVICE_OUT_AUX_DIGITAL) ||
                 ( device == AudioSystem::DEVICE_OUT_ANLG_DOCK_HEADSET) ||
                 ( device == AudioSystem::DEVICE_OUT_DGTL_DOCK_HEADSET) ) {
                uint32_t forceUseInComm =  getForceUse(AudioSystem::FOR_COMMUNICATION);
                switch (forceUseInComm) {

                    case AudioSystem::FORCE_SPEAKER:
                        device = mAvailableOutputDevices & AudioSystem::DEVICE_OUT_SPEAKER;
                        if (device != 0) {
                            ALOGD("%s- Unsupported device in STRATEGY_PHONE: set Speaker as ouput", __FUNCTION__);
                        } else {
                            LOGE("%s- Earpiece device not found", __FUNCTION__);
                        }
                        break;

                    default:
                        device = mAvailableOutputDevices & AudioSystem::DEVICE_OUT_EARPIECE;
                        if (device != 0) {
                            ALOGD("%s- Unsupported device in STRATEGY_PHONE: set Earpiece as ouput", __FUNCTION__);
                        } else {
                            LOGE("%s- Earpiece device not found: set speaker as output", __FUNCTION__);
                            device = mAvailableOutputDevices & AudioSystem::DEVICE_OUT_SPEAKER;
                        }
                        break;
                }
            }

            break;

        case STRATEGY_ENFORCED_AUDIBLE:
            {
                AudioOutputDescriptor *hwOutputDesc = mOutputs.valueFor(mHardwareOutput);
                uint32_t currentDevice = (uint32_t)hwOutputDesc->device();

                LOGD("Enforced audible stream will be output on current device + speaker");

                // If the earpiece is used for the ongoing call, then add it to the output devices
                if(isInCall() && currentDevice == AudioSystem::DEVICE_OUT_EARPIECE) {
                    device |= AudioSystem::DEVICE_OUT_EARPIECE;
                }

                // Strip the earpiece from the output devices when we output on HDMI or WIDI
                if (isInCall() && hasEarpiece() &&
                    (mAvailableOutputDevices & (AudioSystem::DEVICE_OUT_AUX_DIGITAL |
                                                AudioSystem::DEVICE_OUT_WIDI_LOOPBACK))) {
                    device &= ~(AudioSystem::DEVICE_OUT_EARPIECE);
                }
            }
        // FALL THROUGH
        case STRATEGY_SONIFICATION:
        case STRATEGY_SONIFICATION_LOCAL:
                if (getForceUse(AudioSystem::FOR_MEDIA) == AudioSystem::FORCE_BT_SCO) {
                    //in this case play only on local - limitation of our HW cannot play on both sco+ihf
                    device = mAvailableOutputDevices & AudioSystem::DEVICE_OUT_SPEAKER;
                }
                break;

        case STRATEGY_MEDIA:
            if (getForceUse(AudioSystem::FOR_MEDIA) == AudioSystem::FORCE_BT_SCO) {
                device = mAvailableOutputDevices & (AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT |
                                                                AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET |
                                                                AudioSystem::DEVICE_OUT_BLUETOOTH_SCO);
                if (device != 0) {
                    LOGD("Request to play on BT SCO device");
                } else {
                    LOGE("%s- BT SCO device not found", __FUNCTION__);
                }
            }
            break;
         default:
            // do nothing
            break;
    }

    LOGV("getDeviceForStrategy() strategy %d, device 0x%x", strategy, device);

    return (audio_devices_t)device;
}

void AudioPolicyManagerALSA::updateDeviceSupport(const char * property, audio_devices_t device)

{
    audio_devices_t * pAvailableDevice;

    // Input or Output device ?
    pAvailableDevice = device & AUDIO_DEVICE_OUT_ALL ? &mAvailableOutputDevices : &mAvailableInputDevices;
    // Check the device property, if the property is not specified then the device is supported by default
    CBooleanProperty deviceProp(property, true);
    if (deviceProp.isSet()) {
        ALOGD("%s: Device 0x%08X supported", __FUNCTION__, device);
        *pAvailableDevice = (audio_devices_t)(*pAvailableDevice | device);
    } else {
        ALOGD("%s: Device 0x%08X not supported", __FUNCTION__, device);
        *pAvailableDevice = (audio_devices_t)(*pAvailableDevice & ~device);
    }
}

AudioPolicyManagerALSA::AudioPolicyManagerALSA(AudioPolicyClientInterface *clientInterface)
    : baseClass(clientInterface)
{
    // check if earpiece device is supported
//    updateDeviceSupport("audiocomms.dev.earpiece.present", AUDIO_DEVICE_OUT_EARPIECE);
//    // check if back mic device is supported
//    updateDeviceSupport("audiocomms.dev.backmic.present", AUDIO_DEVICE_IN_BACK_MIC);
}

AudioPolicyManagerALSA::~AudioPolicyManagerALSA()
{
}

}; // namespace android
