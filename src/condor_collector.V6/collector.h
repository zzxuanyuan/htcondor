#if !defined(CONDOR_COLLECTOR)
#define CONDOR_COLLECTOR

#include "condor_classad.h"
#include "condor_commands.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

class CollectorDaemon : public Service {
public:
	CollectorDaemon( );
	~CollectorDaemon( );

		// daemon-core handlers
	virtual void Init( );				// main_init
	virtual void Config( );				// main_config
	virtual void Exit( );				// main_shutdown_fast
	virtual void Shutdown( );			// main_shutdown_graceful

		// command handlers
	int receive_query( int, Stream* );	// command: process query
	int receive_modify( int, Stream* );	// command: process an update/modify
	int sendCollectorAd( );				// timer  : send update to wisconsin
	int removeExpiredAds( );			// timer  : remove stale ads

	void checkMaster( ClassAd* );		// to check for fallen masters
protected:
	ClassAdCollection	publicColl;
	ClassAdCollection	privateColl;

	int					publicPartitionID;
	int					privatePartitionID;

	int 				clientTimeout;
	int 				queryTimeout;
	int					collectorUpdateInterval;
	int					masterCheckInterval;
	int					classadLifetime;

	ClassAd				expireQuery;

	int					updateTimerID;
	int					expiredAdsID;

	char 				*condorDevelopersCollector;
	char 				*condorDevelopers;
	char 				*collectorName;
	char				*condorAdmin;

	SafeSock			updateSock;
	Source				src;
};

#endif
