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

#ifndef _UDC_H
#define _UDC_H

#include <stddef.h>

// this module encapsulates client-side operations on a UNIX domain
// socket

// connect to a server listening at the given path. an FD for the
// connection is returned on success, -1 is returned on failure
//
int udc_connect(char* path);

// authenticate to a connected server with the current effective UID.
// returns 0 on success, -1 on failure. on failure, the passed-in FD
// is closed
//
int udc_authenticate(int fd);

#endif
