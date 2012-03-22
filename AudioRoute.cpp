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

#define INPUT_STREAM        false
#define OUTPUT_STREAM       true

namespace android_audio_legacy
{

AudioRoute::AudioRoute(const String8& mName) :
    mName(mName),
    mIsCaptureRouted(false)
{
    LOGD("AudioRoute %s", mName.string());
    mStreams[INPUT_STREAM] = NULL;
    mStreams[OUTPUT_STREAM] = NULL;
    mIsRouteAccessible[INPUT_STREAM] = true;
    mIsRouteAccessible[OUTPUT_STREAM] = true;
}

AudioRoute::~AudioRoute()
{

}

status_t AudioRoute::applyRoutingStrategy(int mode, bool bForOutput)
{
    LOGD("execute mode=%d", mode);

    status_t ret = NO_ERROR;

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
                (void)unRoute(INPUT_STREAM);
            }
            // Route the playback inconditionnaly (routing depends on it)
            LOGD("execute playback");
            ret = route(mode, OUTPUT_STREAM);

            if (ret != NO_ERROR) {

                return ret;
            }
            if(captureStream()) {
                // Now that the playback is routed, the route is established
                // We can now route the capture stream we unrouted previously
                LOGD("execute Playback -> Capture previously requested");
                ret = route(mode, INPUT_STREAM);

                if (ret != NO_ERROR) {

                    return ret;
                }
            }

        } else {
            // Capture is requested to be routed
            //      -> OK if playback is already routed
            //      -> NOK if no playback
            if(playbackStream()) {
                LOGD("execute capture (playback already routed)");
                ret = route(mode, INPUT_STREAM);
                if (ret != NO_ERROR) {

                    return ret;
                }
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
        ret = route(mode, bForOutput);
    }
    return ret;
}

status_t AudioRoute::route(int mode, bool bForOutput)
{
    LOGD("route mode=%d isAccessible=%d", mode, mIsRouteAccessible[bForOutput]);

    status_t ret = NO_ERROR;

    if(mIsRouteAccessible[bForOutput]) {
        if(bForOutput) {
            ret = playbackStream()->doRoute(mode);
        } else {
            ret = captureStream()->doRoute(mode);
            if (ret == NO_ERROR) {

                mIsCaptureRouted = true;
            }
        }
    }
    return ret;
}

status_t AudioRoute::unRoute(bool bForOutput)
{
    LOGD("unRoute isAvailable=%d", mIsRouteAccessible[bForOutput]);
    if(mIsRouteAccessible[bForOutput]) {
        if(bForOutput) {
            playbackStream()->undoRoute();
        } else {
            captureStream()->undoRoute();
            mIsCaptureRouted = false;
        }
    }
    return NO_ERROR;
}

status_t AudioRoute::setStream(ALSAStreamOps* pStream, int mode)
{
    LOGD("setStream mode=%d", mode);
    bool isOut = pStream->isOut();

    assert(!mStreams[isOut]);

    mStreams[isOut] = pStream;

    return applyRoutingStrategy(mode, isOut);
}

status_t AudioRoute::unsetStream(ALSAStreamOps* pStream, int mode)
{
    LOGD("unsetStream mode=%d", mode);
    bool isOut;
    isOut = pStream->isOut();
    assert(pStream && mStreams[isOut] && (pStream == mStreams[isOut]));

    // Unroute the stream
    unRoute(isOut);

    // We are requested to unroute an output stream
    // if playback and capture streams were tied and input stream
    // was already routed,
    // unroute input stream but keep the stream pointer
    if(isOut && tieStreams(mode) && mIsCaptureRouted) {
        unRoute(INPUT_STREAM);
    }

    // Reset the stream pointer and routed flag
    mStreams[isOut] = NULL;

    return NO_ERROR;
}

void AudioRoute::setRouteAccessible(bool isAccessible, int mode, Direction dir)
{
    LOGD("setRouteAccessible mIsRouteAccessible=%d", isAccessible);

    bool action_requested = false;
    if (dir & Capture) {
        if (mIsRouteAccessible[INPUT_STREAM] != isAccessible) {
            action_requested = true;
        }
    }
    if (dir & Playback) {
        if (mIsRouteAccessible[OUTPUT_STREAM] != isAccessible) {
            action_requested = true;
        }
    }

    if (!action_requested)
    {
        LOGD("setRouteAccessible Nothing to do");
        return ;
    }

    if(!isAccessible) {
        /*
         * The route is now unaccessible, meaning, no streams must be
         * opened on this route. Need to unroute them but keep these
         * streams attached to the route to recover when the route is
         * once again accessible
         */
        if ( dir & Capture) {
            if(captureStream() && mIsCaptureRouted) {
                LOGD("setRouteAccessible undoRoute capture stream");
                unRoute(INPUT_STREAM);
            }
            mIsRouteAccessible[INPUT_STREAM] = isAccessible;
        }
        if (dir & Playback) {
            if (playbackStream()) {
                LOGD("setRouteAccessible undoRoute playback stream");
                unRoute(OUTPUT_STREAM);
            }
            mIsRouteAccessible[OUTPUT_STREAM] = isAccessible;
        }
    } else {
        /* The route is now accessible, meaning, streams can be
         * opened again on this route if still attached to it.
         */
        if (dir & Playback) {

            mIsRouteAccessible[OUTPUT_STREAM] = isAccessible;

            if(playbackStream()) {
                LOGD("setRouteAccessible route playback stream");
                applyRoutingStrategy(mode, OUTPUT_STREAM);
            }
        }
        if ( dir & Capture) {

             mIsRouteAccessible[INPUT_STREAM] = isAccessible;

            if(captureStream()) {
                LOGD("setRouteAccessible route capture stream");
                applyRoutingStrategy(mode, INPUT_STREAM);
            }
        }
    }
}

bool AudioRoute::available(bool bForOutput)
{
    if(mIsRouteAccessible[bForOutput])
    {
        if(bForOutput) {
            if(playbackStream())
                return true;
        }
        else
            return mIsCaptureRouted;
    }
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
