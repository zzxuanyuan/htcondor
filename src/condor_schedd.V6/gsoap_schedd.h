#import "gsoap_schedd_types.h"

//gsoap condorSchedd service name: condorSchedd
//gsoap condorSchedd service encoding: encoding-style

struct condorSchedd__StatusResponse {
  struct condorCore__Status response;
};

struct condorSchedd__TransactionAndStatusResponse {
  struct condorSchedd__TransactionAndStatus response;
};

struct condorSchedd__IntAndStatusResponse {
  struct condorSchedd__IntAndStatus response;
};

struct condorSchedd__RequirementsAndStatusResponse {
  struct condorSchedd__RequirementsAndStatus response;
};

struct condorSchedd__ClassAdStructArrayAndStatusResponse {
  struct condorCore__ClassAdStructArrayAndStatus response;
};

struct condorSchedd__ClassAdStructAndStatusResponse {
  struct condorCore__ClassAdStructAndStatus response;
};

int condorSchedd__beginTransaction(xsd__int duration,
                                   struct condorSchedd__TransactionAndStatusResponse & result);

int condorSchedd__commitTransaction(struct condorSchedd__Transaction transaction,
                                    struct condorSchedd__StatusResponse & result);

int condorSchedd__abortTransaction(struct condorSchedd__Transaction transaction,
                                   struct condorSchedd__StatusResponse & result);

int condorSchedd__extendTransaction(struct condorSchedd__Transaction transaction,
                                    xsd__int duration,
                                    struct condorSchedd__TransactionAndStatusResponse & result);

int condorSchedd__newCluster(struct condorSchedd__Transaction transaction,
                             struct condorSchedd__IntAndStatusResponse & result);

int condorSchedd__removeCluster(struct condorSchedd__Transaction transaction,
                                xsd__int clusterId,
                                xsd__string reason,
                                struct condorSchedd__StatusResponse & result);

int condorSchedd__newJob(struct condorSchedd__Transaction transaction,
                         xsd__int clusterId,
                         struct condorSchedd__IntAndStatusResponse & result);

int condorSchedd__removeJob(struct condorSchedd__Transaction transaction,
                            xsd__int clusterId,
                            xsd__int jobId,
                            xsd__string reason,
                            xsd__boolean force_removal,
                            struct condorSchedd__StatusResponse & result);

int condorSchedd__holdJob(struct condorSchedd__Transaction transaction,
                          xsd__int clusterId,
                          xsd__int jobId,
                          xsd__string reason,
                          xsd__boolean email_user,
                          xsd__boolean email_admin,
                          xsd__boolean system_hold,
                          struct condorSchedd__StatusResponse & result);

int condorSchedd__releaseJob(struct condorSchedd__Transaction transaction,
                             xsd__int clusterId,
                             xsd__int jobId,
                             xsd__string reason,
                             xsd__boolean email_user,
                             xsd__boolean email_admin,
                             struct condorSchedd__StatusResponse & result);

int condorSchedd__submit(struct condorSchedd__Transaction transaction,
                         xsd__int clusterId,
                         xsd__int jobId,
                         struct condorCore__ClassAdStruct * jobAd,
                         struct condorSchedd__RequirementsAndStatusResponse & result);

int condorSchedd__getJobAds(struct condorSchedd__Transaction transaction,
                            xsd__string constraint,
                            struct condorSchedd__ClassAdStructArrayAndStatusResponse & result);

int condorSchedd__getJobAd(struct condorSchedd__Transaction transaction,
                           xsd__int clusterId,
                           xsd__int jobId,
                           struct condorSchedd__ClassAdStructAndStatusResponse & result);

int condorSchedd__declareFile(struct condorSchedd__Transaction transaction,
                              xsd__int clusterId,
                              xsd__int jobId,
                              xsd__string name,
                              xsd__int size,
                              enum condorSchedd__HashType hashType,
                              xsd__string hash,
                              struct condorSchedd__StatusResponse & result);

int condorSchedd__sendFile(struct condorSchedd__Transaction transaction,
                           xsd__int clusterId,
                           xsd__int jobId,
                           xsd__string name,
                           xsd__int offset,
                           struct xsd__base64Binary * data,
                           struct condorSchedd__StatusResponse & result);

int condorSchedd__discoverJobRequirements(struct condorCore__ClassAdStruct * jobAd,
                                          struct condorSchedd__RequirementsAndStatusResponse & result);

int condorSchedd__discoverDagRequirements(xsd__string dag,
                                          struct condorSchedd__RequirementsAndStatusResponse & result);

int condorSchedd__createJobTemplate(xsd__int clusterId,
                                    xsd__int jobId,
                                    xsd__string submitDescription,
                                    xsd__string owner,
                                    enum condorSchedd__UniverseType type,
                                    struct condorSchedd__ClassAdStructAndStatusResponse & result);
