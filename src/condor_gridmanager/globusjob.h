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

#ifndef GLOBUSJOB_H
#define GLOBUSJOB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "MyString.h"
#include "globus_utils.h"
#include "classad_hashtable.h"

#include "proxymanager.h"
#include "basejob.h"
#include "globusresource.h"
#include "gahp-client.h"

#define JM_COMMIT_TIMEOUT	600

class GlobusResource;

/////////////////////from gridmanager.h
class GlobusJob;
extern HashTable <HashKey, GlobusJob *> JobsByContact;

extern char *gassServerUrl;
extern char *gramCallbackContact;

// This is needed for WriteGlobusSubmitFailedEventToUserLog() in gridmanager.C
extern GahpClient GahpMain;

void rehashJobContact( GlobusJob *job, const char *old_contact,
					   const char *new_contact );
char *globusJobId( const char *contact );
void gramCallbackHandler( void *user_arg, char *job_contact, int state,
						  int errorcode );
bool InitializeGahp( const char *proxy_filename );

void GlobusJobInit();
void GlobusJobReconfig();
bool GlobusJobAdMatch( const ClassAd *jobad );
bool GlobusJobAdMustExpand( const ClassAd *jobad );
///////////////////////////////////////

class GlobusJob : public BaseJob
{
 public:

	GlobusJob( ClassAd *classad );

	~GlobusJob();

	void Reconfig();
	int doEvaluateState();
	void NotifyResourceDown();
	void NotifyResourceUp();
	void UpdateGlobusState( int new_state, int new_error_code );
	void GramCallback( int new_state, int new_error_code );
	bool GetCallbacks();
	void ClearCallbacks();
	BaseResource *GetResource();

	static int probeInterval;
	static int submitInterval;
	static int restartInterval;
	static int gahpCallTimeout;
	static int maxConnectFailures;
	static int outputWaitGrowthTimeout;

	static void setProbeInterval( int new_interval )
		{ probeInterval = new_interval; }
	static void setSubmitInterval( int new_interval )
		{ submitInterval = new_interval; }
	static void setRestartInterval( int new_interval )
		{ restartInterval = new_interval; }
	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }
	static void setConnectFailureRetry( int count )
		{ maxConnectFailures = count; }

	// New variables
	bool resourceDown;
	bool resourceStateKnown;
	int gmState;
	int globusState;
	int globusStateErrorCode;
	int globusStateBeforeFailure;
	int callbackGlobusState;
	int callbackGlobusStateErrorCode;
	bool jmUnreachable;
	GlobusResource *myResource;
	time_t lastProbeTime;
	bool probeNow;
	time_t enteredCurrentGmState;
	time_t enteredCurrentGlobusState;
	time_t lastSubmitAttempt;
	int numSubmitAttempts;
	int submitFailureCode;
	int lastRestartReason;
	time_t lastRestartAttempt;
	int numRestartAttempts;
	int numRestartAttemptsThisSubmit;
	time_t jmProxyExpireTime;
	time_t outputWaitLastGrowth;
	int outputWaitOutputSize;
	int outputWaitErrorSize;
	// HACK!
	bool retryStdioSize;
	char *resourceManagerString;
	bool useGridJobMonitor;

	bool gahp_proxy_id_set;
	Proxy *myProxy;
	GahpClient gahp;

	MyString *buildSubmitRSL();
	MyString *buildRestartRSL();
	MyString *buildStdioUpdateRSL();
	bool GetOutputSize( int& output, int& error );
	void DeleteOutput();

	char *jobContact;
		// If we're in the middle of a globus call that requires an RSL,
		// the RSL is stored here (so that we don't have to reconstruct the
		// RSL every time we test the call for completion). It should be
		// freed and reset to NULL once the call completes.
	MyString *RSL;
	MyString errorString;
	char *localOutput;
	char *localError;
	bool streamOutput;
	bool streamError;
	bool stageOutput;
	bool stageError;
	int globusError;

	int jmVersion;
	bool restartingJM;
	time_t restartWhen;

	int numGlobusSubmits;

 protected:
	bool callbackRegistered;
	int connect_failure_counter;
	bool AllowTransition( int new_state, int old_state );

	bool FailureIsRestartable( int error_code );
	bool FailureNeedsCommit( int error_code );
	bool JmShouldSleep();
};

bool WriteGlobusSubmitEventToUserLog( ClassAd *job_ad );
bool WriteGlobusSubmitFailedEventToUserLog( ClassAd *job_ad,
											int failure_code );
bool WriteGlobusResourceUpEventToUserLog( ClassAd *job_ad );
bool WriteGlobusResourceDownEventToUserLog( ClassAd *job_ad );

#endif

