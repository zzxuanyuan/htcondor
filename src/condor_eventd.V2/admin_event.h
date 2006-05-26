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

const int EVENT_NOW					= 10; // Do this in a asecond
const int EVENT_INIT				= 1;
const int EVENT_HUERISTIC			= 2;
const int EVENT_EVAL_HUERISTIC		= 3;
const int EVENT_SAMPLING			= 4;
const int EVENT_EVAL_SAMPLING		= 5;
const int EVENT_MAIN_WAIT			= 6;
const int EVENT_RESAMPLE			= 7;
const int EVENT_EVAL_RESAMPLE		= 8;
const int EVENT_GO					= 9;
const int EVENT_DONE				= 10;

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
	int shutdownClean(void);
	int shutdownFast(void);
	int shutdownGraceful(void);

  private:
	// Command handlers
	// Timer handlers
	
	int 		th_DoShutdown( void );
	int 		m_timeridDoShutdown;
	unsigned 	m_intervalDoShutdown;

#if 0
	int 		th_Check_PollingVacates( void );
	int 		m_timerid_PollingVacates;
	unsigned 	m_intervalCheck_PollingVacates;
	unsigned 	m_intervalPeriod_PollingVacates;

	int			m_benchmark_size;
	int			m_benchmark_lastsize;
	int 		m_benchmark_increment;
	int 		m_benchmark_iteration;

	float 		m_NrightNow_megspersec;
	int 		m_NrightNow_size;
	int 		m_NrightNow_time;
#endif
 
	int			th_maintainCheckpoints( void );
	int 		m_timerid_maintainCheckpoints;
	unsigned 	m_intervalCheck_maintainCheckpoints;
	unsigned 	m_intervalPeriod_maintainCheckpoints;

	int 		th_DoShutdown_States( void );
	int 		m_timerid_DoShutdown_States;
	unsigned 	m_intervalCheck_DoShutdown_States;
	unsigned 	m_intervalPeriod_DoShutdown_States;

	time_t		m_shutdownStart;
	time_t 		m_shutdownEnd;
	unsigned	m_shutdownMegs; /* number of megs to checkpoint before shutdown */

	ClassAdList m_CkptBenchMarks;
#if 0
	ClassAdList m_CkptBatches;
#endif
	ClassAdList m_PollingStartdAds;

	// Event Handling Methods

		/// Determine the current attributes for a shutdown
	int check_Shutdown( bool init = false );
	int do_checkpoint_shutdown( bool init = false );
#if 0
	int do_checkpoint_samples( bool init );
#endif
	int process_ShutdownTime( char *req_time );
	int FetchAds_ByConstraint( char *constraint );
	int changeState( int howsoon = EVENT_NOW, int newstate = EVENT_INIT );
	int loadCheckPointHash( int megs );
	int getShutdownDelta( void );

	// Action Methods

	int pollStartdAds( ClassAdList &adsList, char *sinful, char *name );
		/// Send a checkpoint to the job
	int sendCheckpoint( char *sinful, char *name );
		/// Send a vacate to the job
	int sendVacateClaim( char *sinful, char *name );
		/**
		Routine takes a hash of jobs which met the standard universe
		requirement and some constraint and if we have met a threshold
		limit for total image size, will create a smaller hash with jobs
		which are requested to vacate.
		*/

	// Benchmarking methods

		/** 
		Place a benchmark statistic(classad) into a list of benchmarks 
		into the m_CkptBenchMarks hash.
		*/
	int benchmark_insert( float megspersec, int megs, int time, 
		char *name, char *where);
		/*
		This routine first shows the current benchmarks being collected
		on a particular image size amount from hash m_CkptBenchMarks and 
		then shows the history of performance from the prior batches of 
		benchmarks in the hash m_CkptBatches.
		*/
	int benchmark_show_results( );
#if 0
		/// Does the benchmark_insert call
	bool benchmark_store_results(float megspersec, int totmegs, int tottime);
		/**
		Watch after the hash of standard universe jobs in the current 
		benchmarking list(m_CkptTest_su) and watch repeatedly
		until all of these jobs have completed vacating. This routine 
		is called repeatedly until all the jobs have vacated. Vacated 
		jobs are marked done and not looked at until results are 
		computed.
		*/
	int benchmark_analysis( );
		/**
		If we have the critical limit wanted yet.... then we take from the
		standard universe hash(m_JobNodes_su) and set up a batch of jobs
		to watch for vacate completions(m_CkptTest_su). They are all inserted
		into that hash and given instructions to vacate. Other methods
		track the results.
		*/
	int run_ckpt_benchmark( );
		/** 
		process the current batch of checkpoint performances and
		create a summary classad. Add this classad to a batch
		performance list(m_CkptBatches) and remove the current individual 
		records from the m_CkptBenchMarks hash.
		*/
	int compute_ckpt_batches( );
#endif
	// Print Methods
	int standardUDisplay();
	int standardUDisplay_StartdStats();
#if 0
		/** 
		we are trying to have a particular amount of work before 
		performing the next batch of vacates to determine checkpointing
		bandwidth available in the current pool. This routine decides
		if we have the current amount of image size.
		*/
	bool have_requested_batch( int batchsz );

	int standardU_benchmark_Display();
	int SS_test(StartdStats *ss, int duration);
#endif

	// Processing events
	int standardUProcess( int batchsz = 0, bool vacate = false );
	int totalRunningJobs();
	int empty_Hashes();
	int SS_store(StartdStats *ss, int duration);
	int spoolClassAd( ClassAd * ca_shutdownRate, char *direction );

	// Operation Markers
	int 		m_mystate;

	/* Administrator Input div 8 for bits to bytes*/
	float 		m_newshutdownAdminRate;

#if 0
	bool 		m_haveShutdown;
	bool 		m_haveFullStats;
	bool 		m_haveBenchStats;
	bool		m_stillPollingVacates;
#endif

	time_t 		m_shutdownTime;			/* established shutdown time */
	time_t 		m_newshutdownTime;		/* new shutdown time being considered */
	time_t 		m_shutdownDelta;		/* time till shutdown event occurs */
	time_t 		m_timeNow;				/* time till shutdown event occurs */
	time_t 		m_timeSinceNow;			/* time till shutdown event occurs */

	MyString 	m_shutdownTarget;		/* what machine(s) */
	MyString 	m_newshutdownTarget;	/* what new machine(s) */
	MyString 	m_shutdownConstraint;	/* which machines? */

	unsigned 	m_shutdownSize;			/* impact is minimized by batching requests */
	unsigned 	m_newshutdownSize;			/* impact is minimized by batching requests */
#if 0
	unsigned	m_lastVacateTimes;		/* last vacate calculation */
	unsigned	m_VacateTimes;			/* current vacate calculation */
#endif

	// Hash for processing scheduled checkpoint
	HashTable<HashKey, StartdStats *> m_JobNodes_su;

#if 0
	// Hash for processing checkpoint benchmark
	HashTable<HashKey, StartdStats *> m_CkptTest_su;
#endif

	// storage
	ClassAdList m_collector_query_ads;
	ClassAdList m_claimed_standard;
	ClassAdList m_fromStartd;
	FILE		*m_spoolStorage;
	ClassAd		*m_lastShutdown;

};




#endif//__ADMINEVENT_H__
