
#import "../condor_daemon_core.V6/gsoap_daemon_core.h"

//gsoap condorSchedd service namespace: urn:condor-schedd
//gsoap condorSchedd service name: condorSchedd


int condorSchedd__beginTransaction(xsd__int duration, 
                                   struct condorCore__TransactionAndStatus & result);

int condorSchedd__commitTransaction(xsd__int transactionId, 
                                    struct condorCore__Status & result);

int condorSchedd__abortTransaction(xsd__int transactionId,
                                   struct condorCore__Status & result);

int condorSchedd__extendTransaction(xsd__int transactionId,
                                    xsd__int duration,
                                    struct condorCore__Status & result);

int condorSchedd__newCluster(xsd__int transactionId,
                             struct condorCore__IntAndStatus & result);

int condorSchedd__removeCluster(xsd__int transactionId,
                                xsd__int clusterId,
                                xsd__string reason,
                                struct condorCore__Status & result);

int condorSchedd__newJob(xsd__int transactionId,
                         xsd__int clusterId,
                         struct condorCore__IntAndStatus & result);

int condorSchedd__removeJob(xsd__int transactionId,
                            xsd__int clusterId,
                            xsd__int jobId,
                            xsd__string reason,
                            xsd__boolean force_removal,
                            struct condorCore__Status & result);

int condorSchedd__holdJob(xsd__int transactionId,
                          xsd__int clusterId,
                          xsd__int jobId,
                          xsd__string reason,
                          xsd__boolean email_user,
                          xsd__boolean email_admin,
                          xsd__boolean system_hold,
                          struct condorCore__Status & result);

int condorSchedd__releaseJob(xsd__int transactionId,
                             xsd__int clusterId,
                             xsd__int jobId,
                             xsd__string reason,
                             xsd__boolean email_user,
                             xsd__boolean email_admin,
                             struct condorCore__Status & result);

int condorSchedd__submit(xsd__int transactionId,
                         xsd__int clusterId,
                         xsd__int jobId,
                         struct condorCore__ClassAdStruct * jobAd,
                         struct condorCore__RequirementsAndStatus & result);

int condorSchedd__getJobAds(xsd__int transactionId,
                            xsd__string constraint,
                            struct condorCore__ClassAdStructArrayAndStatus & result);

int condorSchedd__getJobAd(xsd__int transactionId,
                           xsd__int clusterId,
                           xsd__int jobId,
                           struct condorCore__ClassAdStructAndStatus & result);

int condorSchedd__sendFile(xsd__int transactionId,
                           xsd__string name,
                           xsd__int offset,
                           struct xsd__base64Binary * data,
                           struct condorCore__Status & result);

int condorSchedd__discoverRequirements(struct condorCore__ClassAdStruct * jobAd,
                                       struct condorCore__RequirementsAndStatus & result);
