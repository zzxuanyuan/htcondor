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
#ifndef _CONDOR_DAEMON_ERRORS_H
#define _CONDOR_DAEMON_ERRORS_H


// if you add another type to this list, make sure to edit
// daemon_types.C and add the string equivilant.

enum daemon_error_t {
	DE_NONE,
	DE_LOCATE_FAILED,
	DE_CONNECT_FAILED,
	DE_AUTHENTICATION_FAILED,
	DE_COMMUNICATION_FAILED, 
	DE_INVALID_REQUEST,
	DE_INVALID_REPLY,
	DE_COMMAND_FAILED,
	_de_threshold_ 
};

#ifdef __cplusplus
extern "C" {
#endif

const char* daemonError( daemon_error_t de );
daemon_error_t stringToDaemonError( const char* name );

#ifdef __cplusplus
}
#endif


#endif /* _CONDOR_DAEMON_ERRORS_H */
