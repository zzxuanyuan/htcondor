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

#include "sshd_info.h"
#include "condor_debug.h"

SshdInfo::SshdInfo(){
  hostname = NULL;
  rshDir   = NULL;
  userShell= NULL;
  workDir  = NULL;
  opensshDir= NULL;
  userName  = NULL;
}

SshdInfo::~SshdInfo(){
  if ( hostname )
	delete hostname;
  if ( rshDir )
	delete rshDir;
  if ( userShell )
	delete userShell;
  if ( workDir )
	delete workDir;
  if ( opensshDir )
	delete opensshDir;
  if ( userName )
	delete userName;
}

  
int
SshdInfo::code(Stream & s){
  if ( !s.code( hostname ) ) {
	EXCEPT( "Failed to get/send hostname" );
  }
  if ( !s.code( rshDir ) ) {
	EXCEPT( "Failed to get/send rshDir" );
  }
  if ( !s.code( userShell ) ) {
	EXCEPT( "Failed to get/send userShell" );
  }
  if ( !s.code( workDir ) ) {
	EXCEPT( "Failed to get/send workDir" );
  }
  if ( !s.code( opensshDir ) ) {
	EXCEPT( "Failed to get/send opensshDir" );
  }
  if ( !s.code( userName ) ) {
	EXCEPT( "Failed to get/send userName" );
  }
  if ( !s.code( port ) ) {
	EXCEPT( "Failed to get/send port" );
  }
  return true;
}


