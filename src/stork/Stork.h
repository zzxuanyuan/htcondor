/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
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
#ifndef __STORK_H__
#define __STORK_H__

#if 0
using namespace std;
#endif

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_commands.h"
#include "StorkJobId.h"
#include <map>
#include <list>

// New ClassAds.  Define WANT_NAMESPACES whenever both new and old ClassAds
// coexist in the same file.
#define WANT_NAMESPACES
#include "classad_distribution.h"

// TODO:

// Macros

// Typedefs
typedef std::map<const int,std::string> int2stringMap;
typedef std::map<const std::string,time_t> string2timeMap;
typedef std::list<classad::ClassAd *> jobList;

// Module standard I/O file descriptor type and indices.
typedef int module_stdio_t[3];
#define MODULE_STDIN_INDEX		0
#define MODULE_STDOUT_INDEX		1
#define MODULE_STDERR_INDEX		2

/// Stork server class
class Stork : public Service
{
	public:

	/// Constructor
	Stork( char *subsystem);

	/// Destructor
	~Stork( void );

	/// Dynamic configuration, repeated upon reconfig command.
	void configure( const bool is_full_reconfig=false);

	/**	Shutdown Stork
		@param is_graceful	true if graceful shutdown
		*/
	void shutdown( const bool is_graceful );

	/// Initialize Stork
	void initialize( void );

	private:

	/** Open completed job history file.  Perform history file rotation if file
		has grown too large.
		@return history file descriptor.
		**/
	int openJobHistory(void);

	/// Initialize job queue ClassAd Collection
	void initializeJobQueue( const char* spoolDir );

	/// Delete job queue views.
	void deleteJobQueueViews(void);

	/// Register Commands
	void registerCmds();

	/// Cancel Commands
	void cancelCmds();

	/// Register Reapers
	void registerReapers();

	/// Cancel Reapers
	void cancelReapers();

	/// Register timers
	void registerTimers();

	/// Cancel timers
	void cancelTimers();

	/// Configure idle jobs view.
	void configureIdleJobsView( void );

	/// require authentication on incoming commands
	int requireAuthentication(	const Stream *rsock,
								const char* subsys,
								const int code,
								const char* msg) const;

	/// job submit command handler
	int commandHandler_SUBMIT( const int command, Stream *stream);

	/// Reschedule idle jobs timer handler.
	int timerHandler_reschedule( void );

	/// Clean jobQ timer handler.
	int timerHandler_cleanJobQ(void);

	/// Run a job.
	bool runJob(classad::ClassAd* ad);

	/// Remove a job from job queue.
	bool deleteJob(classad::ClassAd* ad);

	/// Common job process reaper.
	int commonJobReaper(	const int pid,
							const int exit_status);

	/// Transfer job process reaper.
	int transferJobReaper(	classad::ClassAd* ad);

	/// Check job ad for validity.
	bool checkJobAd(classad::ClassAd* ad, std::string& error);

	/// Check job ad proxy
	bool checkJobAdProxy(classad::ClassAd* ad, std::string& error);
	
	/// Check transfer job ad for validity.
	bool checkTransferJobAd(	classad::ClassAd* ad,
								std::string& error);

	/**	Add a job ad to the queue.
		@param ad is added to the job queue.  The queue takes ownership of the
			ad.  DO NOT FREE THE AD.
		*/
	//jobId_t addJob(classad::ClassAd* ad);

	/// Get next jobId
	StorkJobId getNextJobId(void);

	/// Update a jobAd in the jobQ
	bool updateJobAd(const std::string& key, classad::ClassAd* deltaAd);

	/// Cold start recover job queue from log file.
	bool recoverJobQueue( void );

	/// Initialise top level jobs run directory
	void initExecDir(void) const;

	/// Return job run directory path
	bool jobRunDir(	const classad::ClassAd* ad,
					std::string& dir ) const;

	/// Create job run directory
	bool createJobRunDir(	classad::ClassAd* ad,
							std::string& error,
							std::string& jobDir) const;

	/// Destroy job run directory
	bool destroyJobRunDir(const classad::ClassAd* ad) const;

	/// Spool credential to job run dir.
	bool spoolCredential(	classad::ClassAd* ad,
							std::string& error,
							const char *credential,
							const std::string& jobDir);

	/// Get job user, owner.
	bool getJobPrivilegeInfo(classad::ClassAd* ad, Stream *stream, std::string&
			error) const;

#if 0
	/**	Check user privilege file access
		@param ad is queried for job owner privilege info.
		@param path is the subject of the access check.
		@param mode is a mask consisting of one or more of R_OK, W_OK, X_OK,
			taken from <access.h>
		@param specifies privilege level to assume for access check
		@return true if requested user access, else false.
		System access errors are returned via errno.
		*/
	bool userFileAccess(	const classad::ClassAd* ad,
							const std::string path,
							const int mode,
							const priv_state) const;
#endif

	/// Reschedule idle jobs.
	void rescheduleJobs(void);

	/** Query the job queue.
		@param jobAds is a list to which queried ads are appended.
		@param viewName is view to query
		@param constraint is query constraint
		@param limit places a limit on number of ads returned.
		@return true if requested user access, else false.
	  **/
	bool query(
			jobList& jobAds,
			const std::string& viewName,
			const std::string constraint="",
			int limit=INT_MAX
		);

	/// Open standard I/O streams for a module
	bool open_module_stdio(	const classad::ClassAd* ad,
							module_stdio_t module_stdio);

	/// Close standard I/O streams for a module
	void close_module_stdio( module_stdio_t module_stdio);

	// Member data: // /////////////////////////////////////////////////////////

	/// Subsystem name
	const char*						m_Subsytem;

	/// EXECUTE directory.
	const char*						m_EXECUTE;

	/// module directory.
	const char*						m_ModuleDir;

	/// In-memory job queue.
	classad::ClassAdCollection		m_JobQ;

	/// Root jobs view name.
	classad::ViewName    			m_RootJobsViewName;

	/// Root jobs view.
	const classad::View*			m_RootJobsView;

	/// Idle jobs view name.
	classad::ViewName				m_IdleJobsViewName;

	/// Idle jobs view.
	const classad::View*			m_IdleJobsView;

	/// Running jobs view name.
	classad::ViewName				m_RunJobsViewName;

	/// Running jobs view.
	const classad::View*			m_RunJobsView;

	/// job submit command id
	int 							m_SubmitCmdId;

	/// Optional persistent (disk) copy of jobQ.
	std::string						m_PersistentJobQ;

	/// Optional completed job histor file.
	std::string						m_JobHistory;

	/// Period for truncating persistent jobQ, seconds.
	int								m_CleanJobQInterval;

	/// Timer for truncating persistent jobQ, seconds.
	int 							m_CleanJobQTimerId;

	/// Period for rescheduling jobs, seconds.
	int								m_RescheduleInterval;

	/// Minimum reschedule interval, seconds.
	time_t							m_MinRescheduleInterval;

	/// Last reschedule time.
	time_t							m_lastRescheduleTime;

	/// Timer for rescheduling jobs, seconds.
	int 							m_RescheduleTimerId;

	/// Requirements to reschedule idle jobs.
	std::string						m_RescheduleRequirements;

	/// Rank to reschedule idle jobs.
	std::string						m_RescheduleRank;

	/// Job reaper id.
	int								m_JobReaperId;

	/// Map of running job PIDs to jobIds.
	int2stringMap					m_Pid2JobIdMap;

	/// Cache of existing modules, to avoid redundant module stat()'s
	string2timeMap					m_ModuleCache;

	/// Next jobId integral value, used by getNextJobId()
	StorkJobId						m_NextJobId;

	/// Module cache entry time to live
	time_t							m_ModuleCacheTTL;

	/// Limit for number of running jobs
	int								m_MaxRunningJobs;

	/// Limit the number of jobs Stork starts each rescheduling interval.
	int								m_JobStartCount;

	/// Utility ClassAd parser
	classad::ClassAdParser			m_Parser;

	/// Utility ClassAd unparser
	classad::PrettyPrint			m_Unparser;

	/// Query iterator
	classad::LocalCollectionQuery	m_Query;

}; // class Stork

#endif//__STORK_H__
