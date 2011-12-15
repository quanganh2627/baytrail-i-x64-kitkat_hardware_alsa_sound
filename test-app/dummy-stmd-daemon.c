/* STMD Test Application
**
** Copyright (C) Intel 2010
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** Author: Gustave De Kervern <gustave.dekervern@intel.gro>
*/

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <pthread.h>

#include <cutils/sockets.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>
#include <dirent.h>
#include "stmd.h"

#define LOG_NDEBUG 0
#define LOG_TAG "STMD"
#include <utils/Log.h>

#define ALLOWED_CLIENT  10

unsigned int         modem_status = MODEM_DOWN;
pthread_mutex_t      modem_status_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t    modem_status_thread;
pthread_t    modem_trig_thread;

void* modem_status_client_handler(void*);
void* modem_trig_handler(void*);

/* Client data */
struct client_data
{
    pthread_t thread_id;    /* handler of the client thread */
    int       socket_fd;    /* file descriptor of the client socket */
};

struct socket_data
{
    char*               socket_name;    /* name of the socket */
    int                 fd;             /* socket file descriptor */
    pthread_t           handler;        /* handler of the thread who manage the socket */
    struct client_data* clients;        /* list of clients of the socket */
    pthread_mutex_t     clients_mutex;  /* mutex for the clients list */
    pthread_cond_t      clients_cond;   /* condition for the clients list */
    void * (*client_routine)(void *);   /* function used to manage a client */
};

struct socket_data modem_status_socket = {
    .socket_name = SOCKET_NAME_MODEM_STATUS,
    .fd = -1,
    .handler = -1,
    .clients = NULL,
    .clients_mutex = PTHREAD_MUTEX_INITIALIZER,
    .clients_cond = PTHREAD_COND_INITIALIZER,
    .client_routine = modem_status_client_handler
};

struct socket_data modem_trig_socket = {
    .socket_name = SOCKET_NAME_MODEM_TRIG,
    .fd = -1,
    .handler = -1,
    .clients = NULL,
    .clients_mutex = PTHREAD_MUTEX_INITIALIZER,
    .clients_cond = PTHREAD_COND_INITIALIZER,
    .client_routine = modem_trig_handler
};

#define RUN_TEST(cmd)           \
if (cmd)                        \
    printf("Test is PASS\n\n"); \
else                            \
    printf("Test is FAIL\n\n"); \



/*
 * Set and sends modem status if clients are connected
 *
 * input:
 *      unsigned int status: MODEM_DOWN or MODEM_UP
 * output:
 *      none
 */
void set_modem_status(unsigned int status)
{
    int data_size = 0;
    int i;

    pthread_mutex_lock(&modem_status_mutex);
    modem_status = status;
    LOGV("set_modem_status() - modem status set to %d", modem_status);

    pthread_mutex_lock(&modem_status_socket.clients_mutex);
    for (i=0; i<ALLOWED_CLIENT; i++)
    {
        if (modem_status_socket.clients[i].socket_fd != -1)
        {
            data_size = send(modem_status_socket.clients[i].socket_fd, &modem_status, sizeof(unsigned int), 0);
            if (data_size != sizeof(unsigned int))
            {
                LOGE("set_modem_status() - send failed for client %d [%s]", i, strerror(errno));
            }
        }
    }
    pthread_mutex_unlock(&modem_status_socket.clients_mutex);
    pthread_mutex_unlock(&modem_status_mutex);

}

/*
 * Handles the modem status
 */
void* modem_trig_handler(void* thread_data)
{
    int           ret;
    struct pollfd fds[1];
    unsigned int  data;
    int           data_size = 0;
    int           id = (int)thread_data;
    int           fd_client = modem_trig_socket.clients[id].socket_fd;

    LOGD("Modem status handler started");

    for(;;)
    {
        fds[0].fd = fd_client;
        fds[0].events = POLLIN | POLLHUP;
        fds[0].revents = 0;

        ret = poll(fds, 1, -1);
        if (ret > 0)
        {
            if (fds[0].revents & POLLIN)
            {
                data_size = recv(fd_client, &data, sizeof(unsigned int), 0);

                /* Secure that data size is correct */
                if (data_size != sizeof(unsigned int))
                {
                    LOGE("Modem status handler: wrong size [%d][%s]", data_size, strerror(errno));
                    continue;
                }

                /* Logs modem status and set transition test variable */
                if (data == MODEM_TRIG_UP)
                {
                    printf("\tTest Trig request received: MODEM_UP\n");
                    if (modem_status == MODEM_DOWN || modem_status == MODEM_TRIG_COLD_RESET)
                    {
                        set_modem_status(MODEM_UP);
                        modem_status = MODEM_UP;
                    }
                }
                else if (data == MODEM_TRIG_DOWN)
                {
                    LOGD("Test Trig request received: MODEM_DOWN");
                    if (modem_status == MODEM_UP || modem_status == MODEM_TRIG_COLD_RESET)
                    {
                        set_modem_status(MODEM_DOWN);
                        modem_status = MODEM_DOWN;
                    }
                }
                else if (data == MODEM_TRIG_COLD_RESET)
                {
                    LOGD("Test Trig request received: MODEM_TRIG_COLD_RESET");
                    if (modem_status == MODEM_UP)
                    {
                        set_modem_status(MODEM_COLD_RESET);
                        modem_status = MODEM_COLD_RESET;
                    }
                }
                else
                {
                    LOGD("Modem status received: UNKNOWN [%d]", data);
                }
            }
            else if (fds[0].revents & POLLHUP)
            {
            //    LOGE("Modem status handler: POLLHUP received[%s]", strerror(errno));
            }
            else
            {
                LOGE("Modem status handler: unknow event [%d][%s]", fds[0].revents,strerror(errno));
                exit(-1);
            }
        }
        else
        {
            LOGE("Modem status handler: poll failed [%d][%s]", ret, strerror(errno));
            exit(-1);
        }
    } /* poll loop */

    return NULL;
}

/*
 * Handler used to exit a thread
 */
void thread_exit_handler(int unused)
{
    (void)unused;
    pthread_exit(NULL);
}

/*
 * Set the handler needed to exit a thread
 */
void set_signal_handler()
{
    struct sigaction actions;

    memset(&actions, 0, sizeof(actions));
    sigemptyset(&actions.sa_mask);
    actions.sa_flags = 0;
    actions.sa_handler = thread_exit_handler;
    sigaction(SIGUSR1, &actions, NULL);
}
/*
 * modem-status socket client manager thread
 */
void* modem_status_client_handler(void* thread_data)
{
    int           ret;
    struct pollfd fds[1];
    unsigned int  data;
    int           data_size = 0;
    int           id = (int)thread_data;
    int           fd_client = modem_status_socket.clients[id].socket_fd;

    set_signal_handler();

    LOGD("modem_status_client_handler(%d) - Started", id);

    /* A client is connected: send the modem status to it */
    data_size = send(fd_client, &modem_status, sizeof(unsigned int), 0);
    if (data_size != sizeof(unsigned int))
    {
        LOGE("modem_status_client_handler(%d) - send failed [%d]", id, data_size);
    }

    for(;;)
    {
        fds[0].fd = fd_client;
        fds[0].events = POLLIN | POLLHUP;
        fds[0].revents = 0;

        ret = poll(fds, 1, -1);
        if (ret > 0)
        {
            if (fds[0].revents & POLLIN)
            {
                data_size = recv(fd_client, &data, sizeof(unsigned int), 0);
                if (data_size == 0)
                {
                    LOGV("modem_status_client_handler(%d) - Client is disconnected", id);
                    close(fd_client);
                    fd_client = -1;
                    break;
                }
                else if (data_size < 0)
                {
                    LOGE("modem_status_client_handler(%d) - recv failed [%d]", id, data_size);
                    continue;
                }
                else
                {
                    if (data == MODEM_COLD_RESET_ACK)
                    {
                        LOGD("modem_status_client_handler(%d) - recv ACK", id);
                    }
                }
            }
            else if (fds[0].revents & POLLHUP)
            {
                LOGV("modem_status_client_handler(%d) - POLLHUP event", id);
                close(fd_client);
                fd_client = -1;
                break;
            }
            else
            {
                LOGE("modem_status_client_handler() - Unknow event [%d]", fds[0].revents);
                continue;
            }
        }
        else
        {
            LOGE("modem_status_client_handler() - poll failed [%d]", ret);
            /* close the socket and restart */
            shutdown(fd_client, SHUT_RDWR);
            close(fd_client);
            fd_client = -1;
            break;
        }
    } /* Loop poll() */

    /* Client is lost */
    pthread_mutex_lock(&modem_status_socket.clients_mutex);
    modem_status_socket.clients[id].socket_fd = -1;
    pthread_mutex_unlock(&modem_status_socket.clients_mutex);

    /* Signal that a client is free */
    pthread_cond_signal(&modem_status_socket.clients_cond);

    LOGD("modem_status_client_handler(%d) - End", id);

    return NULL;
}

/*
 * Socket manager
 *
 * Accept client connection to a socket and create the necessary client thread.
 *
 * input:
 *      void* p_data: pointer to the socket structure
 * output:
 *      none
 */
void* socket_manager(void* p_data)
{
    struct socket_data* p_socket = (struct socket_data*)p_data;
    int                 ret;
    pthread_attr_t      attr;
    int                 fd_client;
    int                 client_idx = 0;

    LOGD("socket_manager(%s) - started", p_socket->socket_name);

    set_signal_handler();

    /* Allocate and intialize clients array */
    p_socket->clients = calloc(ALLOWED_CLIENT,
            sizeof(struct client_data));
    if (p_socket->clients == NULL)
    {
        LOGE("socket_manager(%s) - calloc failed", p_socket->socket_name);
        exit(-1);
    }

    for (client_idx=0; client_idx<ALLOWED_CLIENT; client_idx++)
    {
        p_socket->clients[client_idx].socket_fd = -1;
    }

    /* Retrieve sockets */
    p_socket->fd = android_get_control_socket(p_socket->socket_name);
    if (p_socket->fd < 0)
    {
        LOGE("socket_manager(%s) - get socket failed", p_socket->socket_name);
        free(p_socket->clients);
        exit(-1);
    }

    ret = listen(p_socket->fd, ALLOWED_CLIENT);
    if (ret < 0)
    {
        LOGE("socket_manager(%s) - listen failed [%s]",
                p_socket->socket_name, strerror(errno));
        exit(-1);
    }

    client_idx = 0;

    for(;;)
    {
        fd_client = accept(p_socket->fd, NULL, NULL);
        if (fd_client < 0)
        {
            LOGE("socket_manager(%s) - Error during accept [%s]",
                    p_socket->socket_name, strerror(errno));
            continue;
        }

        LOGD("socket_manager(%s) - client %d connected",
                p_socket->socket_name, client_idx);

        pthread_mutex_lock(&p_socket->clients_mutex);
        p_socket->clients[client_idx].socket_fd = fd_client;

        /* Create client thread */
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        pthread_create(&p_socket->clients[client_idx].thread_id, &attr,
                p_socket->client_routine, (void*)client_idx);

        /* Search next client */
        for(client_idx=0; client_idx<ALLOWED_CLIENT; client_idx++)
        {
            if (p_socket->clients[client_idx].socket_fd == -1)
                break;
        }

        /* If no more client allowed, wait end of one */
        if (client_idx >= ALLOWED_CLIENT)
        {
            LOGD("socket_manager(%s) - client list is full", p_socket->socket_name);
            pthread_cond_wait(&p_socket->clients_cond, &p_socket->clients_mutex);
            /* Find client id */
            for(client_idx=0; client_idx<ALLOWED_CLIENT; client_idx++)
            {
                if (p_socket->clients[client_idx].socket_fd == -1)
                    break;
            }
        }

        pthread_mutex_unlock(&p_socket->clients_mutex);

        LOGD("socket_manager(%s) - next client id: %d",
                p_socket->socket_name, client_idx);

    }

    return NULL;
}
int main(int argc, char* argv[])
{
    pthread_attr_t attr;
    int            ret;

    LOGD("DUMMY-STMD STATE: BOOT Good");

    /* Start the modem status handler thread */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    /* start the two socket manager threads */
    ret = pthread_create(&modem_status_socket.handler, &attr, socket_manager, (void*)&modem_status_socket);
    if (ret < 0)
    {
        LOGE("pthread create failed for modem status handler : [%s]", strerror(errno));
        exit(-1);
    }
    LOGD("DUMMY-STMD STATE: First thread created ");
    /* Start the thread that listens the socket of the dummy stmd test app */
    ret = pthread_create(&modem_trig_socket.handler, &attr, socket_manager, (void*)&modem_trig_socket);
//    ret = pthread_create(&modem_trig_thread, &attr, modem_trig_handler, NULL);
    if (ret < 0)
    {
        LOGE("pthread create failed for modem status handler : [%s]", strerror(errno));
        exit(-1);
    }
    LOGD("DUMMY-STMD STATE: second thread created ");
    pthread_attr_destroy(&attr);

    do
    {
        LOGD("DUMMY-STMD STATE .");
        sleep(60);      /* sleep 1 minute */
    } while(1);

    LOGV("Exiting DUMMY-STMD");
    return 0;
}
