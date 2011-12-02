/* AudioStreamInALSA.cpp
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

#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>

#define LOG_TAG "AudioRoute"
#include <utils/Log.h>
#include <utils/String8.h>

#include <cutils/properties.h>
#include <media/AudioRecord.h>
#include <hardware_legacy/power.h>

#include "AudioHardwareALSA.h"
#include "AudioRoute.h"

namespace android_audio_legacy
{

AudioRoute::AudioRoute(const String8& mName) :
    mName(mName), mStreams({NULL, NULL}),
    mIsCaptureRouted(false)
{
    LOGD("AudioRoute %s", mName.string());
}

AudioRoute::~AudioRoute()
{

}

status_t AudioRoute::apply(int mode, bool bForOutput)
{
    LOGD("execute mode=%d", mode);

    //
    // Streams are tied, they must follow each other
    // and only the routing of the playback stream
    // will connect the route
    //
    if(tieStreams(mode)) {
        // Route Request on Playback (Output) stream
        if(bForOutput) {
            if(mIsCaptureRouted) {
                // if an input stream is right now using this route
                // unroute input stream
                mIsCaptureRouted = false;
                captureStream()->undoRoute();
            }
            // Route the playback inconditionnaly (routing depends on it)
            LOGD("execute playback");
            route(mode, true);
            if(captureStream()) {
                // Now that the playback is routed, the route is established
                // We can now route the capture stream we unrouted previously
                LOGD("execute Playback -> Capture previously requested");
                route(mode, false);
            }

        } else {
            // Capture is requested to be routed
            //      -> OK if playback is already routed
            //      -> NOK if no playback
            if(playbackStream()) {
                LOGD("execute capture (playback already routed)");
                route(mode, false);
                assert(!mIsCaptureRouted);
            } else {
                // Else: nothing to do, capture stream is attached to the route
                // but considered as not routed
                assert(mIsCaptureRouted);
            }
        }
    } else {
        LOGD("execute route inconditionnaly");
        //
        // Streams are not tied, route inconditionnaly
        //
        route(mode, bForOutput);
    }
    return NO_ERROR;
}

status_t AudioRoute::route(int mode, bool bForOutput)
{
    LOGD("route mode=%d", mode);

    if(bForOutput) {
        playbackStream()->doRoute(mode);
    } else {
        captureStream()->doRoute(mode);
        mIsCaptureRouted = true;
    }
    return NO_ERROR;
}

status_t AudioRoute::setStream(ALSAStreamOps* pStream, int mode)
{
    LOGD("setStream mode=%d", mode);
    bool isOut = pStream->isOut();

    assert(!mStreams[isOut]);

    mStreams[isOut] = pStream;

    apply(mode, isOut);

    return NO_ERROR;
}

status_t AudioRoute::unsetStream(ALSAStreamOps* pStream, int mode)
{
    LOGD("unsetStream mode=%d", mode);
    bool isOut;
    assert(pStream && mStreams[isOut]);
    isOut = pStream->isOut();

    // Unroute the stream
    pStream->undoRoute();

    // We are requested to unroute an output stream
    // if playback and capture streams were tied and input stream
    // was already routed,
    // unroute input stream but keep the stream pointer
    if(isOut && tieStreams(mode) && mIsCaptureRouted) {
        mIsCaptureRouted = false;
        captureStream()->undoRoute();
    }

    // Reset the stream pointer and routed flag
    mStreams[isOut] = NULL;
    if(!isOut)
        mIsCaptureRouted =  false;

    return NO_ERROR;
}

bool AudioRoute::available(bool bForOutput)
{
    if(bForOutput) {
        if(playbackStream())
            return true;
    }
    else
        return mIsCaptureRouted;

    return false;
}

bool AudioRoute::tieStreams(int mode)
{
    LOGD("tieStreams mode=%d", mode);

    if(mode == AudioSystem::MODE_IN_COMMUNICATION)
        return true;

    return false;
}

}       // namespace android
