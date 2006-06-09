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

// Stork user logging interfaces.

#include "StorkUserLog.h"
#include "condor_debug.h"
#include "stork_job_ad.h"
using std::string;


StorkUserLog::StorkUserLog( void )
{
	initialize();
}

StorkUserLog::StorkUserLog( const classad::ClassAd* ad)
{
	initialize();
	assign(ad);
}

StorkUserLog::~StorkUserLog( void )
{
	return;
}

void
StorkUserLog::assign(const classad::ClassAd* ad)
{
	if (! ad) {
		dprintf(D_ALWAYS, "StorkUserLog called with null jobAd\n");
		return;
	}

	initialize();
	_ad = ad;

	// Determine if this job has a user log.
	if	(	_ad->EvaluateAttrString(STORK_JOB_ATTR_LOG, _log) &&
			! _log.empty() ) {
		_enable = true;
	} else {
		_enable = false;
		return;  // FIMXE
	}

	// Read jobId from job ad.
	_jobId = *_ad;
	if ( _jobId.error() ) {
		_error = _jobId.error();
		_errorMsg = _jobId.errorMsg();
		return;
	}
	_jobId.getTuple(&_cluster, &_proc, &_subproc);

	// Job ad must have an owner attribute.
	if	(	! _ad->EvaluateAttrString(STORK_JOB_ATTR_OWNER, _owner) ||
			_owner.empty() ) {
		dprintf(D_ALWAYS, "StorkUserLog() %s has no owner\n", _jobId.fmt() );
		return;
	}

	// Look for XML logging format
	_ad->EvaluateAttrBool(STORK_JOB_ATTR_LOGXML, _logXML);

	_userLog.setUseXML( _logXML );

	if ( ! _userLog.initialize(
				_owner.c_str(),
				NULL,					// domain TODO: fix for WIN32
				_log.c_str(),
				_cluster,
				_proc,
				_subproc)
			)
	{
		dprintf(D_ALWAYS, "StorkUserLog() error initializing for %s\n",
				_jobId.fmt() );
		return;
	}

	return;
}

StorkUserLog&
StorkUserLog::operator=(const classad::ClassAd* ad)
{
	assign( ad );
	return *this;
}

void
StorkUserLog::initialize(void)
{
	_ad = NULL;
	_log.clear();
	_error = true;
	_errorMsg.clear();
	_owner.clear();
	_enable = false;
	_logXML = false;
	_cluster = 0;
	_proc = 0;
	_subproc = 0;

	return;
}

bool
StorkUserLog::submitEvent(void)
{
	SubmitEvent event;
	string logNotes;

	if ( ! _enable ) {
		return true;
	}

	if	(	_ad->EvaluateAttrString(STORK_JOB_ATTR_LOGNOTES, logNotes) &&
			!logNotes.empty()
		)
	{
		// Ugh.  The SubmitEvent destructor will
		// delete[] submitEventLogNotes, so "new" one up here.
		event.submitEventLogNotes = strnewp( logNotes.c_str() );
	}

	string submit_host;
	_ad->EvaluateAttrString(STORK_JOB_ATTR_SUBMIT_HOST, submit_host);
	strncpy(event.submitHost, submit_host.c_str(),
			sizeof(event.submitHost) - 1 );
	event.submitHost[ sizeof(event.submitHost) - 1 ] = '\0';

	if ( ! _userLog.writeEvent(&event) ) {
		dprintf(D_ALWAYS, "error: failed to log %s submit event.\n",
				_jobId.fmt() );
		return false;
	}

	return true;
}

bool
StorkUserLog::executeEvent(void)
{
	ExecuteEvent event;

	if ( ! _enable ) {
		return true;
	}

	string execute_host;
	if	(	! _ad->EvaluateAttrString(STORK_JOB_ATTR_EXECUTE_HOST, execute_host)
			|| execute_host.empty()
		)
	{
		dprintf(D_ALWAYS, "error: %s ad has no %s\n", 
				_jobId.fmt(), STORK_JOB_ATTR_EXECUTE_HOST );
		return false;
	}

	strncpy(event.executeHost, execute_host.c_str(),
			sizeof(event.executeHost) - 1 );
	event.executeHost[ sizeof(event.executeHost) - 1 ] = '\0';

	if ( ! _userLog.writeEvent(&event) ) {
		dprintf(D_ALWAYS, "error: failed to log %s execute event.\n",
				_jobId.fmt() );
		return false;
	}

	return true;
}

bool
StorkUserLog::genericEvent( std::string text )
{
	GenericEvent event;

	event.setInfoText( text.c_str() );
	if ( ! _userLog.writeEvent(&event) ) {
		dprintf(D_ALWAYS, "error: failed to log %s generic event.\n",
				_jobId.fmt() );
		return false;
	}

	return true;
}

bool
StorkUserLog::terminateEvent( void )
{
	JobTerminatedEvent event;

	// Parse the exit status.
	int exit_status;
	if	(	! _ad->EvaluateAttrInt(STORK_JOB_ATTR_EXIT_STATUS, exit_status) ) {
		dprintf(D_ALWAYS, "error: %s ad has no %s\n", 
				_jobId.fmt(), STORK_JOB_ATTR_EXIT_STATUS );
		return false;
	}
	if ( WIFEXITED( exit_status) ) {
		// Stork requires all successful jobs to exit with status 0
		event.normal = (exit_status == 0) ? true : false;
		event.returnValue = WEXITSTATUS( exit_status );
	} else if ( WIFSIGNALED( exit_status) ) {
		event.normal = false;
		event.signalNumber = WTERMSIG( exit_status );
	} else {
		dprintf(D_ALWAYS, "job %s exit_status %d unknown\n",
				_jobId.fmt(), exit_status );
		return false;
	}

	if ( ! _userLog.writeEvent(&event) ) {
		dprintf(D_ALWAYS, "error: failed to log %s terminate event.\n",
				_jobId.fmt() );
		return false;
	}

	return true;
}

bool
StorkUserLog::abortEvent( void )
{
	JobAbortedEvent event;

	if ( ! _userLog.writeEvent(&event) ) {
		dprintf(D_ALWAYS, "error: failed to log %s aborted event.\n",
				_jobId.fmt() );
		return false;
	}

	return true;
}

