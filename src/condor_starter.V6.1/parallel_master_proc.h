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

#ifndef _CONDOR_PARALLEL_MASTER_PROC_H
#define _CONDOR_PARALLEL_MASTER_PROC_H

#include "condor_common.h"
#include "condor_classad.h"
#include "vanilla_proc.h"


/**
  This class invokes fake sshd instead of 
  the specified executable.
  It also setup several environment variables
  to inform the fake sshd.
 */


class ParallelMasterProc : public VanillaProc
{
 public:

    ParallelMasterProc( ClassAd * jobAd );
    virtual ~ParallelMasterProc();

		/** Pull the MPI Node out of the job ClassAd and save it.
		    Replace executable, setup enviroment variables,
			Then, just call VanillaProc::StartJob() to do the real
			work. */
    virtual int StartJob();

		/** LAM tasks shouldn't be suspended.  So, if we get a
			suspend, instead of sending the LAM task a SIGSTOP, we
			tell it to shutdown, instead. */
    virtual void Suspend();

		/** This is here just so we can print out a log message, since
			we don't expect this will ever get called. */
    virtual void Continue();

		/** Do a family->hardkill(); */
	virtual bool ShutdownFast();
  
    int  SpawnParallelMaster();

protected:
    int alterEnv();

private:
    void SshdCheckInit();
    int  CheckSshdCount(ClassAd *jobAd, int & haveEnough);

    //  for parallel universe
    int sshdCheckInterval;
    int sshdCheckCounter;
    int sshdCheckMax;  
    int sshd_check_tid;



};

#endif
