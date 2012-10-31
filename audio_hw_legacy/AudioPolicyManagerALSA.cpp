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
#include "Property.h"

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

const char* const AudioPolicyManagerALSA::mFmRxAnalogSupportedPropName = "Audiocomms.FM.IsAnalog";
const bool AudioPolicyManagerALSA::mFmRxAnalogSupportedDefaultValue = false;

status_t AudioPolicyManagerALSA::setDeviceConnectionState(AudioSystem::audio_devices device,
                                                          AudioSystem::device_connection_state state,
                                                          const char *device_address)
{
    // Connect or disconnect only 1 device at a time
    if (AudioSystem::popCount(device) != 1) {
        return BAD_VALUE;
    }

    if (strlen(device_address) >= MAX_DEVICE_ADDRESS_LEN) {
        ALOGE("setDeviceConnectionState() invalid address: %s", device_address);
        return BAD_VALUE;
    }

    if (AudioSystem::isOutputDevice(device) && state == AudioSystem::DEVICE_STATE_AVAILABLE) {
        if (mAvailableOutputDevices & device) {
            ALOGW("setDeviceConnectionState() device already connected: %x", device);
            return INVALID_OPERATION;
        }

        ALOGV("setDeviceConnectionState() connecting device %x", device);

        // Clear wired headset/headphone device, if any already available, as only
        // one wired headset/headphone device can be connected at a time
        if (device & (AudioSystem::DEVICE_OUT_WIRED_HEADSET | AudioSystem::DEVICE_OUT_WIRED_HEADPHONE) ){
          mAvailableOutputDevices = (audio_devices_t) (mAvailableOutputDevices & ~(AudioSystem::DEVICE_OUT_WIRED_HEADSET|AudioSystem::DEVICE_OUT_WIRED_HEADPHONE));
        }
    }

    return baseClass::setDeviceConnectionState(device, state, device_address);
}


status_t AudioPolicyManagerALSA::startOutput(audio_io_handle_t output,
                                             AudioSystem::stream_type stream,
                                             int session)
{
    ALOGD("startOutput() output %d, stream type %d, session %d", output, stream, session);

    return baseClass::startOutput(output, stream, session);
}

audio_devices_t AudioPolicyManagerALSA::getDeviceForInputSource(int inputSource)
{
    audio_devices_t device;
    //  We should be able to use Skype during a CSV call
    if ((inputSource == AUDIO_SOURCE_MIC) && (mPhoneState == AudioSystem::MODE_IN_CALL)) {
        LOGV("getDeviceForInputSource() change device from mic to uplink voice in CSV call");
        device = AUDIO_DEVICE_IN_VOICE_CALL;
    } else {
        device = baseClass::getDeviceForInputSource(inputSource);
    }
    LOGV("getDeviceForInputSource()input source %d, device %08x", inputSource, device);
    return device;
}

audio_io_handle_t AudioPolicyManagerALSA::getInput(int inputSource,
                                                   uint32_t samplingRate,
                                                   uint32_t format,
                                                   uint32_t channelMask,
                                                   AudioSystem::audio_in_acoustics acoustics)
{
    audio_devices_t device = getDeviceForInputSource(inputSource);

    audio_io_handle_t activeInput = getActiveInput();
    audio_io_handle_t input = 0;

    // If one input (device in capture) is used then the policy shall refuse to any record
    // application to acquire another input, unless a VoIP call or a voice call record preempts.
    // Or unless the ref count is null, that means that there is an input created but not used,
    // and we can safely return its input handle.
    if (!mInputs.isEmpty() && activeInput) {
        AudioInputDescriptor *inputDesc = mInputs.valueFor(activeInput);

        uint32_t deviceMediaRecMic = (AudioSystem::DEVICE_IN_BUILTIN_MIC | AudioSystem::DEVICE_IN_BACK_MIC |
                                      AudioSystem::DEVICE_IN_BLUETOOTH_SCO_HEADSET | AudioSystem::DEVICE_IN_WIRED_HEADSET |
                                      AudioSystem::DEVICE_IN_FM_RECORD);

        // If an application uses already an input and the requested input is from a VoIP call
        // or a CSV call record, stop the current active input to enable requested input start.
        if(((inputDesc->mDevice & deviceMediaRecMic) &&
            (inputDesc->mInputSource != AUDIO_SOURCE_VOICE_COMMUNICATION)) &&
           ((device & AudioSystem::DEVICE_IN_VOICE_CALL) ||
            (inputSource == AUDIO_SOURCE_VOICE_COMMUNICATION))) {
              LOGI("Stop current active input %d because of higher priority input %d !", inputDesc->mInputSource, inputSource);
              baseClass::stopInput(activeInput);
        }
        else if ((AUDIO_SOURCE_CAMCORDER == inputDesc->mInputSource) &&
                 (AUDIO_SOURCE_MIC == inputSource)) {
            // Create a concurrent input and let upper layers close the active camcorder input
            LOGI("Grant request for input %d creation while current camcorder input", inputSource);
        }
        else if ((inputDesc->mDevice & AudioSystem::DEVICE_IN_VOICE_CALL) &&
                 (inputSource == AUDIO_SOURCE_VOICE_COMMUNICATION)) {
            LOGI("Incoming VoIP call during VCR or VCR -> VoIP swap");
        }
        else if ((inputDesc->mInputSource == AUDIO_SOURCE_VOICE_COMMUNICATION) &&
                 (inputSource == AUDIO_SOURCE_VOICE_RECOGNITION)){
            LOGI("Voice recognition requested during VoIP call");
        }
        else if ((inputDesc->mInputSource == AUDIO_SOURCE_MIC) &&
                 (inputSource == AUDIO_SOURCE_VOICE_RECOGNITION)){
            LOGI("Voice recognition requested while current MIC input source");
        }
        else {
            LOGW("getInput() mPhoneState : %d, device 0x%x, already one input used with other source, return invalid audio input handle!", mPhoneState, device);
            return 0;
        }
    }

    ALOGV("getInput() inputSource %d, samplingRate %d, format %d, channelMask %x, acoustics %x",
          inputSource, samplingRate, format, channelMask, acoustics);

    if (device == 0) {
        ALOGW("getInput() could not find device for inputSource %d", inputSource);
        return 0;
    }

    IOProfile *profile = getInputProfile(device,
                                         samplingRate,
                                         format,
                                         channelMask);
    if (profile == NULL) {
        ALOGW("getInput() could not find profile for device %04x, samplingRate %d, format %d,"
                "channelMask %04x",
                device, samplingRate, format, channelMask);
        return 0;
    }

    if (profile->mModule->mHandle == 0) {
        ALOGE("getInput(): HW module %s not opened", profile->mModule->mName);
        return 0;
    }

    AudioInputDescriptor *inputDesc = new AudioInputDescriptor(profile);

    inputDesc->mInputSource = inputSource;
    inputDesc->mDevice = device;
    inputDesc->mSamplingRate = samplingRate;
    inputDesc->mFormat = (audio_format_t)format;
    inputDesc->mChannelMask = (audio_channel_mask_t)channelMask;
    inputDesc->mRefCount = 0;
    input = mpClientInterface->openInput(profile->mModule->mHandle,
                                    &inputDesc->mDevice,
                                    &inputDesc->mSamplingRate,
                                    &inputDesc->mFormat,
                                    &inputDesc->mChannelMask);

    // only accept input with the exact requested set of parameters
    if (input == 0 ||
        (samplingRate != inputDesc->mSamplingRate) ||
        (format != inputDesc->mFormat) ||
        (channelMask != inputDesc->mChannelMask)) {
        ALOGV("getInput() failed opening input: samplingRate %d, format %d, channelMask %d",
                samplingRate, format, channelMask);
        if (input != 0) {
            mpClientInterface->closeInput(input);
        }
        delete inputDesc;
        return 0;
    }
    mInputs.add(input, inputDesc);

    // Do not call base class method as channel mask information
    // for voice call record is not used on our platform
    return input;
}

status_t AudioPolicyManagerALSA::startInput(audio_io_handle_t input)
{
    ALOGV("startInput() input %d", input);
    ssize_t index = mInputs.indexOfKey(input);
    if (index < 0) {
        ALOGW("startInput() unknow input %d", input);
        return BAD_VALUE;
    }
    AudioInputDescriptor *inputDesc = mInputs.valueAt(index);

#ifdef AUDIO_POLICY_TEST
    if (mTestInput == 0)
#endif //AUDIO_POLICY_TEST
    {
        // refuse 2 active AudioRecord clients at the same time
        // except in the case of incoming VOIP call during voice call recording
        audio_io_handle_t activeInput = getActiveInput();
        if (activeInput != 0) {
            AudioInputDescriptor *activeInputDesc = mInputs.valueFor(activeInput);
            if ((inputDesc->mInputSource & AUDIO_SOURCE_VOICE_COMMUNICATION) &&
                (activeInputDesc->mDevice & AudioSystem::DEVICE_IN_VOICE_CALL) &&
                ((activeInputDesc->mInputSource == AUDIO_SOURCE_VOICE_UPLINK) ||
                 (activeInputDesc->mInputSource == AUDIO_SOURCE_VOICE_DOWNLINK) ||
                 (activeInputDesc->mInputSource == AUDIO_SOURCE_VOICE_CALL))){
                ALOGI("startInput() input %d: VCR input %d already started", input, activeInput);
            } else {
                ALOGI("startInput() input %d failed: other input already started", input);
                return INVALID_OPERATION;
            }
        }
    }

    AudioParameter param = AudioParameter();
    param.addInt(String8(AudioParameter::keyRouting), (int)inputDesc->mDevice);

    param.addInt(String8(AudioParameter::keyInputSource), (int)inputDesc->mInputSource);
    ALOGV("startInput() input source = %d", inputDesc->mInputSource);

    mpClientInterface->setParameters(input, param.toString());

    inputDesc->mRefCount = 1;
    return NO_ERROR;
}

float AudioPolicyManagerALSA::computeVolume(int stream,
                                            int index,
                                            audio_io_handle_t output,
                                            audio_devices_t device)
{
    float volume = 1.0;

    // For CSV voice call, DTMF stream attenuation is only applied in the modem
    // Force volume to max for bluetooth SCO as volume is managed by the headset
    if ( ((stream == AudioSystem::DTMF) && (mPhoneState == AudioSystem::MODE_IN_CALL)) || (stream == AudioSystem::BLUETOOTH_SCO) ) {
        return volume;
    }

    // Compute SW attenuation
    volume = baseClass::computeVolume(stream, index, output, device);


    return volume;
}


status_t AudioPolicyManagerALSA::checkAndSetVolume(int stream,
                                                   int index,
                                                   audio_io_handle_t output,
                                                   audio_devices_t device,
                                                   int delayMs,
                                                   bool force)
{
    if (mFmRxAnalogSupported){
        //if FM is on, update FM Rx Volume
        if(stream == AudioSystem::FM_RX){
            float volume = computeVolume(stream, index, output, device);
            if (volume != mOutputs.valueFor(output)->mCurVolume[stream] ||
                    force) {
                mOutputs.valueFor(output)->mCurVolume[stream] = volume;
                ALOGV("checkAndSetVolume() for output %d stream %d (FM RX), volume %f, delay %d", output, stream, volume, delayMs);
                return mpClientInterface->setFmRxVolume(volume, delayMs);
            }
            return NO_ERROR;
        }
    }
    return baseClass::checkAndSetVolume(stream, index, output, device, delayMs, force);
}


audio_devices_t AudioPolicyManagerALSA::getDeviceForStrategy(routing_strategy strategy, bool fromCache)
{

    uint32_t device = 0;

    device = baseClass::getDeviceForStrategy(strategy, fromCache);
    AudioOutputDescriptor *hwOutputDesc = mOutputs.valueFor(mPrimaryOutput);
    uint32_t currentDevice = (uint32_t)hwOutputDesc->device();

    switch (strategy) {
        case STRATEGY_PHONE:
            // in voice call, the ouput device can not be DGTL_DOCK_HEADSET, AUX_DIGITAL (i.e. HDMI) or  ANLG_DOCK_HEADSET
            if ( ( device == AudioSystem::DEVICE_OUT_AUX_DIGITAL)       ||
                 ( device == AudioSystem::DEVICE_OUT_ANLG_DOCK_HEADSET) ||
                 ( device == AudioSystem::DEVICE_OUT_DGTL_DOCK_HEADSET) ||
                 ( device == AudioSystem::DEVICE_OUT_WIDI)              ||
                 ( device == AUDIO_DEVICE_OUT_USB_ACCESSORY)            ||
                 ( device == AUDIO_DEVICE_OUT_USB_DEVICE) ) {
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
        case STRATEGY_MEDIA:
            // We want to supersede the default policy when we are in call, so that we can change the volume
            // settings for MEDIA streams when in call, on BT SCO, SPEAKER or EARPIECE.
            if (isInCall()){
                // Retrieve the device in use for the call
                AudioOutputDescriptor *hwOutputDesc = mOutputs.valueFor(mPrimaryOutput);
                uint32_t currentDevice = (uint32_t)hwOutputDesc->device();
                if ((currentDevice == AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT)
                    || (currentDevice == AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET)
                    || (currentDevice == AudioSystem::DEVICE_OUT_BLUETOOTH_SCO)
                    || (currentDevice == AudioSystem::DEVICE_OUT_SPEAKER)
                    || (currentDevice == AudioSystem::DEVICE_OUT_EARPIECE)){
                    ALOGV("%s- superseding STRATEGY_MEDIA policy while in call from device 0x%x to 0x%x", __FUNCTION__, device, currentDevice);
                    device = currentDevice;
                }
            }
            break;
        case STRATEGY_ENFORCED_AUDIBLE:
                if ( isInCall() && !mStreams[AUDIO_STREAM_ENFORCED_AUDIBLE].mCanBeMuted
                     && ((getForceUse(AudioSystem::FOR_COMMUNICATION) == AudioSystem::FORCE_BT_SCO)
                         || (currentDevice == AudioSystem::DEVICE_OUT_EARPIECE)) ) {
                        // Set 2 devices for ALSA module : so the mode will be set as normal
                        //  - the BT SCO will be unrouted for instance during a shutter sound
                        //  during CSV call.
                        //  - the earpiece is not handled by the base class on MEDIA fall through case.
                        ALOGD("%s- current device(0x%x) and ihf are concerned by Enforced Audible strategy", __FUNCTION__, currentDevice);
                        device = (mAvailableOutputDevices & AudioSystem::DEVICE_OUT_SPEAKER) | currentDevice;
                }
            break;

       default:
         // do nothing
         break;
    }

    LOGV("getDeviceForStrategy() strategy %d, device 0x%x", strategy, device);

    return (audio_devices_t)device;
}

AudioPolicyManagerALSA::AudioPolicyManagerALSA(AudioPolicyClientInterface *clientInterface)
    : baseClass(clientInterface)
{
    mFmRxAnalogSupported = TProperty<bool>(mFmRxAnalogSupportedPropName, mFmRxAnalogSupportedDefaultValue);
    // check if earpiece device is supported
//    updateDeviceSupport("audiocomms.dev.earpiece.present", AUDIO_DEVICE_OUT_EARPIECE);
//    // check if back mic device is supported
//    updateDeviceSupport("audiocomms.dev.backmic.present", AUDIO_DEVICE_IN_BACK_MIC);
}

AudioPolicyManagerALSA::~AudioPolicyManagerALSA()
{
}

}; // namespace android
