
#ifndef ORACLEJOB_H
#define ORACLEJOB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "MyString.h"
#include "classad_hashtable.h"

#include "basejob.h"

#define JOB_COMMIT_TIMEOUT	600

void OracleJobInit();
void OracleJobReconfig();
bool OracleJobAdMatch( const ClassAd *jobad );
bool OracleJobAdMustExpand( const ClassAd *jobad );

class OracleJob : public BaseJob
{
 public:

	OracleJob( ClassAd *classad );

	~OracleJob();

	void Reconfig();
	int doEvaluateState();
	void UpdateGlobusState( int new_state, int new_error_code );
	BaseResource *GetResource();

	static int probeInterval;
	static int submitInterval;

	static void setProbeInterval( int new_interval )
		{ probeInterval = new_interval; }
	static void setSubmitInterval( int new_interval )
		{ submitInterval = new_interval; }

	int gmState;
//	OracleResource *myResource;
	time_t lastProbeTime;
	bool probeNow;
	time_t enteredCurrentGmState;
	time_t lastSubmitAttempt;
	int numSubmitAttempts;

	MyString errorString;
	char *resourceManagerString;

	char *remoteJobId;

	char *run_java(char **args);
	int do_submit();
	int do_commit();
	int do_status();
	int do_remove();

 protected:
};


#endif

