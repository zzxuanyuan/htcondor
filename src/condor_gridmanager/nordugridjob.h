
#ifndef NORDUGRIDJOB_H
#define NORDUGRIDJOB_H

#if defined(NORDUGRID_UNIVERSE)

#include "condor_common.h"
#include "condor_classad.h"
#include "MyString.h"
#include "classad_hashtable.h"

#include "ftp_lite.h"

#include "basejob.h"
#include "nordugridresource.h"

void NordugridJobInit();
void NordugridJobReconfig();
bool NordugridJobAdMatch( const ClassAd *jobad );
bool NordugridJobAdMustExpand( const ClassAd *jobad );
BaseJob *NordugridJobCreate( ClassAd *jobad );

class NordugridResource;

class NordugridJob : public BaseJob
{
 public:

	NordugridJob( ClassAd *classad );

	~NordugridJob();

	void Reconfig();
	int doEvaluateState();
	BaseResource *GetResource();

	static int probeInterval;
	static int submitInterval;

	static void setProbeInterval( int new_interval )
		{ probeInterval = new_interval; }
	static void setSubmitInterval( int new_interval )
		{ submitInterval = new_interval; }

	int gmState;
	time_t lastProbeTime;
	bool probeNow;
	time_t enteredCurrentGmState;
	time_t lastSubmitAttempt;
	int numSubmitAttempts;

	MyString errorString;
	char *resourceManagerString;

	char *remoteJobId;

	ftp_lite_server *ftp_srvr;
	NordugridResource *myResource;

		// Used by doStageIn() and doStageOut()
	StringList *stage_list;

	int doSubmit( char *&job_id );
	int doStatus( int &new_status );
	int doRemove();
	int doStageIn();
	int doStageOut();

	MyString *buildSubmitRSL();

 protected:
};

#endif // if defined(NORDUGRID_UNIVERSE)

#endif

