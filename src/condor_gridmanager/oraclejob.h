
#ifndef ORACLEJOB_H
#define ORACLEJOB_H

#if defined(ORACLE_UNIVERSE)

#include "condor_common.h"
#include "condor_classad.h"
#include "MyString.h"
#include "classad_hashtable.h"

#include "oci.h"

#include "basejob.h"
#include "oracleresource.h"

#define JOB_COMMIT_TIMEOUT	600

class OciSession;

void OracleJobInit();
void OracleJobReconfig();
bool OracleJobAdMatch( const ClassAd *jobad );
bool OracleJobAdMustExpand( const ClassAd *jobad );
BaseJob *OracleJobCreate( ClassAd *jobad );

extern OCIEnv *GlobalOciEnvHndl;
extern OCIError *GlobalOciErrHndl;

void print_error( sword status, OCIError *error_handle );

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
	OciSession *ociSession;
	time_t lastProbeTime;
	bool probeNow;
	time_t enteredCurrentGmState;
	time_t lastSubmitAttempt;
	int numSubmitAttempts;

	MyString errorString;
	char *resourceManagerString;
	char *dbName;
	char *dbUsername;
	char *dbPassword;

	char *remoteJobId;
	bool jobRunPhase;

	OCIError *ociErrorHndl;

	char *doSubmit1();
	int doSubmit2();
	int doSubmit3();
	int doCommit();
	int doStatus( bool &queued, bool &active, bool &broken,
				  int &num_failures );
	int doRemove();

 protected:
};

#endif // if defined(ORACLE_UNIVERSE)

#endif

