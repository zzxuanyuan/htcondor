
#import "../condor_daemon_core.V6/gsoap_daemon_core.h"

//gsoap condorSchedd service namespace: urn:condor-schedd
//gsoap condorSchedd service name: condorSchedd

int condorSchedd__beginTransaction(xsd__int duration, 
			xsd__int & transactionId);

int condorSchedd__commitTransaction(xsd__int transactionId, 
			xsd__int & result );

int condorSchedd__abortTransaction(xsd__int transactionId,
			xsd__int & result );

int condorSchedd__extendTransaction(xsd__int transactionId,
			xsd__int duration,
			xsd__int & result );

int condorSchedd__newCluster(xsd__int transactionId,
			xsd__int & result);

int condorSchedd__removeCluster(xsd__int transactionId,
			xsd__int clusterId, xsd__string reason,
			xsd__int & result);

int condorSchedd__newJob(xsd__int transactionId,
			xsd__int clusterId, xsd__int & result);

int condorSchedd__removeJob(xsd__int transactionId,
			xsd__int clusterId, xsd__int jobId, xsd__string reason, xsd__boolean force_removal,
			xsd__int & result);

int condorSchedd__holdJob(xsd__int transactionId,
			xsd__int clusterId, xsd__int jobId, xsd__string reason,
			xsd__boolean email_user, xsd__boolean email_admin, xsd__boolean system_hold,
			xsd__int & result);

int condorSchedd__releaseJob(xsd__int transactionId,
			xsd__int clusterId, xsd__int jobId, xsd__string reason,
			xsd__boolean email_user, xsd__boolean email_admin,
			xsd__int & result);

int condorSchedd__submit(xsd__int transactionId,
				xsd__int clusterId, xsd__int jobId,
				struct ClassAdStruct * jobAd,
				xsd__int & result);

int condorSchedd__getJobAds(xsd__int transactionId,
				xsd__string constraint,
				struct ClassAdStructArray & result );

int condorSchedd__getJobAd(xsd__int transactionId,
				xsd__int clusterId, xsd__int jobId,
				struct ClassAdStruct & result );

