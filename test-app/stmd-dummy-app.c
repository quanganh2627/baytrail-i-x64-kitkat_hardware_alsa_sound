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
#define LOG_TAG "STMD_APP"
#include <utils/Log.h>

#define ALLOWED_CLIENT  10

unsigned int         modem_status = MODEM_DOWN;
pthread_mutex_t      modem_trig_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t    modem_status_thread;
int           fd_socket;

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

    pthread_mutex_lock(&modem_trig_mutex);
    modem_status = status;
    LOGV("set_modem_status() - request modem status set to %d", modem_status);

    if (fd_socket != -1)
    {
        data_size = send(fd_socket, &modem_status, sizeof(unsigned int), 0);
        if (data_size != sizeof(unsigned int))
        {
            LOGE("set_modem_status() - send failed for client %d [%s]", i, strerror(errno));
        }
    }

    pthread_mutex_unlock(&modem_trig_mutex);

}



/*
 * Test case 001: test modem reset without core dump (AT+CFUN=15)
 */
bool test_simulate_modem_up()
{

    set_modem_status(MODEM_TRIG_UP);

    /* test is success if up to down and down to up transition are detected */
    return true;
}

/*
 * Test case 002: test modem reset with core dump (AT+XLOG=4)
 */
bool test_simulate_modem_down()
{

    set_modem_status(MODEM_TRIG_DOWN);

    /* test is success if up to down and down to up transition are detected */
    return true;
}

/*
 * Send a reset to STMD
 */
bool test_simulate_modem_cold_reset()
{
    set_modem_status(MODEM_TRIG_COLD_RESET);

    sleep(2);

    set_modem_status(MODEM_TRIG_DOWN);

    sleep(5);

    set_modem_status(MODEM_TRIG_UP);

    /* test is success if up to down and down to up transition are detected */
    return true;
}

bool stress_test()
{

    return true;
}

void start_test(int test_id)
{
    switch (test_id)
    {
        case 1:
            RUN_TEST(test_simulate_modem_up())
            break;

        case 2:
            RUN_TEST(test_simulate_modem_down())
            break;

        case 3:
            RUN_TEST(test_simulate_modem_cold_reset())
            break;

        case 4:
            RUN_TEST(stress_test())
            break;

        case 0:
            exit(0);
            break;

        default:
            printf("This choice is not valid.\n");
    }
}

int main(int argc, char* argv[])
{
    pthread_attr_t attr;
    int            ret;
    int            test_id = 0;
    char           data[64];
    struct pollfd fds[1];
    int           data_size;


    /* Connects thread to modem-status socket */
    fd_socket = socket_local_client(SOCKET_NAME_MODEM_TRIG,
            ANDROID_SOCKET_NAMESPACE_RESERVED,
            SOCK_STREAM);

    if (fd_socket < 0)
    {
        fprintf(stderr, "\n\tFailed to connect to modem-status socket\n");
        exit(-1);
    }


    if (argc == 2)
    {
        test_id = atoi(argv[1]);
        start_test(test_id);
    }
    else
    {
        for (;;)
        {
            printf("\n\n\n");
            printf("** STMD - TEST APPLICATION **\n");
            printf("=============================\n");
            printf("\n");
            printf("1 - Simulate Modem Up\n");
            printf("2 - Simulate Modem Down\n");
            printf("3 - Simulate Modem Cold Reset\n");
            printf("4 - stress test\n");
            printf("\n");
            printf("0 - Quit the test application.");
            printf("\n");
            printf("Please select the test to run : \n");

            fgets(data, 64, stdin);
            test_id = atoi(data);

            start_test(test_id);
        }
    }

    return 0;
}
