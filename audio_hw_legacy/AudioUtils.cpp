#include "AudioUtils.h"
#include <system/audio.h>
#include <utils/Log.h>
#include "SampleSpec.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioUtils"

using namespace android;
using namespace std;

namespace android_audio_legacy {

// ----------------------------------------------------------------------------

#define FRAME_ALIGNEMENT_ON_16  16

uint32_t CAudioUtils::alignOn16(uint32_t u)
{
    return (u + (FRAME_ALIGNEMENT_ON_16 - 1)) & ~(FRAME_ALIGNEMENT_ON_16 - 1);
}

ssize_t CAudioUtils::convertSrcToDstInBytes(ssize_t bytes, const CSampleSpec& ssSrc, const CSampleSpec& ssDst)
{
    return ssDst.convertFramesToBytes(convertSrcToDstInFrames(ssSrc.convertBytesToFrames(bytes), ssSrc, ssDst));
}

ssize_t CAudioUtils::convertSrcToDstInFrames(ssize_t frames, const CSampleSpec& ssSrc, const CSampleSpec& ssDst)
{
    assert(ssSrc.getSampleRate());
    return (frames * ssDst.getSampleRate() + ssSrc.getSampleRate() - 1) / ssSrc.getSampleRate();
}

// This function retrieves the number of bytes
// for one sample (per channel) according to the format
size_t CAudioUtils::formatSize(int format)
{
    size_t sz;
    switch(format) {

    case AUDIO_FORMAT_PCM_8_BIT:
        sz = 1;
        break;
    case AUDIO_FORMAT_PCM_16_BIT:
        sz = 2;
        break;
    case AUDIO_FORMAT_PCM_8_24_BIT:
    case AUDIO_FORMAT_PCM_32_BIT:
        sz = 4;
        break;
    default:
        LOGE("%s: format not recognized", __FUNCTION__);
        sz = AUDIO_FORMAT_INVALID;
        break;
    }
    return sz;
}

// This fonction translates the format from ALSA lib
// to AudioSystem enum
int CAudioUtils::convertSndToHalFormat(snd_pcm_format_t format)
{
    int convFormat;

    switch(format) {

    case SND_PCM_FORMAT_S8:
        convFormat = AUDIO_FORMAT_PCM_8_BIT;
        break;
    case SND_PCM_FORMAT_S16_LE:
        convFormat = AUDIO_FORMAT_PCM_16_BIT;
        break;
    case SND_PCM_FORMAT_S24_LE:
        convFormat = AUDIO_FORMAT_PCM_8_24_BIT;
        break;
    case SND_PCM_FORMAT_S32_LE:
        convFormat = AUDIO_FORMAT_PCM_32_BIT;
        break;
    default:
        LOGE("%s: format not recognized", __FUNCTION__);
        convFormat = AUDIO_FORMAT_INVALID;
        break;
    }
    return convFormat;
}

// ----------------------------------------------------------------------------
}; // namespace android

