/* stmd_protocol.h
**
** Copyright (C) Intel 2011
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
** Author: Guillaume Lucas <guillaumex.lucas@intel.com>
*/

#ifndef _STMD_H_
#define _STMD_H_

/* Sockets name */
#define SOCKET_NAME_MODEM_STATUS    "dummy-status"
#define SOCKET_NAME_MODEM_TRIG    "dummy-trig"

/* STMD -> Clients protocol */
#define MODEM_DOWN          0x00
#define MODEM_UP            0x01
#define PLATFORM_SHUTDOWN   0x02
#define MODEM_COLD_RESET    0x03

/* Clients -> STMD protocol */
#define MODEM_COLD_RESET_ACK 0x30

/* STMD -> Clients protocol */
#define MODEM_TRIG_DOWN          0x00
#define MODEM_TRIG_UP            0x01
#define PLATFORM_TRIG_SHUTDOWN   0x02
#define MODEM_TRIG_COLD_RESET    0x03

#endif /* _STMD_H_ */

