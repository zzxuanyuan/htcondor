#import "gsoap_schedd_types.h"

//gsoap condor service name: condorSchedd

struct condor__StatusResponse {
  struct condor__Status response;
};

struct condor__TransactionAndStatusResponse {
  struct condor__TransactionAndStatus response;
};

struct condor__IntAndStatusResponse {
  struct condor__IntAndStatus response;
};

struct condor__RequirementsAndStatusResponse {
  struct condor__RequirementsAndStatus response;
};

struct condor__ClassAdStructArrayAndStatusResponse {
  struct condor__ClassAdStructArrayAndStatus response;
};

struct condor__ClassAdStructAndStatusResponse {
  struct condor__ClassAdStructAndStatus response;
};

struct condor__Base64DataAndStatusResponse {
  struct condor__Base64DataAndStatus response;
};

struct condor__FileInfoArrayAndStatusResponse {
  struct condor__FileInfoArrayAndStatus response;
};

int condor__beginTransaction(xsd__int duration,
                                   struct condor__TransactionAndStatusResponse & result);

int condor__commitTransaction(struct condor__Transaction transaction,
                                    struct condor__StatusResponse & result);

int condor__abortTransaction(struct condor__Transaction transaction,
                                   struct condor__StatusResponse & result);

int condor__extendTransaction(struct condor__Transaction transaction,
                                    xsd__int duration,
                                    struct condor__TransactionAndStatusResponse & result);

int condor__newCluster(struct condor__Transaction transaction,
                             struct condor__IntAndStatusResponse & result);

int condor__removeCluster(struct condor__Transaction transaction,
                                xsd__int clusterId,
                                xsd__string reason,
                                struct condor__StatusResponse & result);

int condor__newJob(struct condor__Transaction transaction,
                         xsd__int clusterId,
                         struct condor__IntAndStatusResponse & result);

int condor__removeJob(struct condor__Transaction transaction,
                            xsd__int clusterId,
                            xsd__int jobId,
                            xsd__string reason,
                            xsd__boolean force_removal,
                            struct condor__StatusResponse & result);

int condor__holdJob(struct condor__Transaction transaction,
                          xsd__int clusterId,
                          xsd__int jobId,
                          xsd__string reason,
                          xsd__boolean email_user,
                          xsd__boolean email_admin,
                          xsd__boolean system_hold,
                          struct condor__StatusResponse & result);

int condor__releaseJob(struct condor__Transaction transaction,
                             xsd__int clusterId,
                             xsd__int jobId,
                             xsd__string reason,
                             xsd__boolean email_user,
                             xsd__boolean email_admin,
                             struct condor__StatusResponse & result);

int condor__submit(struct condor__Transaction transaction,
                         xsd__int clusterId,
                         xsd__int jobId,
                         struct ClassAdStruct * jobAd,
                         struct condor__RequirementsAndStatusResponse & result);

int condor__getJobAds(struct condor__Transaction transaction,
                            xsd__string constraint,
                            struct condor__ClassAdStructArrayAndStatusResponse & result);

int condor__getJobAd(struct condor__Transaction transaction,
                           xsd__int clusterId,
                           xsd__int jobId,
                           struct condor__ClassAdStructAndStatusResponse & result);

int condor__declareFile(struct condor__Transaction transaction,
                              xsd__int clusterId,
                              xsd__int jobId,
                              xsd__string name,
                              xsd__int size,
                              enum condor__HashType hashType,
                              xsd__string hash,
                              struct condor__StatusResponse & result);

int condor__sendFile(struct condor__Transaction transaction,
                           xsd__int clusterId,
                           xsd__int jobId,
                           xsd__string name,
                           xsd__int offset,
                           struct xsd__base64Binary * data,
                           struct condor__StatusResponse & result);

int condor__getFile(struct condor__Transaction transaction,
                          xsd__int clusterId,
                          xsd__int jobId,
                          xsd__string name,
                          xsd__int offset,
                          xsd__int length,
                          struct condor__Base64DataAndStatusResponse & result);

int condor__closeSpool(struct condor__Transaction transaction,
                             xsd__int clusterId,
                             xsd__int jobId,
                             struct condor__StatusResponse & result);

int condor__listSpool(struct condor__Transaction transaction,
                            xsd__int clusterId,
                            xsd__int jobId,
                            struct condor__FileInfoArrayAndStatusResponse & result);

int condor__discoverJobRequirements(struct ClassAdStruct * jobAd,
                                          struct condor__RequirementsAndStatusResponse & result);

//int condor__discoverDagRequirements(xsd__string dag,
//                                          struct condor__RequirementsAndStatusResponse & result);

int condor__createJobTemplate(xsd__int clusterId,
                                    xsd__int jobId,
                                    xsd__string owner,
                                    enum condor__UniverseType type,
                                    xsd__string cmd,
                                    xsd__string args,
                                    xsd__string requirements,
                                    struct condor__ClassAdStructAndStatusResponse & result);
