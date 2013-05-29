/*
 **
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

#include <list>
#include <media/AudioBufferProvider.h>
#include <SampleSpec.h>

namespace android_audio_legacy {

class CAudioConverter;

class CAudioConversion {

    typedef std::list<CAudioConverter*>::iterator AudioConverterListIterator;
    typedef std::list<CAudioConverter*>::const_iterator AudioConverterListConstIterator;

public:

    CAudioConversion();
    virtual ~CAudioConversion();

    /**
     * Configures the conversion chain.
     * It configures the conversion chain that may be used to convert samples from the source
     * to destination sample specification. This configuration tries to order the list of converters
     * so that it minimizes the number of samples on which the resampling is done.
     *
     * @param[in] ssSrc source sample specifications.
     * @param[in] ssDst destination sample specifications.
     *
     * @return status OK, error code otherwise.
     */
    android::status_t configure(const CSampleSpec& ssSrc, const CSampleSpec& ssDst);

    /**
     * Converts audio samples.
     * It converts audio samples using the conversion chains that must be configured before.
     * Destination buffer may be given or not to minimize the number of copy. If not given,
     * allocation is done by the resampler. In this case, the ouput buffer will contain valid data
     * until next convert call or configure.
     *
     * @param[in] src buffer of samples to conversion.
     * @param[out] dst destination sample buffer. If the value pointer by dst
     *                 is null, the converter will allocated memory and give it back to the called.
     *                 This memory will be freed on next configure call or on destruction of the
     *                 instance of the converter.
     *                 If no error is returned, the ouput buffer will contain valid data until next
     *                 convert call or configure.
     * @param[in] inFrames number of frames in the source sample specification to convert.
     * @param[out] outFrames number of frames in the destination sample specification converted.
     *
     * @return status OK, error code otherwise.
     */
    android::status_t convert(const void* src, void** dst, const uint32_t inFrames, uint32_t *outFrames);

    /**
     * Converts audio samples and output an exact number of output frames.
     * The caller must give an AudioBufferProvider object that may implement getNextBuffer API
     * to feed the conversion chain.
     * The caller must allocate itself the destination buffer and garantee overflow will not happen.
     *
     * @param[out] dst pointer on the caller destination buffer.
     * @param[in] outFrames frames in the destination sample specification requested to be outputed.
     * @param[in:out] pBufferProvider object that will provide source buffer.
     *
     * @return status OK, error code otherwise.
     */
    android::status_t getConvertedBuffer(void* dst, const uint32_t outFrames, android::AudioBufferProvider *pBufferProvider);

private:
    CAudioConversion(const CAudioConversion&);
    CAudioConversion& operator = (const CAudioConversion&);

    /**
     * This function pushes the converter to the list
     * and alters the source sample spec according to the sample spec reached
     * after this convertion.
     *
     * Lets take an example:
     * ssSrc = { a, b, c } and ssDst = { a', b', c' } where:
     *              -a is the channel numbers,
     *              -b is the number of bytes used in the audio format.
     *              -c is the rate,
     *
     * Let s' take the assumption that our converter (SampleSpecItem input parameter) is a
     * resampler ie works on sample spec item b
     * After the converter, temporary destination sample spec will be: { a, b', c }
     *
     * Update the source Sample Spec to this temporary sample spec for the
     * next convertion that might have to be added.
     * ssSrc = temp dest = { a, b', c }
     *
     * @param[in] eConverterType sample spec item on which the converter is working
     * @param[in:out] ssSrc source sample specifications.
     * @param[in] ssDst destination sample specifications.
     *
     * @return status OK, error code otherwise.
     */
    android::status_t doConfigureAndAddConverter(SampleSpecItem eConverterType, CSampleSpec* pSsSrc, const CSampleSpec* pSsDst);

    /**
     * Recursive function to add converters to the chain of convertion required.
     *
     * When a converter is added, the source sample specification is modified to represents the
     * audio data sample specification AFTER applying this converter.
     * This sample spec will be used as the source for next convertion.
     * In order to minimize power consumption, resampling operation should be applied on the minimum
     * frame size. So, down-remapping or down-formatting will be done in prior of resampling.
     *
     * Let's take an example:
     * ssSrc = { a, b, c } and ssDst = { a', b', c' } where:
     *              -a / a' are the channel numbers,
     *              -b / b' are the number of bytes used in the audio format.
     *              -c / c' are the rates,
     * and with (a' > a) and (b' < b)
     * As all sample spec items are different, we need to use 3 converter to reach the destination
     * audio data sample specifications.
     *
     * First take into account a (number of channels):
     *      As a' is higher than a, first performs the remapping:
     *      ssSrc = { a, b, c } dst = { a', b', c' }
     *      The temporary output becomes the new source for next converter
     *      ssSrc = temporary Output = { a', b, c }
     *
     * Then, take into account b (format size).
     *      As b' is lower than b, do not perform the reformating now...
     *
     * Finally, take into account the sample rate:
     *      as they are different, use a resampler:
     *      ssSrc = { a', b, c } dst = { a', b', c' }
     *      The temporary output becomes the new source for next converter
     *      ssSrc = temporary Output = { a', b, c' }
     *
     * No more converter: exit from last recursive call
     * Taking into account b again...(format size)
     *      as b' < b, use a reformatter
     *      ssSrc = { a', b, c' } dst = { a', b', c' }
     *      The temporary output becomes the new source for next converter
     *      ssSrc = temporary Output = { a', b', c' }
     *
     * Exit from recursive call
     *
     * @param[in] eConverterType sample spec item on which the converter is working
     * @param[in:out] ssSrc source sample specifications.
     * @param[in] ssDst destination sample specifications.
     *
     * @return status OK, error code otherwise.
     */
    android::status_t configureAndAddConverter(SampleSpecItem eConverterType, CSampleSpec* pSsSrc, const CSampleSpec* pSsDst);

    void emptyConversionChain();

    // List of audio converter enabled
    std::list<CAudioConverter*> _activeAudioConvList;

    // List of Audio Converter objects available
    // (Each converter works on a dedicated sample spec item)
    CAudioConverter* _apAudioConverter[ENbSampleSpecItems];

    CSampleSpec   _ssSrc;
    CSampleSpec   _ssDst;

    // Conversion is done into ConvOutBuffer
    size_t _ulConvOutBufferIndex;
    size_t _ulConvOutFrames;
    size_t _ulConvOutBufferSizeInFrames;
    int16_t* _pConvOutBuffer;

    // Buffer is acquired from the provider into ConvInBuffer
    android::AudioBufferProvider::Buffer _pConvInBuffer;

    static const uint32_t MAX_RATE;

    static const uint32_t MIN_RATE;
};

}; // namespace android
