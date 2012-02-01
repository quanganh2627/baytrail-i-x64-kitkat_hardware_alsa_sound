/* AudioRoute.h
 **
 ** Copyright 2011 Intel Corporation
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

#ifndef ANDROID_AUDIO_ROUTE_H
#define ANDROID_AUDIO_ROUTE_H

#include <utils/String8.h>

#include <hardware_legacy/AudioHardwareBase.h>

namespace android_audio_legacy
{
class ALSAStreamOps;
class AudioRoute;

class AudioRoute
{
public:
    enum Direction {
        Capture = 0x1,
        Playback = 0x2,
        FullDuplex = 0x3
    };

    AudioRoute(const String8& strName);
    virtual           ~AudioRoute();


    virtual bool isApplicable(uint32_t devices, int mode, bool bForOutput) = 0;

    status_t applyRoutingStrategy(int mode, bool bForOutput);

    status_t setStream(ALSAStreamOps* pStream, int mode);

    status_t unsetStream(ALSAStreamOps* pStream, int mode);

    bool available(bool bForOutput);

    void setRouteAccessible(bool isAccessible, int mode, Direction dir = FullDuplex);

    const String8& getName() const { return mName; }

private:
    status_t route(int mode, bool bForOutput);

    status_t unRoute(bool bForOutput);

    static bool tieStreams(int mode);

    ALSAStreamOps* captureStream() { return mStreams[0]; }

    ALSAStreamOps* playbackStream() { return mStreams[1]; }

protected:
    AudioRoute(const AudioRoute &);
    AudioRoute& operator = (const AudioRoute &);


private:
    String8 mName;
    ALSAStreamOps* mStreams[2];
    bool mIsCaptureRouted;
    bool mIsRouteAccessible[2];
};
// ----------------------------------------------------------------------------

};        // namespace android
#endif    // ANDROID_AUDIO_ROUTE_H
