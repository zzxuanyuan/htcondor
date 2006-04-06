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
#ifndef __ADMINEVENT_H__
#define __ADMINEVENT_H__

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon.h"
#include "dc_master.h"
#include "classad_hashtable.h"
#include "MyString.h"

/*

States

*/

const int EVENT_INIT				= 1;
const int EVENT_SAMPLING			= 2;
const int EVENT_EVAL_SAMPLING		= 3;
const int EVENT_MAIN_WAIT			= 4;
const int EVENT_RESAMPLE			= 5;
const int EVENT_GO					= 6;

const int BATCH_SIZE				= 4;

struct StartdStats {
	StartdStats( const char Name[], int Universe, int ImageSize, int LastCheckpoint) :
		universe(Universe), imagesize(ImageSize), lastcheckpoint(LastCheckpoint),
		jobstart(0), virtualmachineid(0), remotetime(0), ckptmegs(0), ckptlength(0), 
		ckptmegspersec(0), ckpttime(0), ckptgroup(0), ckptdone(0)
		{ 	
			strcpy( name, Name); 
			state[0] = '\0';
			activity[0] = '\0';
			clientmachine[0] = '\0';
			myaddress[0] = '\0';
			jobid[0] = '\0';
			ckptsrvr[0] = '\0';
		}
	char 	name[128];
	char	state[128];
	char	activity[128];
	char 	clientmachine[128];
	char	myaddress[128];
	char	jobid[128];
	char	ckptsrvr[128];
	int 	universe;
	int		imagesize;
	int		lastcheckpoint;
	int 	jobstart;
	int		virtualmachineid;
	int 	remotetime;
	// space for managing the benchmarking and checkpointing staging
	int 	ckptmegs;
	int 	ckptlength;
	float   ckptmegspersec;
	int 	ckpttime;
	int 	ckptgroup;
	int 	ckptdone;
};

class AdminEvent : public Service
{
  public:
	// ctor/dtor
	AdminEvent( void );
	~AdminEvent( void );
	int init( void );
	int config( bool init = false );
	int shutdownFast(void);
	int shutdownGraceful(void);

  private:
	// Command handlers
	// Timer handlers
	
	int 		th_DoShutdown( void );
	int 		m_timeridDoShutdown;
	unsigned 	m_intervalDoShutdown;

	int 		th_Check_PollingVacates( void );
	int 		m_timerid_PollingVacates;
	unsigned 	m_intervalCheck_PollingVacates;
	unsigned 	m_intervalPeriod_PollingVacates;

	int 		th_DoShutdown_States( void );
	int 		m_timerid_DoShutdown_States;
	unsigned 	m_intervalCheck_DoShutdown_States;
	unsigned 	m_intervalPeriod_DoShutdown_States;

	int			m_benchmark_size;
	int			m_benchmark_lastsize;
	int 		m_benchmark_increment;
	int 		m_benchmark_iteration;

	float 		m_NrightNow_megspersec;
	int 		m_NrightNow_size;
	int 		m_NrightNow_time;

	ClassAdList m_CkptBenchMarks;
	ClassAdList m_CkptBatches;
	ClassAdList m_PollingStartdAds;

	// Event Handling Methods

	int check_Shutdown( bool init = false );
	int tune_Shutdown_batch_size( bool init );
	int do_checkpoint_samples( bool init );
	int process_ShutdownTime( char *req_time );
	int FetchAds_ByConstraint( char *constraint );
	int benchmark_analysis( );
	int benchmark_insert( float megspersec, int megs, int time );
	bool benchmark_store_results(float megspersec, int totmegs, int tottime);
	int benchmark_show_results( );

	// Action Methods
	int pollStartdAds( ClassAdList &adsList, char *sinful, char *name );
	int sendCheckpoint( char *sinful, char *name );
	int sendVacateClaim( char *sinful, char *name );
	int run_ckpt_benchmark( );
	int compute_ckpt_batches( );
	bool have_requested_batch( int batchsz );

	// Print Methods
	int standardUDisplay();
	int standardUDisplay_StartdStats();
	int standardU_benchmark_Display();

	// Processing events
	int standardUProcess();
	int empty_Hashes();
	int SS_store(StartdStats *ss, int duration);
	int SS_test(StartdStats *ss, int duration);

	// Operation Markers
	int 		m_mystate;
	bool 		m_haveShutdown;
	bool 		m_haveFullStats;
	bool 		m_haveBenchStats;
	bool		m_stillPollingVacates;

	time_t 		m_shutdownTime;			/* established shutdown time */
	time_t 		m_newshutdownTime;		/* new shutdown time being considered */
	time_t 		m_shutdownDelta;		/* time till shutdown event occurs */

	MyString 	m_shutdownTarget;		/* what machine(s) */
	MyString 	m_newshutdownTarget;	/* what new machine(s) */
	MyString 	m_shutdownConstraint;	/* which machines? */
	MyString 	m_newshutdownAdminRate;	/* Administrator Input */
	MyString	m_spoolHistory;			/* How have we done in the past */

	unsigned 	m_shutdownSize;			/* impact is minimized by batching requests */
	unsigned 	m_newshutdownSize;			/* impact is minimized by batching requests */
	unsigned	m_lastVacateTimes;		/* last vacate calculation */
	unsigned	m_VacateTimes;			/* current vacate calculation */

	// Hash for processing scheduled checkpoint
	HashTable<HashKey, StartdStats *> m_JobNodes_su;

	// Hash for processing checkpoint benchmark
	HashTable<HashKey, StartdStats *> m_CkptTest_su;

	// storage
	ClassAdList m_collector_query_ads;
	ClassAdList m_claimed_standard;
	ClassAdList m_fromStartd;

};




#endif//__ADMINEVENT_H__
