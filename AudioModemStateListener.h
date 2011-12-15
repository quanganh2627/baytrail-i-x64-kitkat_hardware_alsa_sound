/* AudioModemStateListener.h
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

#ifndef ANDROID_AUDIO_MODEM_LISTENER_H
#define ANDROID_AUDIO_MODEM_LISTENER_H

//#define TEST_WITH_DUMMY_STMD

#ifdef TEST_WITH_DUMMY_STMD
#include "test-app/dummy_stmd.h"
/* Sockets name */
#define SOCKET_NAME_MODEM_STATUS SOCKET_NAME_DUMMY_MODEM_STATUS

/* STMD -> Clients protocol */
#define MODEM_DOWN              DUMMY_MODEM_DOWN
#define MODEM_UP                DUMMY_MODEM_UP
#define PLATFORM_SHUTDOWN       DUMMY_PLATFORM_SHUTDOWN
#define MODEM_COLD_RESET        DUMMY_MODEM_COLD_RESET

/* Clients -> STMD protocol */
#define MODEM_COLD_RESET_ACK    DUMMY_MODEM_COLD_RESET_ACK
#else
#include "stmd.h"
#endif

namespace android_audio_legacy
{

class ALSAStreamOps;
class AudioRoute;

class AudioModemStateListener
{
//    friend class AudioRoute;
    typedef List<AudioRoute*>::iterator AudioRouteListIterator;

public:
    AudioModemStateListener(AudioModemStateObserver *obs);
    virtual           ~AudioModemStateListener();

    const bool& isStarted() const { return mStarted; }
    const int& getModemStatus() const { return mModemStatus; }
    status_t start();
    status_t stop();

private:
    AudioModemStateListener(const AudioModemStateListener &);
    AudioModemStateListener& operator = (const AudioModemStateListener &);

    AudioRoute* getRoute(uint32_t devices, int mode, bool bForOutput);
    friend class AudioModemStateObserver;
    static void* threadStart(void *obj);
    static void threadExitHandler(int unused);
    void setSignalHandler();
    void setModemStatus(int status);

    /* thread main loop */
    void run();

    /* send cold reset ack to stmd through rw socket */
    bool sendModemColdResetAck();

private:
    int mFdSocket;
    bool mStarted;
    int mModemStatus;
    pthread_t mThread;
    pthread_mutex_t mMutex;
    AudioModemStateObserver*     mObserver;
};
// ----------------------------------------------------------------------------

};        // namespace android
#endif    // ANDROID_AUDIO_MODEM_LISTENER_H
