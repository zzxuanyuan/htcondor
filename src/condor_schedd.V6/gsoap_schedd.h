
#import "../condor_daemon_core.V6/gsoap_daemon_core.h"

//gsoap condorSchedd service namespace: urn:condor-schedd
//gsoap condorSchedd service name: condorSchedd

typedef long long xsd__long;

int condorSchedd__beginTransaction(int duration, 
			xsd__long & transactionId);

int condorSchedd__commitTransaction(xsd__long transactionId, 
			int & result );

int condorSchedd__abortTransaction(xsd__long transactionId,
			int & result );

int condorSchedd__extendTransaction(xsd__long transactionId,
			int duration,
			int & result );

int condorSchedd__newCluster(xsd__long transactionId,
			int & result);

int condorSchedd__removeCluster(xsd__long transactionId,
			int clusterId, char* reason,
			int & result);

int condorSchedd__newJob(xsd__long transactionId,
			int clusterId, int & result);

int condorSchedd__removeJob(xsd__long transactionId,
			int clusterId, int jobId, char* reason, bool force_removal,
			int & result);

int condorSchedd__holdJob(xsd__long transactionId,
			int clusterId, int jobId, char* reason,
			bool email_user, bool email_admin, bool system_hold,
			int & result);

int condorSchedd__releaseJob(xsd__long transactionId,
			int clusterId, int jobId, char* reason,
			bool email_user, bool email_admin,
			int & result);

int condorSchedd__submit(xsd__long transactionId,
				int clusterId, int jobId,
				struct ClassAdStruct * jobAd,
				int & result);

int condorSchedd__getJobAds(xsd__long transactionId,
				char *constraint,
				struct ClassAdStructArray & result );

int condorSchedd__getJobAd(xsd__long transactionId,
				int clusterId, int jobId,
				struct ClassAdStruct & result );

