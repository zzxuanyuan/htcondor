/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef GRIDMANAGER_H
#define GRIDMANAGER_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "user_log.c++.h"
#include "classad_hashtable.h"
#include "list.h"

#include "proxymanager.h"
#include "globusresource.h"
#include "globusjob.h"
#include "gahp-client.h"
#include "oraclejob.h"


#define UA_UPDATE_JOB_AD			0x0001
#define UA_DELETE_FROM_SCHEDD		0x0002
#define UA_LOG_SUBMIT_EVENT			0x0004
#define UA_LOG_EXECUTE_EVENT		0x0008
#define UA_LOG_SUBMIT_FAILED_EVENT	0x0010
#define UA_LOG_TERMINATE_EVENT		0x0020
#define UA_LOG_ABORT_EVENT			0x0040
#define UA_LOG_EVICT_EVENT			0x0080
#define UA_HOLD_JOB					0x0100
#define UA_FORGET_JOB				0x0200

extern char *ScheddAddr;
extern char *X509Proxy;
extern bool useDefaultProxy;
extern char *ScheddJobConstraint;
extern char *GridmanagerScratchDir;
extern char *Owner;

// initialization
void Init();
void Register();

// maintainence
void Reconfig();
	
bool addScheddUpdateAction( BaseJob *job, int actions, int request_id = 0 );
void removeScheddUpdateAction( BaseJob *job );


#endif
