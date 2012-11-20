#pragma once

#include <stdint.h>
#include <sys/types.h>
#include <utils/Errors.h>
#include <tinyalsa/asoundlib.h>


using namespace android;

namespace android_audio_legacy {

// ----------------------------------------------------------------------------
class CSampleSpec;

class CAudioUtils
{
public:
    // use emulated popcount optimization
    // http://www.df.lth.se/~john_e/gems/gem002d.html
    static inline uint32_t popCount(uint32_t u)
    {
        u = ((u&0x55555555) + ((u>>1)&0x55555555));
        u = ((u&0x33333333) + ((u>>2)&0x33333333));
        u = ((u&0x0f0f0f0f) + ((u>>4)&0x0f0f0f0f));
        u = ((u&0x00ff00ff) + ((u>>8)&0x00ff00ff));
        u = ( u&0x0000ffff) + (u>>16);
        return u;
    }

    static inline uint32_t convertUsecToMsec(uint32_t uiTimeInUsec) { return ((uiTimeInUsec + 999) / 1000); }

    static uint32_t alignOn16(uint32_t u);

    static ssize_t convertSrcToDstInBytes(ssize_t bytes, const CSampleSpec& ssSrc, const CSampleSpec& ssDst);

    static ssize_t convertSrcToDstInFrames(ssize_t frames, const CSampleSpec& ssSrc, const CSampleSpec& ssDst);

    // This function retrieves the number of bytes
    // for one sample (per channel) according to the format
    static size_t formatSize(int format);

    // This fonction translates the format from ALSA lib
    // to AudioSystem enum
    static int convertTinyToHalFormat(pcm_format format);
    static pcm_format convertHalToTinyFormat(int format);

    /**
      * Constante used during convert of frames to delays in micro-seconds (us).
      * It is used for delays computation of AEC effect
      */
    static const uint32_t USEC_TO_SEC = 1000000000;
};

// ----------------------------------------------------------------------------
}; // namespace android

