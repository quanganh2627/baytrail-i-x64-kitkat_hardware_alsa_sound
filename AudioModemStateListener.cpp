/* AudioModemStateListener.cpp
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
#include <string>
#include <pthread.h>
#include <cutils/sockets.h>

#define LOG_TAG "AudioModemStateListener"
#include <utils/Log.h>

#include "AudioHardwareALSA.h"
#include "AudioModemStateListener.h"
#include "AudioModemStateObserver.h"

#define SLEEP_TIME_MSEC             20
#define SOCKET_OPEN_TRY_MAX         25

namespace android_audio_legacy
{

AudioModemStateListener::AudioModemStateListener(AudioModemStateObserver *obs) :
    mFdSocket(-1),
    mStarted(false),
    mModemStatus(MODEM_DOWN),
    mObserver(obs)
{
    LOGD("AudioModemStateListener");
}

AudioModemStateListener::~AudioModemStateListener()
{
    if(mStarted)
        stop();
}

status_t AudioModemStateListener::start()
{
    LOGD("start");

    /*
     * In case of STMD not started, try every 20ms to reconnect to the socket
     * after a failed attemp. Exit the loop after 1s
     */
    int it = 0;
    while(++it < SOCKET_OPEN_TRY_MAX) {
        mFdSocket = socket_local_client(SOCKET_NAME_MODEM_STATUS,
                ANDROID_SOCKET_NAMESPACE_RESERVED,
                SOCK_STREAM);
        if (mFdSocket < 0)
        {
            LOGD("Attempt #%d to connect to modem-status socket failed (%s)..retry after %d ms", it, strerror(errno), SLEEP_TIME_MSEC);
            usleep(SLEEP_TIME_MSEC * 1000);
        }
        else
            break;
    }

    if (mFdSocket < 0)
    {
        LOGE("Failed to connect to modem-status socket %s\n", strerror(errno));
        close(mFdSocket);
        return NO_INIT;
    }

    if(pthread_create(&mThread, NULL, AudioModemStateListener::threadStart, this))
        return -1;
    mStarted = true;
    return NO_ERROR;
}

status_t AudioModemStateListener::stop()
{
    void *ret;
    int   status;

    if(!mStarted)
        return NO_ERROR;

    /* Ensure thread is really started */
    if (mThread == -1)
    {
        return NO_ERROR;
    }

    if ((status = pthread_kill(mThread, SIGUSR1)) != 0)
    {
        LOGE("end_thread() - Error cancelling thread, error = %d (%s)", status,
                strerror(errno));
    }
    else if(pthread_join(mThread, &ret)){
        LOGE("Error joining to listener thread (%s)", strerror(errno));
        return -1;
    }
    mStarted = false;
    shutdown(mFdSocket, SHUT_RDWR);
    close(mFdSocket);
    return NO_ERROR;
}

void* AudioModemStateListener::threadStart(void* obj)
{
    AudioModemStateListener* me = reinterpret_cast<AudioModemStateListener *>(obj);

    LOGD("starting");

    me->setSignalHandler();
    me->run();
    LOGD("stopping");
    pthread_exit(NULL);
    return NULL;
}

/*
 * Handler used to exit a thread
 */
void AudioModemStateListener::threadExitHandler(int unused)
{
    (void)unused;
    pthread_exit(NULL);
}

/*
 * Set the handler needed to exit a thread
 */
void AudioModemStateListener::setSignalHandler()
{
    struct sigaction actions;

    memset(&actions, 0, sizeof(actions));
    sigemptyset(&actions.sa_mask);
    actions.sa_flags = 0;
    actions.sa_handler = threadExitHandler;
    sigaction(SIGUSR1, &actions, NULL);
}

void AudioModemStateListener::run()
{
    int rts, data_size;
    uint32_t data;
    fd_set fdSetTty;

    LOGD("run");

    FD_ZERO(&fdSetTty);
    FD_SET(mFdSocket, &fdSetTty);

    while(1) {
        rts = select(mFdSocket+1, &fdSetTty, NULL, NULL, NULL);
        LOGD("run after select");
        if (rts > 0) {
            data_size = recv(mFdSocket, &data, sizeof(uint32_t), 0);
            if (data_size != sizeof(unsigned int)) {
                LOGE("Modem status handler: wrong size [%d]\n", data_size);
                continue;
            }
            setModemStatus(data);

            if (mModemStatus == MODEM_COLD_RESET) {
                /* Acknowledge the cold reset message by writing to the socket */
                sendModemColdResetAck();
            }
        }
        else if ((rts == -1) && (errno != EINTR) && (errno != EAGAIN)) {
            mModemStatus = MODEM_DOWN;
            return ;
        }
    }
}

void AudioModemStateListener::setModemStatus(int status)
{
    if (status == MODEM_UP || status == MODEM_DOWN || status == MODEM_COLD_RESET)
        mModemStatus = status;
    else
        mModemStatus = MODEM_DOWN;
    LOGD("Modem status received: %d", mModemStatus);

    /* Informs of the modem state to who implements the observer class */
    mObserver->onModemStateChange(mModemStatus);
}

bool AudioModemStateListener::sendModemColdResetAck()
{
    if(mFdSocket >= 0)
    {
        uint32_t data;
        int data_size = 0;
        data = MODEM_COLD_RESET_ACK;
        data_size = send(mFdSocket, &data, sizeof(uint32_t), 0);
        if(data_size < 0)
        {
            LOGE("Could not send MODEM_COLD_RESET ACK\n");
            return false;
        }
        return true;
    }
    LOGE("Socket not initialized");
    return false;
}

}       // namespace android
