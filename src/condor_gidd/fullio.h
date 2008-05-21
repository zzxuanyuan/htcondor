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

#ifndef _FULLIO_H
#define _FULLIO_H

// this module contains implementations of read() and write() that loop
// until all data is read or written. these routines also handle EINTR.
// shorts reads or writes are considered a failure. if a failure occurs,
// the passed-in FD will be closed (since it would be difficult to make
// any guarantees about the state of the underlying file or stream).

#include <stddef.h>

// do a full read. returns 0 on success, -1 on failure (which includes
// short reads due to EOF). if a failure occurs, the given FD will be
// closed.
//
int fullio_read(int fd, void* buf, size_t len);

// do a full write. returns 0 on success, -1 on failure. if a failure
// occurs, the given FD will be closed.
//
int fullio_write(int client, void* buf, size_t len);

#endif
