/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef _UDS_H
#define _UDS_H

// this module encapsulates management of a UNIX domain server socket

#include <sys/types.h>

// initialize the module using the given path for the server socket's
// address. returns 0 on success, -1 on error
//
int uds_init(char* path);

// accept a client connection. return an FD to the client on success,
// -1 on failure.
//
int uds_accept(void);

// authenticate a connected client. returns 0 on success, -1 on failure.
// on success the uid parameter will be set to the client's UID. on
// failure, the passed-in FD is closed
//
int uds_authenticate(int fd, uid_t* uid);

#endif
