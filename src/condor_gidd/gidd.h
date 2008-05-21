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

#ifndef _GIDD_H
#define _GIDD_H

// common stuff needed by both the gidd server and its clients

// the path to the UNIX socket the gidd will be listening for commands on
//
#define GIDD_SOCKET "/tmp/gidd_socket"

// commands
//
typedef enum {
	GIDD_ALLOC,
	GIDD_TEST
} gidd_cmd_t;

#endif
