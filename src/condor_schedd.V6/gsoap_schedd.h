#import "gsoap_schedd_types.h"

//gsoap condorSchedd service name: condorSchedd
//gsoap condorSchedd service encoding: encoding-style

int condorSchedd__beginTransaction(xsd__int duration, 
                                   struct condorSchedd__TransactionAndStatus & result);

int condorSchedd__commitTransaction(struct condorSchedd__Transaction transaction, 
                                    struct condorCore__Status & result);

int condorSchedd__abortTransaction(struct condorSchedd__Transaction transaction,
                                   struct condorCore__Status & result);

int condorSchedd__extendTransaction(struct condorSchedd__Transaction transaction,
                                    xsd__int duration,
                                    struct condorSchedd__TransactionAndStatus & result);

int condorSchedd__newCluster(struct condorSchedd__Transaction transaction,
                             struct condorSchedd__IntAndStatus & result);

int condorSchedd__removeCluster(struct condorSchedd__Transaction transaction,
                                xsd__int clusterId,
                                xsd__string reason,
                                struct condorCore__Status & result);

int condorSchedd__newJob(struct condorSchedd__Transaction transaction,
                         xsd__int clusterId,
                         struct condorSchedd__IntAndStatus & result);

int condorSchedd__removeJob(struct condorSchedd__Transaction transaction,
                            xsd__int clusterId,
                            xsd__int jobId,
                            xsd__string reason,
                            xsd__boolean force_removal,
                            struct condorCore__Status & result);

int condorSchedd__holdJob(struct condorSchedd__Transaction transaction,
                          xsd__int clusterId,
                          xsd__int jobId,
                          xsd__string reason,
                          xsd__boolean email_user,
                          xsd__boolean email_admin,
                          xsd__boolean system_hold,
                          struct condorCore__Status & result);

int condorSchedd__releaseJob(struct condorSchedd__Transaction transaction,
                             xsd__int clusterId,
                             xsd__int jobId,
                             xsd__string reason,
                             xsd__boolean email_user,
                             xsd__boolean email_admin,
                             struct condorCore__Status & result);

int condorSchedd__submit(struct condorSchedd__Transaction transaction,
                         xsd__int clusterId,
                         xsd__int jobId,
                         struct condorCore__ClassAdStruct * jobAd,
                         struct condorSchedd__RequirementsAndStatus & result);

int condorSchedd__getJobAds(struct condorSchedd__Transaction transaction,
                            xsd__string constraint,
                            struct condorCore__ClassAdStructArrayAndStatus & result);

int condorSchedd__getJobAd(struct condorSchedd__Transaction transaction,
                           xsd__int clusterId,
                           xsd__int jobId,
                           struct condorCore__ClassAdStructAndStatus & result);


int condorSchedd__sendFile(struct condorSchedd__Transaction transaction,
                           xsd__int clusterId,
                           xsd__int jobId,
                           xsd__string name,
                           xsd__int offset,
                           struct xsd__base64Binary * data,
                           struct condorCore__Status & result);

int condorSchedd__discoverJobRequirements(struct condorCore__ClassAdStruct * jobAd,
                                          struct condorSchedd__RequirementsAndStatus & result);

int condorSchedd__discoverDagRequirements(xsd__string dag,
                                           struct condorSchedd__RequirementsAndStatus & result);

