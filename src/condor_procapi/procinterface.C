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
#include "condor_common.h"
#include "procinterface.h"


ProcAd::ProcAd() {

}

ProcAd::~ProcAd() {

}

  /* getProcAd returns a ClassAd containing information about one process.
     Information can be retrieved on any process owned by the same user, 
     and on some systems (Linux, HPUX, Sol2.6) information can be gathered 
     from all processes.  See the ProcInfo struct definitions in procapi.h
     to see the exact definitions of what gets returned.  */
ClassAd * ProcAd::getProcAd ( pid_t pid ) {

  piPTR pi = NULL;
  ClassAd *ad;
  spy.getProcInfo ( pid, pi );

  ad = dumpToAd ( pi );
  delete pi;

  return ad;
}
  
  /* getProcSetAd returns a ClassAd containing the sums of the monitored 
     values for a set of pids.  These pids are specified by an array of 
     pids ('pids') that has 'numpids' elements. */
ClassAd * ProcAd::getProcSetAd ( pid_t *pids, int numpids ) {

  piPTR pi = NULL;
  ClassAd *ad;
  spy.getProcSetInfo( pids, numpids, pi );

  ad = dumpToAd ( pi );
  delete pi;

  return ad;
}

  /* getFamilyAd returns a ClassAd containing the sums of the monitored
     values of a 'family' of processes descended from one pid.  By 
     'family', I mean that pid and all children, children's children, etc. */
ClassAd * ProcAd::getFamilyAd ( pid_t fatherpid ) {

  piPTR pi = NULL;
  ClassAd *ad;
  spy.getFamilyInfo( fatherpid, pi );

  ad = dumpToAd ( pi );
  delete pi;

  return ad;
}

ClassAd * ProcAd::dumpToAd( piPTR pi ) {

  ClassAd *ad = new ClassAd;

  ad->InsertAttr( ATTR_TYPE, "ProcessInformation" );
  ad->InsertAttr( "THIS_PID", (int) pi->pid );
  ad->InsertAttr( "PARENT_PID", (int) pi->ppid );
  ad->InsertAttr( "IMAGE_SIZE", (int) pi->imgsize, K_FACTOR );
  ad->InsertAttr( "RESIDENT_SET_SIZE", (int) pi->rssize, K_FACTOR );
  ad->InsertAttr( "MAJOR_PAGE_FAULTS", (int) pi->majfault );
  ad->InsertAttr( "MINOR_PAGE_FAULTS", (int) pi->minfault );
  ad->Insert( "USER_TIME", Literal::MakeRelTime( pi->user_time ) );
  ad->Insert( "SYSTEM_TIME", Literal::MakeRelTime( pi->sys_time ) );
  ad->Insert( "PROCESS_AGE", Literal::MakeRelTime( pi->age ) );
  ad->InsertAttr( "PERCENT_CPU_USAGE", pi->cpuusage );

  return ad;
}
