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
//dprintf(D_ALWAYS, "DEBUG trace: %s:%d\n", __FILE__, __LINE__);

#include "Stork.h"
#include "stork_config.h"
#include "stork_job_ad.h"
#include "newclassad_stream.h"
#include "std_string_utils.h"
#include "directory.h"
#include "basename.h"
#include "UrlParser.h"
#include "stork_util.h"
#include "StorkUserLog.h"
#include "internet.h"
#include "globus_utils.h"
#include "env.h"
#include "condor_arglist.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
using std::string;	// used _everywhere_ in new ClassAds API

// Constants
#define NULL_CMD_ID						(-1)
#define NULL_TIMER_ID					(-1)
#define NULL_REAPER_ID					(-1)
// Avoid blocking the server for client communication problems:
#define SERVER_SOCK_TIMEOUT				10
#define STORK_JOB_QUEUE_LOG				"stork_job_queue.log"
#define STORK_JOB_HISTORY_LOG			"stork_history"
#define KEY_MAXJOBID					"maxJobId"

Stork::Stork( char *subsystem)
{
	m_Subsytem = (const char*)subsystem;
	//initialize();		// causes memory errors!
	return;
}

Stork::~Stork( void )
{
	if (m_EXECUTE) free((char*)m_EXECUTE);
	if (m_ModuleDir) free((char*)m_ModuleDir);

	return;
}

void
Stork::initialize( void )
{
	// Stork requires an execute (job sandbox) directory.
	m_EXECUTE = param(EXECUTE);
	if (! m_EXECUTE) {
		EXCEPT( EXECUTE " directory not defined in config file!" );
	}
	initExecDir();	// does a stat() of m_EXECUTE

	// Stork requires the SPOOL directory.
	const char *spool = param(SPOOL);
	if (! spool) {
		EXCEPT( SPOOL " directory not defined in config file!" );
	}
	StatInfo spoolStat(spool);
	if ( spoolStat.Error() ) {
		EXCEPT("stat %s directory: %s\n", spool, strerror(spoolStat.Errno() ));
	}
	if (! spoolStat.IsDirectory() ) {
		EXCEPT("%s is not a directory\n", spool);
	}
	m_PersistentJobQ.clear();

	// Stork requires a STORK_MODULE_DIR (or [deprecated] LIBEXEC) directory
	m_ModuleDir = param(STORK_MODULE_DIR);
	if (! m_ModuleDir) {
		m_ModuleDir = param(LIBEXEC);
		if (! m_ModuleDir) {
			EXCEPT(STORK_MODULE_DIR " directory not defined in config file!" );
		}
	}
	StatInfo moduleDirStat(m_ModuleDir);
	if ( moduleDirStat.Error() ) {
		EXCEPT("stat %s directory: %s\n", m_ModuleDir,
				strerror(moduleDirStat.Errno() ));
	}
	if (! moduleDirStat.IsDirectory() ) {
		EXCEPT("%s is not a directory\n", m_ModuleDir);
	}
	dprintf(D_ALWAYS, "default module location: %s\n", m_ModuleDir);

	// Initialize job queue ClassAd Collection
	initializeJobQueue( spool );

	// TODO: add job history file
	if (spool) free( (void *)spool);

	// Initialize next job id.
	getNextJobId();

	m_SubmitCmdId = NULL_CMD_ID;

	m_CleanJobQInterval = -1;
	m_CleanJobQTimerId = NULL_TIMER_ID;

	m_RescheduleInterval = -1;
	m_lastRescheduleTime = 0;
	m_RescheduleTimerId = NULL_TIMER_ID;

	m_JobReaperId = NULL_REAPER_ID;

	// Register daemoncore commands, reapers and timers last, after all other
	// initializations are complete.
	//registerTimers();
	registerCmds();
	registerReapers();

	configure();	// calls registerTimers()

	return;
}

// Open completed job history file.  Perform history file rotation if file has
// grown too large.
int
Stork::openJobHistory(void)
{
	int maxSize =
		param_integer(
				MAX_STORK_HISTORY, 			// name
				MAX_STORK_HISTORY_DEFAULT,	// default value
				MAX_STORK_HISTORY_MIN		// min value
		);
	if ( maxSize) {
		StatInfo logStat( m_JobHistory.c_str() );
		if (logStat.Error() && logStat.Errno() != ENOENT ) {
			dprintf(D_ALWAYS, "stat %s: %s\n",
					m_JobHistory.c_str(), strerror(logStat.Errno() ) );
			return -1;
		}

		if ( (logStat.Error() == 0) && (logStat.GetFileSize() > maxSize) ) {
			string old = m_JobHistory;
			old += ".old";
			dprintf(D_ALWAYS, "Saving job history file to %s\n", old.c_str() );
			unlink(old.c_str() );

#if defined(WIN32)
#error link syscall not available
			// copy WIN32 preserve_log_file() 
#else
			if ( link( m_JobHistory.c_str(), old.c_str() ) < 0 ) {
				dprintf(D_ALWAYS, "openJobHistory() link(%s,%s): %s\n",
						m_JobHistory.c_str(), old.c_str(), strerror(errno) );
				return -1;
			}
			if ( unlink( m_JobHistory.c_str() ) < 0 ) {
				dprintf(D_ALWAYS, "openJobHistory() unlink(%s): %s\n",
						m_JobHistory.c_str(), strerror(errno) );
				return -1;
			}

			// The unlinked file should be gone!
			StatInfo goner( m_JobHistory.c_str() );
			if (goner.Error() == 0) {
				dprintf(D_ALWAYS, "openJobHistory() unlink(%s) succeeded but "
						"file still exists!\n", m_JobHistory.c_str() );
				return -1;
			}
#endif
		}
	}

	const int flags = O_WRONLY|O_CREAT|O_APPEND|O_LARGEFILE;
	const int mode = 0644;
	int fd = open( m_JobHistory.c_str(), flags, mode);
	if (fd < 0) {
		dprintf(D_ALWAYS, "open job history file %s for write: %s\n",
				m_JobHistory.c_str(), strerror(errno) );
	}
	return fd;
}

// Initialize job queue ClassAd Collection
void
Stork::initializeJobQueue( const char* spool )
{
	// Setup the root view of the jobs queue.
	m_RootJobsViewName = "root";
	m_RootJobsView = m_JobQ.GetView(m_RootJobsViewName);
	ASSERT(m_RootJobsView);

	// Initialize query.
	m_Query.Bind(&m_JobQ);

	// Recover job queue from persistent disk file, if enabled.
	// TODO What happens when the same views are defined above, and in the
	// persistent disk file?  Do they clash?  Does the collection start another
	// possibly redundant query with each new view definition?
	if ( param_boolean(STORK_JOB_QUEUE, STORK_JOB_QUEUE_DEFAULT) ) {
		string_printf(m_PersistentJobQ, "%s/%s", spool, STORK_JOB_QUEUE_LOG);

		dprintf(D_ALWAYS, "Initialize job queue from file %s\n",
				m_PersistentJobQ.c_str() );
		if (m_JobQ.InitializeFromLog(m_PersistentJobQ.c_str() ) ) {
			dprintf(D_FULLDEBUG, "Initialize job queue: success\n");
			if (! recoverJobQueue() ) {
				EXCEPT("Unable to recover job queue from %s\n",
						m_PersistentJobQ.c_str() );
			}
		} else {
			m_PersistentJobQ.clear();
			EXCEPT("Unable to initialize job queue from %s\n",
					m_PersistentJobQ.c_str() );
		}
	}

	// Test access to completed job history file, if enabled.
	if ( param_boolean(STORK_JOB_HISTORY, STORK_JOB_HISTORY_DEFAULT) ) {
		string_printf(m_JobHistory, "%s/%s", spool, STORK_JOB_HISTORY_LOG);

		dprintf(D_ALWAYS, "Initialize job history file %s\n",
				m_JobHistory.c_str() );
		int fd = openJobHistory();
		if (fd < 0) {
			EXCEPT("Unable to initialize job history file %s: %s\n",
					m_JobHistory.c_str(), strerror(errno) );
		}
		close(fd);
	}

	// FIXME: DO NOT BE TEMPTED TO DEFINE VIEWS BEFORE RECOVERING JOB QUEUE, OR [YET]
	// UNEXPLAINED MEMORY ERRORS RESULT.

	// Create idle jobs view of the job queue.
	m_IdleJobsViewName = "idleJobs";
#if 0
		// The view may have been recovered from the history file.
	if (! m_JobQ.ViewExists( m_IdleJobsViewName ) ) {
		string constraint = STORK_RESCHEDULE_REQUIREMENTS_DEFAULT;
		dprintf(D_FULLDEBUG, "initialize %s = \"%s\"\n",
				STORK_RESCHEDULE_REQUIREMENTS,
				STORK_RESCHEDULE_REQUIREMENTS_DEFAULT);
		string rank = STORK_RESCHEDULE_RANK_DEFAULT;
		dprintf(D_FULLDEBUG, "initialize %s = \"%s\"\n",
				STORK_RESCHEDULE_RANK,
				STORK_RESCHEDULE_RANK_DEFAULT);
		string partitionExprs;
		dprintf(D_FULLDEBUG, "create job queue view %s\n",
				m_IdleJobsViewName.c_str() );
		if (! m_JobQ.CreateSubView( m_IdleJobsViewName, m_RootJobsViewName,
					constraint, rank, partitionExprs ) ) {
			EXCEPT("Unable to create idle jobs view of job queue: %s\n",
				classad::CondorErrMsg.c_str() );
		}
	}
	// TODO Is this a memory leak?
	m_IdleJobsView = m_JobQ.GetView( m_IdleJobsViewName );
	ASSERT(m_IdleJobsView);
#endif

	// Create running jobs view of the job queue.
	m_RunJobsViewName = "runJobs";
		// The view may have been recovered from the history file.
	if (! m_JobQ.ViewExists( m_RunJobsViewName ) ) {
		string constraint;		// no constraint
		string rank;			// no rank
		string partitionExprs;	// no partitionExprs
		dprintf(D_FULLDEBUG, "create job queue view %s\n",
				m_IdleJobsViewName.c_str() );
		if (! m_JobQ.CreateSubView( m_RunJobsViewName, m_RootJobsViewName,
					constraint, rank, partitionExprs ) ) {
			EXCEPT("Unable to create running jobs view of job queue: %s\n",
				classad::CondorErrMsg.c_str() );
		}
	}
	m_RunJobsView = m_JobQ.GetView( m_RunJobsViewName );
	ASSERT(m_RunJobsView);

	return;
}

// Delete job queue views.
void
Stork::deleteJobQueueViews(void)
{
	typedef vector<string> ViewList;
	ViewList views;
	ViewList::const_iterator viewNameIter;

	if ( ! m_JobQ.GetSubordinateViewNames(m_RootJobsViewName, views) ) {
		dprintf(D_ALWAYS, "destroyJobQueueViews() GetSubordinateViewNames() "
				"failed: %s\n", classad::CondorErrMsg.c_str() );
		return;
	}

	for	(	viewNameIter=views.begin();
			viewNameIter!=views.end();
			viewNameIter++
		)
	{
		const classad::ViewName viewName = *viewNameIter; // ClassAd type bug!
		dprintf(D_FULLDEBUG, "Destroying job queue view %s\n",
				viewName.c_str());
		if ( ! m_JobQ.DeleteView( viewName) ) {
			dprintf(D_ALWAYS, "destroyJobQueueViews() DeleteView(%s) "
				"failed: %s\n", viewName.c_str(),
				classad::CondorErrMsg.c_str() );
		}
	}
}

/// Dynamic configuration, repeated upon reconfig command.
void
Stork::configure( const bool is_full_reconfig)
{
	dprintf(D_ALWAYS, "Configuring Stork\n");

	// Register any timer values that have changed.
	registerTimers();

	// Module stat cache entry time to live.
	m_ModuleCacheTTL =
		param_integer(
				STORK_MODULE_CACHE_TTL, 		// name
				STORK_MODULE_CACHE_TTL_DEFAULT,	// default value
				STORK_MODULE_CACHE_TTL_MIN		// min value
		);

	m_MaxRunningJobs =
			param_integer(
					STORK_MAX_NUM_JOBS,			// name
					STORK_MAX_NUM_JOBS_DEFAULT,	// default value
					STORK_MAX_NUM_JOBS_MIN		// min value
			);

	m_JobStartCount =
			param_integer(
					STORK_JOB_START_COUNT,			// name
					STORK_JOB_START_COUNT_DEFAULT,	// default value
					STORK_JOB_START_COUNT_MIN		// min value
			);

	m_MinRescheduleInterval =
			param_integer(
					STORK_MIN_RESCHEDULE_INTERVAL,			// name
					STORK_MIN_RESCHEDULE_INTERVAL_DEFAULT,	// default value
					STORK_MIN_RESCHEDULE_INTERVAL_MIN		// min value
			);
	int tmpRescheduleInt =
		param_integer(
				STORK_RESCHEDULE_INTERVAL, 		// name
				STORK_RESCHEDULE_INTERVAL_DEFAULT,	// default value
				STORK_RESCHEDULE_INTERVAL_MIN		// min value
			);
	// If min reschedule interval is greater than reschedule interval, this is
	// a configuration error.  Use half reschedule interval .
	if ( m_MinRescheduleInterval > tmpRescheduleInt) {
		m_MinRescheduleInterval = tmpRescheduleInt / 2;
		dprintf(D_ALWAYS, "config error: %s exceeds %s, setting %s to %lu\n",
			STORK_MIN_RESCHEDULE_INTERVAL,
			STORK_RESCHEDULE_INTERVAL,
			STORK_MIN_RESCHEDULE_INTERVAL,
			(unsigned long)m_MinRescheduleInterval);
	}

	// Configure idle jobs view.
	configureIdleJobsView();

	return;
}


/// Configure idle jobs view.
void
Stork::configureIdleJobsView( void )
{
	char* tmp = NULL;

	// Evaluate reschedule requirements.
	bool rescheduleRequirementsChanged = false;
	string tmpRescheduleRequirements;
	tmp = param( STORK_RESCHEDULE_REQUIREMENTS );

	// Default requirements
	if  ( ! tmp ) {
		tmp = strdup(STORK_RESCHEDULE_REQUIREMENTS_DEFAULT);
	}

	if (tmp) {
		// Config param value is defined.
		// Always append additional requirement that job is idle.
		string_printf(tmpRescheduleRequirements,
				"((%s) && (other.%s == \"%s\"))",
				tmp, STORK_JOB_ATTR_STATUS, STORK_JOB_STATUS_IDLE);
		if (tmp) {
			free(tmp);
			tmp = NULL;
		}

		// Change view requirements only if parameter has changed, to avoid a
		// potentially expensive new query.
		if ( m_RescheduleRequirements != tmpRescheduleRequirements ) {
			// config param value has changed
			classad::ExprTree* expr = NULL;
			if	( m_Parser.ParseExpression( tmpRescheduleRequirements, expr,
						true)
				)
			{
				// config param value is a valid expression.  Note that
				// SetViewInfo() below probably checks this anyhow, but the
				// returned error message may not indicate this param
				// expression parse failed.
				rescheduleRequirementsChanged = true;
			} else {
				dprintf(D_ALWAYS, "%s value %s is not a valid expression: %s\n",
					STORK_RESCHEDULE_REQUIREMENTS,
					tmpRescheduleRequirements.c_str(),
					classad::CondorErrMsg.c_str() );
			}
			if (expr) delete expr;
		}
	}

	// Evaluate reschedule rank.
	bool rescheduleRankChanged = false;
	string tmpRescheduleRank;
	tmp = param( STORK_RESCHEDULE_RANK );

	// Default rank.
	if  ( ! tmp ) {
		tmp = strdup(STORK_RESCHEDULE_RANK_DEFAULT);
	}

	if (tmp) {
		// Config param value is defined.
		tmpRescheduleRank = tmp;
		if (tmp) {
			free(tmp);
			tmp = NULL;
		}

		// Change view requirements only if parameter has changed, to avoid a
		// potentially expensive new query.
		if ( m_RescheduleRank != tmpRescheduleRank ) {
			// config param value has changed
			classad::ExprTree* expr = NULL;
			if ( m_Parser.ParseExpression( tmpRescheduleRank, expr, true) ) {
				// config param value is a valid expression.  Note that
				// SetViewInfo() below probably checks this anyhow, but the
				// returned error message may not indicate this param
				// expression parse failed.
				rescheduleRankChanged = true;
			} else {
				dprintf(D_ALWAYS, "%s value %s is not a valid expression: %s\n",
					STORK_RESCHEDULE_RANK,
					tmpRescheduleRank.c_str(),
					classad::CondorErrMsg.c_str() );
			}
			if (expr) delete expr;
		}
	}

	// update the Idle Jobs View only if the requirements or rank expressions
	// have changed.  Note that changing the view will force a new query of the
	// job queue.
	if ( rescheduleRequirementsChanged || rescheduleRankChanged ) {
		string tmpRequirements = rescheduleRequirementsChanged ?
			tmpRescheduleRequirements : m_RescheduleRequirements;
		string tmpRank = rescheduleRankChanged ?
			tmpRescheduleRank : m_RescheduleRank;
		string partitionExprs;
		if ( ! m_JobQ.DeleteView( m_IdleJobsViewName ) ) {
			// This error can occur upon startup.
			dprintf(D_ALWAYS, "DeleteView(%s): %s\n",
					m_IdleJobsViewName.c_str(), classad::CondorErrMsg.c_str() );
		}
		if (! m_JobQ.CreateSubView( m_IdleJobsViewName, m_RootJobsViewName,
					tmpRescheduleRequirements, tmpRescheduleRank, partitionExprs ) ) {
			EXCEPT("Unable to create idle jobs view of job queue: %s\n",
				classad::CondorErrMsg.c_str() );
		}

		dprintf(D_ALWAYS,
				"successfully updated idle jobs reschedule view\n");
		if (rescheduleRequirementsChanged) {
			dprintf(D_ALWAYS, "%s = \"%s\"\n", 
					STORK_RESCHEDULE_REQUIREMENTS,
					tmpRequirements.c_str() );
			m_RescheduleRequirements = tmpRequirements;
		}
		if (rescheduleRankChanged) {
			dprintf(D_ALWAYS, "%s = \"%s\"\n", 
					STORK_RESCHEDULE_RANK,
					tmpRank.c_str() );
			m_RescheduleRank = tmpRank;
		}

		// TODO Is this a memory leak?
		m_IdleJobsView = m_JobQ.GetView( m_IdleJobsViewName );
		ASSERT(m_IdleJobsView);

	}
	return;
}

void
Stork::shutdown( const bool is_graceful )

{
	dprintf(D_ALWAYS, "shutdown(%s) called\n",
			is_graceful ? "graceful" : "fast");
	// Cancel/destroy daemoncore commands reapers and timers
	//cancelCmds();			// TODO do we _really_ want to to this?
	//cancelReapers();		// TODO do we _really_ want to to this?
	//cancelTimers();		// TODO do we _really_ want to to this?

	// TODO: disable DC commands

	// Save max job id.
	classad::ClassAd* ad = new classad::ClassAd;
	StorkJobId jobId = m_NextJobId;
	--jobId;
	jobId.updateClassAd(*ad);
	dprintf(D_ALWAYS, "saving max job id = %s\n", jobId.fmt() );
	if (! m_JobQ.AddClassAd( KEY_MAXJOBID, ad) ) {
		dprintf(D_ALWAYS, "save max job id error: %s\n",
				classad::CondorErrMsg.c_str() );
	}

	if ( is_graceful ) {
		// TODO: soft kill child processes

		// At next server startup, we want to define views from config file,
		// not from peristent jobQ log file.
		deleteJobQueueViews();	// TODO is this helpful?
		if ( m_PersistentJobQ.length() > 0 ) {
			timerHandler_cleanJobQ();
		}
	}

	// TODO: hard kill child processes
	return;
}

void
Stork::registerCmds(void)
{
	dprintf(D_ALWAYS, "Register commands\n");

	// Submit job command.
	if (m_SubmitCmdId != NULL_CMD_ID) {
		daemonCore->Cancel_Command(m_SubmitCmdId);
	}
	m_SubmitCmdId =
		daemonCore->Register_Command(
			STORK_SUBMIT,
			"STORK_SUBMIT",
			(CommandHandlercpp)&Stork::commandHandler_SUBMIT,
			"commandHandler_SUBMIT",
			(Service*)this,
			WRITE
		);
	ASSERT(m_SubmitCmdId != NULL_CMD_ID);		// TODO: handle this?

	return;
}

void
Stork::cancelCmds(void)
{
	dprintf(D_ALWAYS, "Canceling commands\n");

	if (m_SubmitCmdId != NULL_CMD_ID) {
		daemonCore->Cancel_Command(m_SubmitCmdId);
		m_SubmitCmdId = NULL_CMD_ID;
	}

	return;
}

void
Stork::registerReapers(void)
{
	dprintf(D_ALWAYS, "Register reapers\n");

	// Submit job command.
	if (m_JobReaperId != NULL_REAPER_ID) {
		daemonCore->Reset_Reaper(	m_JobReaperId,
									"transfer job reaper",
									(ReaperHandlercpp)&Stork::commonJobReaper,
									"Stork::commonJobReaper",
									this);
	}
	m_JobReaperId =
		daemonCore->Register_Reaper(
									"transfer job reaper",
									(ReaperHandlercpp)&Stork::commonJobReaper,
									"Stork::commonJobReaper",
									this);
			
	ASSERT(m_JobReaperId != NULL_REAPER_ID);	// TODO: handle this?

	return;
}

void
Stork::cancelReapers(void)
{
	dprintf(D_ALWAYS, "Canceling reapers\n");

	if (m_JobReaperId != NULL_REAPER_ID) {
		daemonCore->Reset_Reaper(	m_JobReaperId,
									"transfer job reaper",
									(ReaperHandlercpp)&Stork::commonJobReaper,
									"Stork::commonJobReaper",
									this);
		m_JobReaperId = NULL_REAPER_ID;
	}

	return;
}

int
Stork::requireAuthentication(
		const Stream *stream,
		const char* subsys,
		const int code,
		const char* message ) const
{
	ReliSock* rsock = (ReliSock*)stream;

	if( ! rsock->isAuthenticated() ) {
		char * p =
			SecMan::getSecSetting ("SEC_%s_AUTHENTICATION_METHODS", "WRITE");

		MyString methods;	// note: secman requires MyString methods.
		if (p) {
			methods = p;
			free (p);
		} else {
			methods = SecMan::getDefaultAuthenticationMethods();
		}
		CondorError errstack;
		if( ! rsock->authenticate(methods.Value(), &errstack) ) {
			// we failed to authenticate, we should bail out now
			// since we don't know what user is trying to perform
			// this action.
			// TODO: it'd be nice to print out what failed, but we
			// need better error propagation for that...
			errstack.push( (char*)subsys, code, (char *)message );
			dprintf( D_ALWAYS, "aborting: %s\n", errstack.getFullText() );
			return FALSE;
		}
	}

	return TRUE;
}

// STORK_SUBMIT command handler.
// server over-the-wire protocol:
//
//	mode		data		type
//	receive		job			ClassAd (character string)
//	receive		credential	character string
//  end of message
//	send		jobId		character string, empty string for error
//  end of message
int
Stork::commandHandler_SUBMIT( const int command, Stream *stream)
{
	int rtnStatus = FALSE;	// this function return status
	char *credential = NULL;
	// The ClassAdCollection job queue will take ownership of the ad, so the ad
	// must be malloc'ed.
	classad::ClassAd* ad = new classad::ClassAd;
	StorkJobId jobId;
	string submit_host;		// remote submit host sinful string
	string user;			// remote job user
	string error;			// last error.  The submit error string is saved
							// until the end of this handler, and echoed across
							// the wire to the client.
	StorkUserLog userLog;	// user logging object;
	string jobDir;			// job run directory

	// Set a timeout, since we don't want to block long trying to read from our
	// client.
	stream->timeout(SERVER_SOCK_TIMEOUT);

	// This command must be authenticated
	if	(!  requireAuthentication( stream, m_Subsytem, STORK_SUBMIT_FAILED, 
				"submit command authentication error" )
		)
	{
		error = "submit command authentication error";
		goto EXIT;
	}

	// Get job ad
	if ( !StreamGet( stream, *ad ) ) {
		string_printf(error, "submit command jobAd rcv error: %s",
				strerror(errno) );
		goto EXIT;
	}

	// Log job receipt.
	jobId = getNextJobId();
	user = ((ReliSock *)stream)->getFullyQualifiedUser();
	ad->InsertAttr(STORK_JOB_ATTR_USER, user);
	submit_host = sin_to_string( ((ReliSock *)stream)->endpoint() );
    ad->InsertAttr(STORK_JOB_ATTR_SUBMIT_HOST, submit_host);
	jobId = m_NextJobId;
	dprintf(D_ALWAYS, "%s submitted from %s:\n",
			jobId.fmt(), user.c_str() );
	if(DebugFlags & D_FULLDEBUG) {
		string unparsed;
		m_Unparser.Unparse( unparsed, ad);
		dprintf(D_FULLDEBUG, "%s\n", unparsed.c_str() );
	}

	// Get credential
	if (! stream->get(credential) ) {
		string_printf(error,
				"submit command spooled credential rcv error: %s",
				strerror(errno) ) ;
		goto EXIT;
	}

	// Insert required job attributes
	jobId.updateClassAd(*ad);
	ad->InsertAttr(STORK_JOB_ATTR_STATUS, STORK_JOB_STATUS_IDLE);
	ad->Insert(STORK_JOB_ATTR_SUBMIT_TIME, classad::Literal::MakeAbsTime() );
	ad->Insert(STORK_JOB_ATTR_NUM_ATTEMPTS, 0);
	//ad->InsertAttr(STORK_JOB_ATTR_SUBMIT_TIME, (int)time(NULL) );

	// Get, check job privilege info.
	if (! getJobPrivilegeInfo(ad, stream, error) ) {
		// FIXME save error string
		goto EXIT;
	}

	// Create job run directory
	if (! createJobRunDir(ad, error, jobDir) ) {
		goto EXIT;
	}

	// spool credential
	if (strlen(credential) > 0) {
dprintf(D_FULLDEBUG, "DEBUG spool user proxy\n%30s\n",credential);
		if(! spoolCredential(ad, error, credential, jobDir) ) {
			goto EXIT;
		}
dprintf(D_FULLDEBUG, "DEBUG spool user proxy to %s\n", jobDir.c_str() );
	}

	// Check job ad for errors.
	if (! checkJobAd(ad, error) ) {
		goto EXIT;
	}

	// Add jobAd to job queue
	if (! m_JobQ.AddClassAd( jobId.key() , ad) ) {
		// note: AddClassAd() will delete the argument ad upon failure, so do
		// not delete the ad here.
		string_printf(error, "error adding job key=\"%s\" to collection: %s",
				(jobId.key() ).c_str(), classad::CondorErrMsg.c_str() );
		goto EXIT;
	}

	// Update user log with submit event.
	userLog = ad;
	userLog.submitEvent();

	rtnStatus = TRUE;	// command successfully handled
	++m_NextJobId;		// increment id for next new job

	// TODO Add user log submit event

EXIT:
	if ( ! stream->end_of_message() ) {
		if ( error.empty() ) {
			// preserve first error
			error = "submit command receive message error";
		}
	}
	if (! stream->put( (char *)jobId.c_str() ) ) {
		if ( error.empty() ) {
			// preserve first error
			error = "submit command jobId send error";
		}
	}
	if (! error.empty() ) {
		dprintf(D_ALWAYS, "%s\n", error.c_str() );
	}
	if (! stream->put( (char *)error.c_str() ) ) {
		dprintf(D_ALWAYS, "submit command error string send error");
	}
	// TODO: This last EOM can sometimes return failure, but appears to work
	// nonetheless.  Perhaps the client is closing the socket early?  Figger
	// out what's going on.
	stream->end_of_message();

	if ( rtnStatus == FALSE ) {
		if ( ad != NULL ) {
			destroyJobRunDir( ad );
			delete ad;
		}
	}
	if (credential) free(credential);

	// Give new jobs a chance to run.
	rescheduleJobs();

	return rtnStatus;
}

void
Stork::registerTimers(void)
{
	dprintf(D_ALWAYS, "Register timers\n");
	int interval;

	// Set timer to clean job queue.
	if (m_PersistentJobQ.length() > 0) {
		interval =
			param_integer(
					STORK_QUEUE_CLEAN_INTERVAL, 		// name
					STORK_QUEUE_CLEAN_INTERVAL_DEFAULT,	// default value
					STORK_QUEUE_CLEAN_INTERVAL_MIN		// min value
			);
		if ( interval != m_CleanJobQInterval ) {
			if ( m_CleanJobQTimerId != NULL_TIMER_ID ) {
				daemonCore->Cancel_Timer(m_CleanJobQTimerId);
			}
			m_CleanJobQTimerId =
				daemonCore->Register_Timer(
						interval,							// deltawhen, sec
						interval,							// period, sec
						(TimerHandlercpp)&Stork::timerHandler_cleanJobQ,
						"Stork::timerHandler_cleanJobQ",	// description
						this								// service
				);
			ASSERT(m_CleanJobQTimerId != NULL_TIMER_ID)	// TODO: handle this?
			if (m_CleanJobQTimerId != NULL_TIMER_ID) {
				m_CleanJobQInterval = interval;
			}
		}
	} // Set timer to clean job queue.

	// Set job reschedule interval.
	interval =
		param_integer(
				STORK_RESCHEDULE_INTERVAL, 		// name
				STORK_RESCHEDULE_INTERVAL_DEFAULT,	// default value
				STORK_RESCHEDULE_INTERVAL_MIN		// min value
		);
	if ( interval != m_RescheduleInterval ) {
		if ( m_RescheduleTimerId != NULL_TIMER_ID ) {
			daemonCore->Cancel_Timer(m_RescheduleTimerId);
		}
		m_RescheduleTimerId =
			daemonCore->Register_Timer(
					interval,							// deltawhen, sec
					interval,							// period, sec
					(TimerHandlercpp)&Stork::timerHandler_reschedule,
					"Stork::timerHandler_reschedule",	// description
					this								// service
			);
		ASSERT(m_RescheduleTimerId != NULL_TIMER_ID);	// TODO: handle this?
		if (m_RescheduleTimerId != NULL_TIMER_ID) {
			m_RescheduleInterval = interval;
		}
	} // Set job reschedule interval.

	return;
}

void
Stork::cancelTimers(void)
{
	dprintf(D_ALWAYS, "Canceling timers\n");
	if ( m_CleanJobQTimerId != NULL_TIMER_ID ) {
		daemonCore->Cancel_Timer(m_CleanJobQTimerId);
		m_CleanJobQTimerId = NULL_TIMER_ID;
		m_CleanJobQInterval = -1;
	}

	if ( m_RescheduleTimerId != NULL_TIMER_ID ) {
		daemonCore->Cancel_Timer(m_RescheduleTimerId);
		m_RescheduleTimerId = NULL_TIMER_ID;
		m_RescheduleInterval = -1;
	}

	return;
}

int
Stork::timerHandler_cleanJobQ(void)
{
	dprintf(D_ALWAYS, "Cleaning job queue %s\n", m_PersistentJobQ.c_str() );
	m_JobQ.TruncateLog();
	// Cleaning very large job queues can take a while ...
	dprintf(D_FULLDEBUG, "Done cleaning job queue %s\n",
			m_PersistentJobQ.c_str() );

	return TRUE;
}

int
Stork::timerHandler_reschedule(void)
{
	rescheduleJobs();
	return TRUE;
}

/// Check job ad for validity.
bool
Stork::checkJobAd(classad::ClassAd* ad, std::string& error)
{
	string type;
	if (! ad->EvaluateAttrString(STORK_JOB_ATTR_TYPE, type) ) {
		string_printf(error, "job missing attribute: %s",
				STORK_JOB_ATTR_TYPE);
		return false;
	}

	string stdio;
	if ( ad->EvaluateAttrString(STORK_JOB_ATTR_INPUT, stdio) ) {
		if ( full_access( *ad, (char *)stdio.c_str(), R_OK) ) {
			string_printf(error, "job %s file %s access error: %s",
					STORK_JOB_ATTR_INPUT,
					stdio.c_str(), strerror(errno) );
			return false;
		}
	}
	if ( ad->EvaluateAttrString(STORK_JOB_ATTR_OUTPUT, stdio) ) {
		if ( full_access( *ad, (char *)stdio.c_str(), W_OK) ) {
			string_printf(error, "job %s file %s access error: %s",
					STORK_JOB_ATTR_OUTPUT,
					stdio.c_str(), strerror(errno) );
			return false;
		}
	}
	if ( ad->EvaluateAttrString(STORK_JOB_ATTR_ERROR, stdio) ) {
		if ( full_access( *ad, (char *)stdio.c_str(), W_OK) ) {
			string_printf(error, "job output %s file %s access error: %s",
					STORK_JOB_ATTR_ERROR,
					stdio.c_str(), strerror(errno) );
			return false;
		}
	}

	// Perform job type specific checks.
	if ( type == STORK_JOB_TYPE_TRANSFER) {
		if (!  checkTransferJobAd(ad, error) ) {
			return false;
		}
		// add more job types here
	} else {
		string_printf(error, "unknown job type: %s", type.c_str() );
		return false;
	}

	// Verify module exists.  Check stat() cache first to avoid
	// unnecessary module stat().
	string module;
	if ( ! ad->EvaluateAttrString(STORK_JOB_ATTR_MODULE, module) ) {
		string_printf(error, "%s:%d system error accessing module",
				__FILE__, __LINE__);
		return false;
	}
	string2timeMap::const_iterator pos;
	pos = m_ModuleCache.find(module);
	bool doStat = false;
	time_t now = time(NULL);
	if (pos == m_ModuleCache.end() ) {
		// module not cached
		doStat = true;
		dprintf(D_FULLDEBUG, "module %s cache miss\n", module.c_str() );
	} else if (now - pos->second > m_ModuleCacheTTL) {
		doStat = true;
		m_ModuleCache.erase(module);
		dprintf(D_FULLDEBUG, "module %s cache entry expired\n",module.c_str() );
	}
	if (doStat) {
		if ( full_access(*ad, (char *)module.c_str(), X_OK) ) {
			string_printf(error, "required job module %s access error: %s",
					module.c_str(), strerror(errno) );
			return false;
		} else {
			m_ModuleCache[ module ] = now;
		}
	}

	// Check any proxy associated with job
	if (! checkJobAdProxy(ad, error) ) {
		return false;
	}

	// Build runtime exec arguments list from, in order:
	//	basename of module path (which can be user supplied) to be argv[0]
	//	any job type dependent runtime arguments prefix
	//	any user supplied runtime arguments
	string exec_args = condor_basename( module.c_str() );
	string tmp_exec_args;
	ad->EvaluateAttrString( STORK_JOB_ATTR_EXEC_ARGS, tmp_exec_args);
	if ( ! tmp_exec_args.empty() ) {
		exec_args += " ";
		exec_args += tmp_exec_args;
	}
	string arguments;
	ad->EvaluateAttrString( STORK_JOB_ATTR_ARGUMENTS, arguments);
	if ( ! arguments.empty() ) {
		exec_args += " ";
		exec_args += arguments;
	}
	ad->InsertAttr( STORK_JOB_ATTR_EXEC_ARGS, exec_args);

	// Build runtime exec environment list from, in order:
	//	any user supplied environment
	//	any job type dependent runtime environment
	// TODO verify any system appended environment, like X509_USER_PROXY trumps
	// user supplied environment.
#if 0
	string exec_env;
	ad->EvaluateAttrString( STORK_JOB_ATTR_EXEC_ENV, exec_env);
	string environment;
	ad->EvaluateAttrString( STORK_JOB_ATTR_ENVIRONMENT, environment);
	if ( ! environment.empty() ) {
		exec_env += " ";
		exec_env += environment;
	}
	ad->InsertAttr( STORK_JOB_ATTR_EXEC_ENV, exec_env);
#endif

	return true;
}

/// Check transfer job ad for validity.
bool
Stork::checkTransferJobAd(classad::ClassAd* ad, string& error)
{
	string module;
	string srcUrl;
	string destUrl;

	// Ad should specifiy either a module, or both source and destination URLs.
	// This format enables module command line args to be passed directly to
	// globus-url-copy .
	if ( ad->EvaluateAttrString(STORK_JOB_ATTR_MODULE, module) ) {
		// module specified: then neither src or dest URLs are allowed
		if ( ad->EvaluateAttrString(STORK_JOB_ATTR_SRC_URL, srcUrl) ) {
			string_printf(error, "specify either %s, or both %s and %s",
					STORK_JOB_ATTR_MODULE,
					STORK_JOB_ATTR_SRC_URL,
					STORK_JOB_ATTR_DEST_URL
			);
			return false;
		}
		if ( ad->EvaluateAttrString(STORK_JOB_ATTR_DEST_URL, destUrl) ) {
			string_printf(error, "specify either %s, or both %s and %s",
					STORK_JOB_ATTR_MODULE,
					STORK_JOB_ATTR_SRC_URL,
					STORK_JOB_ATTR_DEST_URL
			);
			return false;
		}
	} else {
		// module not specified: then both src and dest URLs are required

		// Check source URL
		UrlParser srcUrlParser;
		if ( ad->EvaluateAttrString(STORK_JOB_ATTR_SRC_URL, srcUrl) ) {
			srcUrlParser = srcUrl;
			if (! srcUrlParser.parse() ) {
				string_printf(error, "parse %s %s: %s\n",
						STORK_JOB_ATTR_SRC_URL,
						srcUrl.c_str(), srcUrlParser.errorMsg() );
				return false;
			}
			string protocol = srcUrlParser.protocol();
			if ( protocol == "file" ) {
				if ( full_access( *ad, (char *)srcUrlParser.path(), R_OK) ) {
					string_printf(error, "%s access error: %s",
							srcUrlParser.path(), strerror(errno) );
					return false;
				}
			}
		} else {
			string_printf(error, "missing job attribute: %s",
					STORK_JOB_ATTR_SRC_URL);
			return false;
		}

		// Check destination URL
		UrlParser destUrlParser;
		if ( ad->EvaluateAttrString(STORK_JOB_ATTR_DEST_URL, destUrl) ) {
			destUrlParser = destUrl;
			if (! destUrlParser.parse() ) {
				string_printf(error, "parse %s %s: %s\n",
						STORK_JOB_ATTR_DEST_URL,
						destUrl.c_str(), destUrlParser.errorMsg() );
				return false;
			}
			string protocol = destUrlParser.protocol();
			if ( protocol == "file" ) {
				if ( full_access( *ad, (char *)destUrlParser.path(), R_OK) ) {
					string_printf(error, "%s access error: %s",
							destUrlParser.path(),
							strerror(errno) );
					return false;
				}
			}
		} else {
			string_printf(error, "missing job attribute: %s",
					STORK_JOB_ATTR_DEST_URL);
			return false;
		}

		// Determine transfer module path
		string_printf(module, "%s/stork.transfer.%s-%s",
			m_ModuleDir, srcUrlParser.protocol(), destUrlParser.protocol() );
		ad->InsertAttr(STORK_JOB_ATTR_MODULE, module);
	}

	// Prepend source and destination URLs to runtime arguments string.
	if ( ! srcUrl.empty() &&  ! destUrl.empty() ) {
		string exec_args;
		string_printf(exec_args, "%s %s", srcUrl.c_str(), destUrl.c_str() );
		ad->InsertAttr(STORK_JOB_ATTR_EXEC_ARGS, exec_args);
	}

	return true;
}

/// Check job ad proxy
bool
Stork::checkJobAdProxy(classad::ClassAd* ad, string& error)
{
	string proxy;
	if ( ! ad->EvaluateAttrString(STORK_JOB_ATTR_X509PROXY, proxy) ) {
		return true;
	}

	// If proxy value is default, then locate proxy using GSI search path.
	if ( proxy == STORK_JOB_X509PROXY_DEFAULT ) {
		priv_state initialPriv = set_user_priv();
		// Locate default proxy as user
		char *defproxy = get_x509_proxy_filename();
		set_priv(initialPriv);	// restore initial privilege state
		if (defproxy) {
			dprintf(D_ALWAYS, "using default proxy: %s\n", defproxy);
			proxy = defproxy;
			free(defproxy);
			ad->InsertAttr(STORK_JOB_ATTR_X509PROXY, proxy);
		} else {
			string_printf(error, "locate default proxy: %s",
					x509_error_string() );
			return false;
		}
	}

	if ( param_boolean(STORK_CHECK_PROXY, STORK_CHECK_PROXY_DEFAULT) ) {

        // Do a quick check on the proxy.

		priv_state initialPriv = set_user_priv();
		int status = x509_proxy_try_import( proxy.c_str() ) ;
		set_priv(initialPriv);	// restore initial privilege state
        if ( status != 0 ) {
            string_printf(error, "check proxy %s: %s",
                    proxy.c_str(), x509_error_string() );
            return false;
        }
		initialPriv = set_user_priv();
        int remaining =
            x509_proxy_seconds_until_expire( proxy.c_str() );
		set_priv(initialPriv);	// restore initial privilege state
        if (remaining < 0) {
            string_printf(error, "check proxy %s expiration: %s",
                    proxy.c_str(),
                    x509_error_string() );
            return false;
        }
        if (remaining == 0) {
            string_printf(error, "proxy %s has expired",
                    proxy.c_str() );
            return false;
        }
	}

	// Associate proxy with job via environment.
	string exec_env = "X509_USER_PROXY=";
	exec_env += proxy;
	ad->InsertAttr(STORK_JOB_ATTR_EXEC_ENV, exec_env);

	return true;
}

#if 0
/// Add a job ad to the queue.
jobId_t
Stork::addJob(classad::ClassAd* ad)
{
	string unparsed;
	bool status;
	jobId_t jobId;
	string jobIdStr;

	jobId = getNextJobId();
	ad->InsertAttr(STORK_JOB_ATTR_ID, (int)jobId);
	if(DebugFlags & D_FULLDEBUG) {
		unparser.Unparse( unparsed, ad);
		dprintf(D_FULLDEBUG, "add job\n%s\nto job queue\n",
				unparsed.c_str() );
	}
	string_printf(jobIdStr, FMTJOBID, jobId);
	status =  m_JobQ.AddClassAd(jobIdStr, ad);
	if (! status) {
		// note: AddClassAd() will delete the argument ad upon failure, so do
		// not delete the ad here.
		jobId = _INVALID_JOB_ID;	// never return a an invalid jobId
		ad = NULL;
		dprintf(D_ALWAYS, "unable to add job %s to queue\n",
				jobIdStr.c_str() );
	}

	return jobId;
}
#endif

/// Get next jobId
StorkJobId
Stork::getNextJobId(void)
{
	while (1) {

		// Ensure candidate jobId is not in use.
		if (m_JobQ.GetClassAd( m_NextJobId.key() ) ) {
			// This jobId is in use.  Try the next.
			++m_NextJobId;
			continue;
		}

		// Return after finding a valid, unique jobId;
		break;
	}
	return m_NextJobId;
}

// Update a jobAd in the jobQ
bool
Stork::updateJobAd(const std::string& key, classad::ClassAd* deltaAd)
{
	bool status;

	// Make a copy of the ad, and create a "updates" ad
	classad::ClassAd *updates = new classad::ClassAd( );
    updates->Insert( "Updates", deltaAd );

    status = m_JobQ.ModifyClassAd( key, updates );
	// note: ModifyClassAd() will delete the argument ad upon failure, so do
	// not delete the ad here.
	if (! status) {
		string deltaAdStr;
		m_Unparser.Unparse(deltaAdStr, deltaAd);
		dprintf(D_ALWAYS,
				"error modifying job %s ad.  Delta ad:\n%s\nreason: %s\n",
				key.c_str(),
				deltaAdStr.c_str(),
				classad::CondorErrMsg.c_str() );

	}
    return status;
}

// Cold start recover job queue from log file.
// TODO This algorithm is too inefficient for large job queues, in that it
// walks _every_ job ad in the queue.  A better algorithm would be to attempt
// to recover only the maxJobId and all running jobs from the collection.
// However, attempts to define the running jobs view before calling the
// ClassAdCollection::InitializeFromLog() have always produced memory errors.
bool
Stork::recoverJobQueue( void )
{
	int jobRunCount = 0;
	dprintf(D_FULLDEBUG, "recover job queue\n") ;

	// Query all jobs.
	jobList jobAds;
	string nullQuery;
	if (! query( jobAds, m_RootJobsViewName, nullQuery) ) {
		EXCEPT("recoverJobQueue query %s error\n", m_RootJobsViewName.c_str() );
	}

	// FIXME remove any jobs in "remove state

	for	(	jobList::const_iterator iter = jobAds.begin();
			iter != jobAds.end();
			iter++
		)
	{
        classad::ClassAd* ad = *iter;	// Get job ad.

		// Find max jobId
		StorkJobId jobId = *ad;
		if ( jobId.error() ) continue;	// not a job ad
		if ( jobId > m_NextJobId ) m_NextJobId = jobId;

		// Move running jobs to idle state
		string             		status;			// job status
		if (! ad->EvaluateAttrString(STORK_JOB_ATTR_STATUS, status) ) {
			continue;	// not a job ad
		}

		if (status == STORK_JOB_STATUS_RUN) {
			jobRunCount++;
			if(DebugFlags & D_FULLDEBUG) {
				dprintf(D_FULLDEBUG,
				"WARNING: previous server shutdown left %s running!\n",
					jobId.fmt() );
			}
			classad::ClassAd *deltaAd = new classad::ClassAd( );
			deltaAd->InsertAttr(STORK_JOB_ATTR_STATUS, STORK_JOB_STATUS_IDLE);
			updateJobAd(jobId.key() , deltaAd);
		}
	}

	if (jobRunCount > 0) {
		dprintf(D_ALWAYS,
				"WARNING: previous server shutdown left %d jobs running!\n",
				jobRunCount);
	}
	dprintf(D_ALWAYS, "max job id at previous server shutdown = %s\n",
			m_NextJobId.fmt() );

	++m_NextJobId;
	return true;
}

// Return job run directory path
bool
Stork::jobRunDir(	const classad::ClassAd* ad,
					std::string& dir ) const
{
	StorkJobId jobId(*ad);
	const char* fmt = "%s/%s";
	string_printf(dir, fmt, m_EXECUTE, jobId.c_str() );
	return true;
}

// Initialise top level jobs run directory
void
Stork::initExecDir(void) const
{
	StatInfo execDirStat( m_EXECUTE );
	priv_state initialPriv = get_priv();
	const mode_t desiredMode = (0777 | S_ISVTX);	// TODO: UNIX ONLY

	// Ensure execute directory exists.
	if ( execDirStat.Error() != 0 ) {
		if ( execDirStat.Errno() == ENOENT ) {
			// job execute directory does not exist
			dprintf(D_ALWAYS, "creating %s\n", m_EXECUTE );
			initialPriv = set_condor_priv();
			if ( mkdir( m_EXECUTE , desiredMode) < 0 ) {
				// failed to create job execute directory
				set_priv(initialPriv);	// restore initial privilege state
				EXCEPT("mkdir %s: %s\n", m_EXECUTE, strerror(errno) );
			}
			set_priv(initialPriv);	// restore initial privilege state
		} else {
			// failed to stat job execute directory
			EXCEPT("stat %s: %s\n", m_EXECUTE,
					strerror(execDirStat.Errno() ) );
		}
	} else if ( ! execDirStat.IsDirectory() ) {
		// job execute stat entry is not a directory.
		EXCEPT("%s: is not a directory\n", m_EXECUTE );
	}

	// Ensure directory access permissions.
	if ( (execDirStat.GetMode() & desiredMode) != desiredMode) {
		dprintf(D_ALWAYS, "changing %s permissions from %#o to %#o\n",
				m_EXECUTE, execDirStat.GetMode(), desiredMode );
		initialPriv = set_condor_priv();
		if ( chmod( m_EXECUTE, desiredMode ) < 0 ) {
			set_priv(initialPriv);	// restore initial privilege state
			EXCEPT("chmod %s: %s\n", m_EXECUTE, strerror(errno) );
		}
		set_priv(initialPriv);	// restore initial privilege state
	}

	// Wipe entire execute directory tree.
	Directory execDir( m_EXECUTE, initialPriv);
	dprintf(D_ALWAYS, "clearing out %s\n", m_EXECUTE );
	if (! execDir.Remove_Entire_Directory() ) {
		EXCEPT("%s Remove_Entire_Directory: %s\n", m_EXECUTE,
				strerror(errno) );
	}

	return;
}

// Create job run directory
bool
Stork::createJobRunDir(	classad::ClassAd* ad,
						string& error,
						string& jobDir
						) const
{
	StorkJobId jobId(*ad);					// get job id
	string owner;							// job owner
	string domain;							// job domain: WIN32 only.
	priv_state initialPriv = get_priv();
	bool rtnStatus = false;

	jobRunDir(ad, jobDir);	// get job run dir path
	dprintf(D_FULLDEBUG, "create job execute directory %s\n",
			jobDir.c_str() );

	StatInfo jobDirStat( jobDir.c_str() );
	if ( jobDirStat.Error() == 0 && jobDirStat.IsDirectory() ) {
		// directory already exists
		return true;
	}

	if	(	! ad->EvaluateAttrString(STORK_JOB_ATTR_OWNER, owner) ||
			owner.empty()
		)
	{
		string_printf(error, "%s: no %s", jobId.fmt(), STORK_JOB_ATTR_OWNER);
		goto EXIT;
	}

#ifdef WIN32
	if	(	! ad->EvaluateAttrString(STORK_JOB_ATTR_DOMAIN, domain) ||
			owner.empty()
		)
	{
		string_printf(error, "job " FMTJOBID ": unable to get domain", jobId);
		goto EXIT;
	}
#endif

	// On Unix, be sure we're in user priv for this.
	// But on NT (at least for now), we should be in Condor priv
	// because the execute directory on NT is not world writable.
#ifndef WIN32
	// UNIX
	if (! init_user_ids(owner.c_str(), domain.c_str() ) ) {
		string_printf(error, "%s : init_user_ids error", jobId.fmt() );
		goto EXIT;
	}
    initialPriv = set_user_priv();
#else
	// WIN32
    initialPriv = set_condor_priv();
#endif

	// Create job run directory
	if ( mkdir( jobDir.c_str() , 0755) < 0 ) {
		set_priv(initialPriv);	// restore initial privilege state
		string_printf(error, "create %s run directory %s failed: %s",
			jobId.fmt() , jobDir.c_str(), strerror(errno) );
		goto EXIT;
	}
	set_priv(initialPriv);	// restore initial privilege state
	ad->InsertAttr(STORK_JOB_ATTR_RUN_DIR, jobDir);

	rtnStatus = true;
EXIT:
	return rtnStatus;
}

// Destroy job run directory
bool
Stork::destroyJobRunDir(const classad::ClassAd* ad) const
{
	string jobDir;
	priv_state initialPriv = get_priv();

	if (! ad->EvaluateAttrString(STORK_JOB_ATTR_RUN_DIR, jobDir) ) {
		return true;
	}
	dprintf(D_FULLDEBUG, "destroy job execute directory %s\n",
			jobDir.c_str() );

	Directory dir( jobDir.c_str(), PRIV_ROOT );

	if (! dir.Remove_Entire_Directory() ) {
		dprintf(D_ALWAYS, "%s Remove_Entire_Directory(%s) failed: %s\n",
				"destroyJobRunDir", jobDir.c_str(), strerror(errno) );
		return false;
	}

	initialPriv = set_root_priv();
	if ( rmdir( jobDir.c_str() ) < 0 ) {
		set_priv(initialPriv);	// restore initial privilege state
		dprintf(D_ALWAYS, "%s rmdir(%s) failed: %s\n",
				"destroyJobRunDir", jobDir.c_str(), strerror(errno) );
		return false;
	}
	set_priv(initialPriv);	// restore initial privilege state

	return true;
}

/// Spool credential to job run dir.
bool
Stork::spoolCredential(	classad::ClassAd* ad,
						std::string& error,
						const char *credential,
						const std::string& jobDir)
{
	StorkJobId jobId(*ad);					// get job id
	string credFile;
	const mode_t mode = 0600;				// Mode value is _crucial_ .
	string owner;							// job owner
	string domain;							// job domain: WIN32 only.
	priv_state initialPriv = get_priv();
	bool rtnStatus = false;
	int fd;
	string environment;
	Env env;
	MyString errorMsg;
    string X509_USER_PROXY;
    MyString delimitedString;
    string exec_env;

	if	(	! ad->EvaluateAttrString(STORK_JOB_ATTR_OWNER, owner) ||
			owner.empty()
		)
	{
		string_printf(error, "%s: no %s", jobId.fmt(), STORK_JOB_ATTR_OWNER);
		goto EXIT;
	}

#ifdef WIN32
	if	(	! ad->EvaluateAttrString(STORK_JOB_ATTR_DOMAIN, domain) ||
			owner.empty()
		)
	{
		string_printf(error, "job " FMTJOBID ": unable to get domainn", jobId);
		goto EXIT;
	}
#endif

	// On Unix, be sure we're in user priv for this.
	// But on NT (at least for now), we should be in Condor priv
	// because the execute directory on NT is not world writable.
#ifndef WIN32
	// UNIX
	if (! init_user_ids(owner.c_str(), domain.c_str() ) ) {
		string_printf(error, "%s: init_user_ids() error", jobId.fmt() );
		goto EXIT;
	}
    initialPriv = set_user_priv();
#else
	// WIN32
    initialPriv = set_condor_priv();
#endif

	// Create credential file.
	string_printf(credFile, "%s/x509proxy", jobDir.c_str() );
	fd = open (credFile.c_str(), O_CREAT|O_WRONLY, mode);
	if (fd < 0 ) {
		set_priv(initialPriv);	// restore initial privilege state
		string_printf(error, "open credential file %s: %s", credFile.c_str(),
				strerror(errno) );
		goto EXIT;
	}
	if	(	full_write(fd, (char *)credential, strlen(credential)) !=
			(ssize_t)strlen(credential)
		)
	{
		close(fd);
		set_priv(initialPriv);	// restore initial privilege state
		string_printf(error, "write credential file %s: %s", credFile.c_str(),
				strerror(errno) );
		goto EXIT;
	}
	close(fd);

	set_priv(initialPriv);	// restore initial privilege state

	// Update job ad with new credential location.
	ad->EvaluateAttrString(STORK_JOB_ATTR_EXEC_ENV, environment);
	if ( ! env.MergeFromV2Raw( environment.c_str(), &errorMsg ) ) {
		dprintf(D_ALWAYS, "%s failed to set user environment \"%s\":%s\n",
			jobId.fmt(), environment.c_str(), errorMsg.Value() );
		goto EXIT;
	}
    X509_USER_PROXY = "X509_USER_PROXY=";
    X509_USER_PROXY += credFile;
	if ( ! env.MergeFromV2Raw( X509_USER_PROXY.c_str(), &errorMsg ) ) {
		dprintf(D_ALWAYS, "%s failed to set user environment \"%s\":%s\n",
			jobId.fmt(), X509_USER_PROXY.c_str(), errorMsg.Value() );
		goto EXIT;
	}
    if (! env.getDelimitedStringV2Raw(&delimitedString,  &errorMsg ) ) {
		dprintf(D_ALWAYS, "%s failed to get user environment :%s\n",
			jobId.fmt(), errorMsg.Value() );
		goto EXIT;
    }
    exec_env = delimitedString.Value();
	ad->InsertAttr(STORK_JOB_ATTR_EXEC_ENV, exec_env);

	rtnStatus = true;
EXIT:
	return rtnStatus;
}

// Get job user, owner.
bool
Stork::getJobPrivilegeInfo(classad::ClassAd* ad, Stream *stream, string& error)
	const
{
	string owner;			// remote user mapped to local job owner.
	string ownerVerify;		// verify owner in ClassAd (paranoia)
	string domain;			// NT domain
	StorkJobId jobId(*ad);	// get job id

	owner = ((ReliSock*)stream)->getOwner();
	ad->InsertAttr(STORK_JOB_ATTR_OWNER, owner);
	if	(	// paranoid owner verification.
			(! ad->EvaluateAttrString(STORK_JOB_ATTR_OWNER, ownerVerify) ) ||
			owner.empty() ||
			ownerVerify.empty() ||
			(owner != ownerVerify)
		)
	{
		string_printf(error, "%s no %s", jobId.fmt(), STORK_JOB_ATTR_OWNER);
		return false;
	}

#if 0
	// TODO: Check WIN32 DOMAIN security code before use!
	if	(	! ad->EvaluateAttrString(STORK_JOB_ATTR_OWNER, owner) ||
			owner.empty()
		)
	{
		setErrorAndLog("job " FMTJOBID ": unable to get domain\n", jobId);
		goto EXIT;
	}
#endif

	if (! init_user_ids(owner.c_str(), domain.c_str() ) ) {
		string_printf(error, "%s init_user_ids(\"%s\", \"%s\") failed",
				jobId.fmt(), owner.c_str(), domain.c_str() );
		return false;
	}

	if( is_root() && (! can_switch_ids() ) ) {
		string_printf(error, "%s can_switch_ids failed", jobId.fmt() );
		return false;
	}

	return true;
}

#if 0
/*	Check user privilege file access */
/* FIXME: this implementation is broken:
   1) The access() system call uses the _real_ uid, not the effective uid,
   making the requested privilge state meaningless.
   2). Write access checks via mode W_OK only check if the file exists, and is
   writeable.  No check is made if the file does not exist, but the directory
   is writeable.
   */
bool
Stork::userFileAccess(	const classad::ClassAd* ad,
						const std::string path,
						const int mode,
						const priv_state) const
{
	return (access( path.c_str(), mode) == 0);
}
#endif

// Query the job queue using a dynamic compiled constraint expression
bool
Stork::query(
			jobList& jobAds,
			const std::string& viewName,
			const std::string constraint,
			int limit
		)
{
#if 0
dprintf(D_ALWAYS, "DEBUG query() view=%s constraint=\"%s\" limit=%d\n",
viewName.c_str(), constraint.c_str(), limit);
#endif
	classad::ExprTree* expr = NULL;
	int count = 0;

	if (limit == 0) return true;	// return if no ads are requested.
	if (limit < 0) {
		dprintf(D_ALWAYS, "error: negative %s query limit: %d\n",
				viewName.c_str(), limit );
		return false;
	}

	if ( ! constraint.empty() ) {
		//constraintTree = m_Parser.ParseExpression( constraint, true );
		if	( ! m_Parser.ParseExpression( constraint, expr, true) ) {
			dprintf(D_ALWAYS, "error parsing query constraint \"%s\": %s\n",
					constraint.c_str(), classad::CondorErrMsg.c_str() );
			return false;
		}
	}

	if ( ! m_Query.Query( viewName, expr) ) {
		dprintf(D_ALWAYS, "view %s query \"%s\" failed: %s\n",
			viewName.c_str(), constraint.c_str(),
			classad::CondorErrMsg.c_str() );
		if (! expr) delete expr;
		return false;
	}

	// Query the collection, and append ads to end of return list.
	classad::LocalCollectionQuery::const_iterator iter;
	for ( iter = m_Query.begin(); iter != m_Query.end(); iter++ ) {
		string key = *iter;
		classad::ClassAd* ad = m_JobQ.GetClassAd(key);
		if (! ad) {
			dprintf(D_ALWAYS, "query failed: null ad for key %s: %s\n",
					key.c_str(), classad::CondorErrMsg.c_str() );
			if (! expr) delete expr;
			return false;
		}
		jobAds.push_back(ad);
		count++;
		if (count >= limit) {
			if (! expr) delete expr;
			return true;
		}
	}

	if (! expr) delete expr;
	return true;
}

// Reschedule idle jobs.
void
Stork::rescheduleJobs(void)
{
	dprintf(D_FULLDEBUG, "scheduling idle jobs\n");
	time_t now = time(NULL);
	time_t delta = now - m_lastRescheduleTime;

	if ( delta < m_MinRescheduleInterval ) {
		dprintf(D_FULLDEBUG,
				"skip this scheduling cycle: "
				"only %lu seconds from last cycle\n", delta);
		return;
	}

	int currentRunningJobs = m_Pid2JobIdMap.size();
	if (currentRunningJobs >= m_MaxRunningJobs) {
		dprintf(D_FULLDEBUG,
				"skip this scheduling cycle: job limit=%d, current=%d\n",
				m_MaxRunningJobs, currentRunningJobs);
		return;
	}

	int currentIdleJobs = m_IdleJobsView->Size();
	if (currentIdleJobs == 0) {
		dprintf(D_FULLDEBUG,
				"skip this scheduling cycle: no idle jobs to schedule\n");
		return;
	}

	// Go figger number of jobs to start.
	int runJobDelta = m_MaxRunningJobs - currentRunningJobs;
	int jobsToStart =
		MIN( m_JobStartCount, MIN( currentIdleJobs, runJobDelta) );
	dprintf(D_FULLDEBUG,
	"job counts: idle=%d cycle limit=%d to max run limit=%d this cycle=%d\n",
	currentIdleJobs, m_JobStartCount, runJobDelta, jobsToStart);

	// Query idle jobs.
	jobList jobAds;
	string nullQuery;
	if (! query( jobAds, m_IdleJobsViewName, nullQuery, jobsToStart ) ) {
		dprintf(D_ALWAYS, "query %s error\n", m_IdleJobsViewName.c_str() );
		return;
	}

	for	(	jobList::const_iterator iter = jobAds.begin();
			iter != jobAds.end();
			iter++
		)
	{
		// Run the job.
        classad::ClassAd* ad = *iter;
		if ( ! runJob(ad) ) {
			// A system error occurred running the job
			StorkUserLog userLog(ad);
			ad->InsertAttr(STORK_JOB_ATTR_STATUS, STORK_JOB_STATUS_REMOVE);
			userLog.genericEvent("system error starting job");
			userLog.abortEvent();
			deleteJob(ad);
		}
	}

	dprintf(D_FULLDEBUG, "idle/total job count: %u/%d\n",
			m_IdleJobsView->Size(), m_RootJobsView->Size() );

	m_lastRescheduleTime = time(NULL);
	return;
}

// Run a job.
bool
Stork::runJob(classad::ClassAd* ad)
{
	string unparsed;
	m_Unparser.Unparse( unparsed, ad);
	dprintf(D_ALWAYS, "run job %s\n", unparsed.c_str() );

	// Get this job id.
	StorkJobId jobId(*ad);
	if ( jobId.error() ) {
		dprintf(D_ALWAYS, "error: runJob() jobId: %s\n", jobId.errorMsg() );
		return false;
	}

	// Get runtime module
	string module;
	ad->EvaluateAttrString(STORK_JOB_ATTR_MODULE, module);

	// Get runtime directory
	string run_dir;
	ad->EvaluateAttrString(STORK_JOB_ATTR_RUN_DIR, run_dir);

	// Create job runtime environment from user supplied environment, then
	// system supplied environment
	Env env;
	string environment;
	MyString errorMsg;
	ad->EvaluateAttrString(STORK_JOB_ATTR_ENVIRONMENT, environment);
	if ( ! env.MergeFromV2Raw( environment.c_str(), &errorMsg ) ) {
		dprintf(D_ALWAYS, "%s failed to set user environment \"%s\":%s\n",
			jobId.fmt(), environment.c_str(), errorMsg.Value() );
		return false;
	}
	ad->EvaluateAttrString(STORK_JOB_ATTR_EXEC_ENV, environment);
	if ( ! env.MergeFromV2Raw( environment.c_str(), &errorMsg ) ) {
		dprintf(D_ALWAYS, "%s failed to set system environment \"%s\":%s\n",
			jobId.fmt(), environment.c_str(), errorMsg.Value() );
		return false;
	}

	// Create job runtime arguments list.
	ArgList args;
	string exec_args;
	ad->EvaluateAttrString( STORK_JOB_ATTR_EXEC_ARGS, exec_args);
	if ( ! args.AppendArgsV2Raw(exec_args.c_str() ,&errorMsg) ) {
		dprintf(D_ALWAYS, "%s failed to read arguments \"%s\":%s\n",
				jobId.fmt(), exec_args.c_str(), errorMsg.Value()  );
		return false;
	}

	// Open file descriptors for child module.
	module_stdio_t module_stdio;
	if (!  open_module_stdio( ad, module_stdio) ) {
		return false;	// TODO return true for transient FS error?
	}

	int pid =
		daemonCore->Create_Process(
				module.c_str(),			// executable full path name
				args,					// executable arguments
				PRIV_USER_FINAL,		// privilege state
				m_JobReaperId,			// reaper_id
				FALSE,					// want_commanand_port
				&env,					// environment
				run_dir.c_str(),		// Current Working Directory
				FALSE,					// new process group
				NULL,					// list of socks to inherit
				module_stdio			// child stdio file descriptors
		 								// nice increment = 0
		 								// job_opt_mask = 0
										// fd_inherit_list[]    = NULL
		);

	if (pid == FALSE) {
		dprintf(D_ALWAYS, "runJob(): Create_process() %s error\n",
				jobId.fmt() );
		return false;
	} else {
		dprintf(D_ALWAYS, "start %s pid: %d\n", jobId.fmt(), pid);
	}

	// Save process pid.
	int2stringMap::const_iterator pos;
	pos = m_Pid2JobIdMap.find(pid);
	if (pos != m_Pid2JobIdMap.end() ) {
		dprintf(D_ALWAYS, "ERROR: pid %d for %s is not unique!\n",
				pid, jobId.fmt() );
		// TODO: now what?
		return false;
	}
	m_Pid2JobIdMap[ pid ] = jobId.fmt();

	// Update user log with execute event.
	string execute_host = daemonCore->InfoCommandSinfulString();
	ad->InsertAttr(STORK_JOB_ATTR_EXECUTE_HOST, execute_host);
	StorkUserLog userLog(ad);
	userLog.executeEvent();

	// Update job ad
	int num_attempts;
	ad->EvaluateAttrInt( STORK_JOB_ATTR_NUM_ATTEMPTS, num_attempts);
	{
		classad::ClassAd *ad = new classad::ClassAd( );
		ad->InsertAttr(STORK_JOB_ATTR_NUM_ATTEMPTS, ++num_attempts);
		ad->Insert(STORK_JOB_ATTR_START_TIME, classad::Literal::MakeAbsTime() );
		// TODO determine why changing STORK_JOB_ATTR_STATUS requires use of
		// ModifyClassAd().  Changing other attributes don't require modify.
		ad->InsertAttr(STORK_JOB_ATTR_STATUS, STORK_JOB_STATUS_RUN);
		updateJobAd(jobId.key() , ad);
	}

	return true;
}

/// Remove a job from job queue.
bool
Stork::deleteJob(classad::ClassAd* ad)
{
	StorkJobId jobId(*ad);

	// Append unparsed (text) job ad to history file.
	if ( param_boolean(STORK_JOB_HISTORY, STORK_JOB_HISTORY_DEFAULT) ) {
		string tmp;
		m_Unparser.Unparse(tmp, ad);

		int fd = openJobHistory();
		if (fd >= 0) {
			ssize_t nbytes = full_write(fd, (void*)tmp.c_str(), tmp.length() );
			if ( nbytes != (ssize_t)tmp.length() ) {
				dprintf(D_ALWAYS, "deleteJob(%s) write(%s): %s\n",
					jobId.fmt(), m_JobHistory.c_str(), strerror(errno) );
			}
			close(fd);
		} else {
			dprintf(D_ALWAYS, "deleteJob(%s) open(%s): %s\n",
					jobId.fmt(), m_JobHistory.c_str(), strerror(errno) );
		}
	}

	// Remove job from collection.
	if (! m_JobQ.RemoveClassAd( jobId.key() ) ) {
		// This is really bad.  Is the job queue now corrupt?
		dprintf(D_ALWAYS, "ERROR unable to remove job %s from job queue\n",
				jobId.fmt() );
	}

	return true;
}

/// Common job process reaper.
int
Stork::commonJobReaper(	const int pid,
						const int exit_status)
{

	// Traverse pid -> ClassAd key -> job ad
	int2stringMap::const_iterator pos;
	pos = m_Pid2JobIdMap.find(pid);
	if (pos == m_Pid2JobIdMap.end() ) {
		dprintf(D_ALWAYS, "ERROR: no job for terminated pid %d\n", pid);
		return FALSE;	// TODO: now what?
	}
	string key = pos->second;
	classad::ClassAd* ad = m_JobQ.GetClassAd(key);
	if (! ad ) {
		dprintf(D_ALWAYS, "ERROR: no job ad for terminated pid %d\n", pid);
		return FALSE;	// TODO: now what?
	}

	// TODO retrieve any new files, including core files, in rundir.
	destroyJobRunDir(ad);

	StorkJobId jobId(*ad);
	dprintf(D_ALWAYS, "%s pid %d exited with status %d\n",
			jobId.fmt(), pid, exit_status);

	// Update job ad
	ad->InsertAttr( STORK_JOB_ATTR_EXIT_STATUS, exit_status);
	ad->InsertAttr( STORK_JOB_ATTR_STATUS, STORK_JOB_STATUS_COMPLETE);

	// update user log
	StorkUserLog userLog(ad);
	userLog.terminateEvent();	// FIXME restart job if more retries left

	deleteJob( ad );
	return TRUE;
}

/// Transfer job process reaper.
int
Stork::transferJobReaper(	classad::ClassAd* ad)
{
	return TRUE;
}

/// Open standard I/O streams for a module
bool
Stork::open_module_stdio(	const classad::ClassAd* ad,
							module_stdio_t module_stdio)
{
	std::string path;
	StorkJobId jobId(*ad);
	priv_state initialPriv;

	module_stdio[ MODULE_STDIN_INDEX ] = -1;
	module_stdio[ MODULE_STDOUT_INDEX ] = -1;
	module_stdio[ MODULE_STDERR_INDEX ] = -1;

	string owner;
	if ( ! ad->EvaluateAttrString(STORK_JOB_ATTR_OWNER, owner)  ||
		owner.empty() )
	{
        dprintf(D_ALWAYS, "%s attribute %s missing\n",
				jobId.fmt(), STORK_JOB_ATTR_OWNER);
        return false;
    }
#ifdef WIN32
#error open_module_stdio() check for Windows not yet supported
#endif

	// Prepare to switch to euid
	if (! init_user_ids(owner.c_str(), NULL) ) {
		dprintf(D_ALWAYS, "error: %s open_module_stdio() unable to switch to "
				"user %s\n", jobId.fmt(), owner.c_str() );
		return false;
	}

	if ( ! ad->EvaluateAttrString(STORK_JOB_ATTR_INPUT, path) ) {
		path = NULL_FILE;
	}
	if ( path.empty() ) {
		dprintf(D_ALWAYS, "error: %s empty input file path\n",
			jobId.fmt() );
		return false;
	}
	initialPriv = set_user_priv();
	module_stdio[ MODULE_STDIN_INDEX ] =
		open( path.c_str(), O_RDONLY);
	set_priv( initialPriv );    // restore initial privilege state
	if ( module_stdio[ MODULE_STDIN_INDEX ] < 0 ) {
		dprintf(D_ALWAYS, "error:  %s open input file %s: %s\n",
				jobId.c_str(), path.c_str(), strerror(errno) );
		return false;
	}

	if ( ! ad->EvaluateAttrString(STORK_JOB_ATTR_OUTPUT, path) ) {
		path = NULL_FILE;
	}
	if ( path.empty() ) {
		dprintf(D_ALWAYS, "error: %s empty output file path\n",
				jobId.c_str() );
		return false;
	}
	initialPriv = set_user_priv();
	module_stdio[ MODULE_STDOUT_INDEX ] =
		open( path.c_str(), O_WRONLY|O_CREAT|O_APPEND, 0644);
	set_priv( initialPriv );    // restore initial privilege state
	if ( module_stdio[ MODULE_STDOUT_INDEX ] < 0 ) {
		dprintf(D_ALWAYS, "error: %s open output file %s: %s\n",
				jobId.fmt(), path.c_str(), strerror(errno) );
		return false;
	}

	if ( ! ad->EvaluateAttrString(STORK_JOB_ATTR_ERROR, path) ) {
		path = NULL_FILE;
	}
	if ( path.empty() ) {
		dprintf(D_ALWAYS, "error: %s empty error file path\n",
				jobId.fmt() );
		return false;
	}
	initialPriv = set_user_priv();
	module_stdio[ MODULE_STDERR_INDEX ] =
		open( path.c_str(), O_WRONLY|O_CREAT|O_APPEND, 0644);
	set_priv( initialPriv );    // restore initial privilege state
	if ( module_stdio[ MODULE_STDERR_INDEX ] < 0 ) {
		dprintf(D_ALWAYS, "error: %s open error file %s: %s\n",
				jobId.fmt(), path.c_str(), strerror(errno) );
		return false;
	}

	return true;
}


/// Close standard I/O streams for a module
void
Stork::close_module_stdio( module_stdio_t module_stdio)
{
	if ( module_stdio[ MODULE_STDIN_INDEX ] >= 0) {
		close( module_stdio[ MODULE_STDIN_INDEX ] );
		module_stdio[ MODULE_STDIN_INDEX ] = -1;
	}

	if ( module_stdio[ MODULE_STDOUT_INDEX ] >= 0) {
		close( module_stdio[ MODULE_STDOUT_INDEX ] );
		module_stdio[ MODULE_STDOUT_INDEX ] = -1;
	}

	if ( module_stdio[ MODULE_STDERR_INDEX ] >= 0) {
		close( module_stdio[ MODULE_STDERR_INDEX ] );
		module_stdio[ MODULE_STDERR_INDEX ] = -1;
	}

	return;
}
