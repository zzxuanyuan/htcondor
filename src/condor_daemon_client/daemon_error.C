/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "daemon_error.h"

static const char* daemon_errors[] = {
	"NONE",
	"LOCATE_FAILED",
	"CONNECT_FAILED",
	"AUTHENTICATION_FAILED",
	"COMMUNICATION_FAILED", 
	"INVALID_REQUEST",
	"INVALID_REPLY",
	"COMMAND_FAILED",
};

extern "C" {

const char*
daemonError( daemon_error_t de )
{	
	if( de < _de_threshold_ ) {
		return daemon_errors[de];
	} else {
		return "Unknown";
	}
}

daemon_error_t
stringToDaemonError( const char* err )
{
	int i;
	for( i=0; i<_de_threshold_; i++ ) {
		if( !stricmp(daemon_errors[i], err) ) {
			return (daemon_error_t)i;
		}
	}
	return DE_NONE;
}

} /* extern "C" */
