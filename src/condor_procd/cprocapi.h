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

#ifndef _CPROCAPI_H
#define _CPROCAPI_H

#include <stddef.h>
#include <sys/types.h>

// this module serves as a translation layer between ProcAPI (which is
// implemented in C++) and the procd_snapshot tool (which is a root-owned
// setuid program written in C for simplicity). procInfo structures are
// returned to the caller as void* blobs of size cprocapi_size

#ifdef __cplusplus
extern "C" {
#endif

// the size of the blobs that this module returns. this can't be a
// preprocessor macro, since procapi.h would need to be included, but it's
// a C++ header file
//
extern const size_t cprocapi_size;

// this causes ProcAPI to fetch a list of information about all processes
// on the system. a handle to the first list item is returned. additional
// list elements may be accessed using cprocapi_next(). NULL is returned
// if a failure occurs
//
void* cprocapi_first(void);

// given a blob retrieve the next blob in the list, or NULL if there are
// no more. the memory for the passed-in blob is deallocated so it should
// no longer be used after this call
//
void* cprocapi_next(void*);

// retrieve the PID of the process whose information is contained in the
// given blob
//
pid_t cprocapi_pid(void*);

#ifdef __cplusplus
}
#endif

#endif
