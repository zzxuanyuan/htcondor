/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.
 * No use of the CONDOR Software Program Source Code is authorized
 * without the express consent of the CONDOR Team.  For more information
 * contact: CONDOR Team, Attention: Professor Miron Livny,
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685,
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure
 * by the U.S. Government is subject to restrictions as set forth in
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison,
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include "condor_classad.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

// Things to include for the stubs
#include "condor_version.h"
#include "condor_attributes.h"
#include "scheduler.h"
#include "condor_qmgr.h"
#include "MyString.h"
#include "internet.h"

#include "condor_ckpt_name.h"
#include "condor_config.h"

#include "loose_file_transfer.h"

#include "condorSchedd.nsmap"
#include "soap_scheddC.cpp"
#include "soap_scheddServer.cpp"

#include "schedd_api.h"

#include "../condor_c++_util/soap_helpers.cpp"

// XXX: There should be checks to see if the cluster/job Ids are valid
// in the transaction they are being used...


static int current_trans_id = 0;
static int trans_timer_id = -1;

template class HashTable<MyString, Job *>;
HashTable<MyString, Job *> jobs = HashTable<MyString, Job *>(1024, MyStringHash, rejectDuplicateKeys);

static bool
convert_FileInfoList_to_Array(struct soap * soap,
                              List<FileInfo> & list,
                              struct condorSchedd__FileInfoArray & array)
{
  array.__size = list.Number();
  array.__ptr = (struct condorSchedd__FileInfo *) soap_malloc(soap, array.__size * sizeof(struct condorSchedd__FileInfo));

  FileInfo *info;
  list.Rewind();
  for (int i = 0; list.Next(info); i++) {
    array.__ptr[i].name = strdup(info->name.GetCStr());
    array.__ptr[i].size = (int) info->size;
  }

  return true;
}

static bool valid_transaction_id(int id)
{
  if (current_trans_id == id || 0 == id ) {
    return true;
  } else {
    return false;
  }
}

static
int
getJob(int clusterId, int jobId, Job *&job)
{
  MyString key;
  key += clusterId;
  key += ".";
  key += jobId;

  return jobs.lookup(key, job);
}

static
int
insertJob(int clusterId, int jobId, Job *job)
{
  MyString key;
  key += clusterId;
  key += ".";
  key += jobId;

  return jobs.insert(key, job);
}

static
int
removeJob(int clusterId, int jobId)
{
  MyString key;
  key += clusterId;
  key += ".";
  key += jobId;

  return jobs.remove(key);
}

static
int
removeCluster(int clusterId)
{
  // XXX: This should really make sure they are all removed or none!
  MyString currentKey;
  Job *job;
  jobs.startIterations();
  while (jobs.iterate(currentKey, job)) {
    if (job->getClusterID() == clusterId) {
      jobs.remove(currentKey);
    }
  }

  return 0;
}

static
int
extendTransaction(const condorSchedd__Transaction & transaction)
{
  if (transaction.id &&	// must not be 0
      transaction.id == current_trans_id && // must be the current transaction
      trans_timer_id != -1) {

    if (transaction.duration < 1) {
      return 1;
    }

    daemonCore->Reset_Timer(trans_timer_id, transaction.duration);
  }

  return 0;
}

// TODO : Todd needs to redo all the transaction stuff and get it
// right.  For now it is in horrible "demo" mode with piles of
// assumptions (i.e. only one client, etc).  Once it is redone and
// decent, all the logic should move OUT of the stubs and into the
// schedd proper... since it should all work the same from the cedar
// side as well.
int
condorSchedd__transtimeout()
{
  struct condorSchedd__StatusResponse result;

  dprintf(D_ALWAYS,"SOAP in condorSchedd__transtimeout()\n");

  condorSchedd__Transaction transaction;
  transaction.id = current_trans_id;
  condorSchedd__abortTransaction(NULL, transaction, result);
  return TRUE;
}

int
condorSchedd__beginTransaction(struct soap *s,
                               int duration,
                               struct condorSchedd__TransactionAndStatusResponse & result)
{
  if ( current_trans_id ) {
    // if there is an existing transaction, abort it.
    // TODO - support more than one active transaction!!!
    struct condorSchedd__StatusResponse result;
    struct condorSchedd__Transaction transaction;
    transaction.id = current_trans_id;
    // XXX: handle errors
    condorSchedd__abortTransaction(s, transaction, result);
  }
  if ( duration < 1 ) {
    duration = 1;
  }

  trans_timer_id = daemonCore->Register_Timer(duration,
                                              (TimerHandler)&condorSchedd__transtimeout,
                                              "condorSchedd_transtimeout");

  current_trans_id = time(NULL);   // TODO : choose unique id - use time for now
  result.response.transaction.id = current_trans_id;
  result.response.transaction.duration = duration;
  result.response.status.code = SUCCESS;

  setQSock(NULL);	// Tell the qmgmt layer to allow anything -- that is, until
                        // we authenticate the client.

  BeginTransaction();

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__beginTransaction() id=%ld\n",result.response.transaction.id);

  return SOAP_OK;
}

int
condorSchedd__commitTransaction(struct soap *s,
                                struct condorSchedd__Transaction transaction,
                                struct condorSchedd__StatusResponse & result )
{
  result.response.code = SUCCESS;
  if ( transaction.id == current_trans_id ) {
    CommitTransaction();
    current_trans_id = 0;
    transaction.id = 0;
    if ( trans_timer_id != -1 ) {
      daemonCore->Cancel_Timer(trans_timer_id);
      trans_timer_id = -1;
    }
  }
  if ( !valid_transaction_id(transaction.id) ) {
    // TODO error - unrecognized transactionId
    result.response.code = INVALIDTRANSACTION;
  }

  //jobs.clear(); // XXX: Do the destructors get called?

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__commitTransaction() res=%d\n",result.response.code);
  return SOAP_OK;
}


int
condorSchedd__abortTransaction(struct soap *s,
                               struct condorSchedd__Transaction transaction,
                               struct condorSchedd__StatusResponse & result )
{
  result.response.code = SUCCESS;
  if ( transaction.id && transaction.id == current_trans_id ) {
    AbortTransactionAndRecomputeClusters();
    dprintf(D_ALWAYS, "SOAP cleared file hashtable for transaction: %d\n", transaction.id);

    // XXX: Call schedd.abort() for all schedd's

    // Let's forget about all the file associated with the transaction.
    //jobs.clear(); /* Memory leak? */

    current_trans_id = 0;
    transaction.id = 0;
    if ( trans_timer_id != -1 ) {
      daemonCore->Cancel_Timer(trans_timer_id);
      trans_timer_id = -1;
    }
  }
  if ( !valid_transaction_id(transaction.id) ) {
    // TODO error - unrecognized transactionId
    result.response.code = INVALIDTRANSACTION;
  }

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__abortTransaction() res=%d\n",result.response.code);
  return SOAP_OK;
}


int
condorSchedd__extendTransaction(struct soap *s,
                                struct condorSchedd__Transaction transaction,
                                int duration,
                                struct condorSchedd__TransactionAndStatusResponse & result )
{
  result.response.status.code = SUCCESS;

  transaction.duration = duration;
  if (extendTransaction(transaction)) {
    result.response.status.code = FAIL;
  } else {
    result.response.transaction.id = transaction.id;
    result.response.transaction.duration = duration;
  }
  
  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__extendTransaction() res=%d\n",result.response.status.code);
  return SOAP_OK;
}


int
condorSchedd__newCluster(struct soap *s,
                         struct condorSchedd__Transaction transaction,
                         struct condorSchedd__IntAndStatusResponse & result)
{
  if ( (transaction.id == 0) || (!valid_transaction_id(transaction.id)) ) {
    // TODO error - unrecognized transactionId
  }

  extendTransaction(transaction);

  result.response.integer = NewCluster();
  if ( result.response.integer == -1 ) {
    // TODO error case
  }

  result.response.status.code = SUCCESS;

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__newCluster() res=%d\n",result.response.status.code);
  return SOAP_OK;
}


int
condorSchedd__removeCluster(struct soap *s,
                            struct condorSchedd__Transaction transaction,
                            int clusterId,
                            char* reason,
                            struct condorSchedd__StatusResponse & result)
{
  // TODO!!!
  result.response.code = SUCCESS;
  if ( !valid_transaction_id(transaction.id) ) {
    // TODO error - unrecognized transactionId
    result.response.code = INVALIDTRANSACTION;
  } else {
    extendTransaction(transaction);

    if (0 == DestroyCluster(clusterId,reason)) {  // returns -1 or 0
      result.response.code = SUCCESS;
    } else {
      result.response.code = FAIL;
    }
  }

  if (removeCluster(clusterId)) {
    result.response.code = FAIL;
  }

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__removeCluster() res=%d\n",result.response.code);
  return SOAP_OK;
}


int
condorSchedd__newJob(struct soap *s,
                     struct condorSchedd__Transaction transaction,
                     int clusterId,
                     struct condorSchedd__IntAndStatusResponse & result)
{
  result.response.integer = 0;
  if ( (transaction.id == 0) || (!valid_transaction_id(transaction.id)) ) {
    // TODO error - unrecognized transactionId
  }

  extendTransaction(transaction);

  result.response.integer = NewProc(clusterId);
  if ( result.response.integer == -1 ) {
    // TODO error case
  }

  // Create a Job for this new job.
  Job *job = new Job(clusterId, result.response.integer);
  if (insertJob(clusterId, result.response.integer, job)) {
    result.response.status.code = FAIL;

    return SOAP_OK;
  }

  result.response.status.code = SUCCESS;

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__newJob() res=%d\n",result.response.status.code);
  return SOAP_OK;
}


int
condorSchedd__removeJob(struct soap *s,
                        struct condorSchedd__Transaction transaction,
                        int clusterId,
                        int jobId,
                        char* reason,
                        bool force_removal,
                        struct condorSchedd__StatusResponse & result)
{
  // TODO --- do something w/ force_removal flag; it is ignored for now.
  result.response.code = SUCCESS;
  if ( !valid_transaction_id(transaction.id) ) {
    // TODO error - unrecognized transactionId
  }

  extendTransaction(transaction);

  if ( !abortJob(clusterId,jobId,reason,transaction.id ? false : true) )
    {
      // TODO error - remove failed
      result.response.code = FAIL;
    }

  if (removeJob(clusterId, jobId)) {
    result.response.code = FAIL;
  }

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__removeJob() res=%d\n",result.response.code);
  return SOAP_OK;
}


int
condorSchedd__holdJob(struct soap *s,
                      struct condorSchedd__Transaction transaction,
                      int clusterId,
                      int jobId,
                      char* reason,
                      bool email_user,
                      bool email_admin,
                      bool system_hold,
                      struct condorSchedd__StatusResponse & result)
{
  result.response.code = SUCCESS;
  if ( !valid_transaction_id(transaction.id) ) {
    // TODO error - unrecognized transactionId
  }

  extendTransaction(transaction);

  if ( !holdJob(clusterId,jobId,reason,transaction.id ? false : true,
                email_user, email_admin, system_hold) )
    {
      // TODO error - remove failed
      result.response.code = FAIL;
    }

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__holdJob() res=%d\n",result.response.code);
  return SOAP_OK;
}


int
condorSchedd__releaseJob(struct soap *s,
                         struct condorSchedd__Transaction transaction,
                         int clusterId,
                         int jobId,
                         char* reason,
                         bool email_user,
                         bool email_admin,
                         struct condorSchedd__StatusResponse & result)
{
  result.response.code = SUCCESS;
  if ( !valid_transaction_id(transaction.id) ) {
    // TODO error - unrecognized transactionId
  }

  extendTransaction(transaction);

  if ( !releaseJob(clusterId,jobId,reason,transaction.id ? false : true,
                   email_user, email_admin) )
    {
      // TODO error - release failed
      result.response.code = FAIL;
    }

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__releaseJob() res=%d\n",result.response.code);
  return SOAP_OK;
}


int
condorSchedd__submit(struct soap *s,
                     struct condorSchedd__Transaction transaction,
                     int clusterId,
                     int jobId,
                     struct condorCore__ClassAdStruct * jobAd,
                     struct condorSchedd__RequirementsAndStatusResponse & result)
{
  result.response.status.code = SUCCESS;

  if ( (transaction.id == 0) || (!valid_transaction_id(transaction.id)) ) {
    // TODO error - unrecognized transactionId
  }

  extendTransaction(transaction);

  Job *job = new Job(clusterId, jobId);
  if (getJob(clusterId, jobId, job)) {
    result.response.status.code = UNKNOWNJOB;

    return SOAP_OK;
  }

  ClassAd realJobAd;
  if (!convert_adStruct_to_ad(s, &realJobAd, jobAd)) {
    result.response.status.code = FAIL;

    return SOAP_OK;
  }

  if (job->submit(*jobAd)) {
    result.response.status.code = FAIL;

    return SOAP_OK;
  }

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__submit() res=%d\n",result.response.status.code);
  return SOAP_OK;
}


int
condorSchedd__getJobAds(struct soap *s,
                        struct condorSchedd__Transaction transaction,
                        char *constraint,
                        struct condorSchedd__ClassAdStructArrayAndStatusResponse & result )
{
  dprintf(D_ALWAYS,"SOAP entering condorSchedd__getJobAds() \n");

  if ( !valid_transaction_id(transaction.id) ) {
    // TODO error - unrecognized transactionId
  }

  extendTransaction(transaction);

  List<ClassAd> adList;
  ClassAd *ad = GetNextJobByConstraint(constraint,1);
  while ( ad ) {
    adList.Append(ad);
    ad = GetNextJobByConstraint(constraint,0);
  }

  // fill in our soap struct response
  if ( !convert_adlist_to_adStructArray(s,&adList,&result.response.classAdArray) ) {
    dprintf(D_ALWAYS,"condorSchedd__getJobAds: convert_adlist_to_adStructArray failed!\n");
  }

  result.response.status.code = SUCCESS;

  return SOAP_OK;
}


int
condorSchedd__getJobAd(struct soap *s,
                       struct condorSchedd__Transaction transaction,
                       int clusterId,
                       int jobId,
                       struct condorSchedd__ClassAdStructAndStatusResponse & result )
{
  // TODO : deal with transaction consistency; currently, job ad is
  // invisible until a commit.  not very ACID compliant, is it? :(

  dprintf(D_ALWAYS,"SOAP entering condorSchedd__getJobAd() \n");

  if ( !valid_transaction_id(transaction.id) ) {
    // TODO error - unrecognized transactionId
  }

  extendTransaction(transaction);

  ClassAd *ad = GetJobAd(clusterId,jobId);
  if ( !convert_ad_to_adStruct(s,ad,&result.response.classAd) ) {
    dprintf(D_ALWAYS,"condorSchedd__getJobAds: convert_adlist_to_adStructArray failed!\n");
  }

  result.response.status.code = SUCCESS;

  return SOAP_OK;
}

int
condorSchedd__declareFile(struct soap *soap,
                          struct condorSchedd__Transaction transaction,
                          int clusterId,
                          int jobId,
                          char * name,
                          int size,
                          enum condorSchedd__HashType hashType,
                          char * hash,
                          struct condorSchedd__StatusResponse & result)
{
  dprintf(D_ALWAYS,"SOAP entering condorSchedd__declareFile() \n");

  if (!valid_transaction_id(transaction.id)) {
    // TODO error - unrecognized transactionId
    result.response.code = INVALIDTRANSACTION;

    return SOAP_OK;
  }

  extendTransaction(transaction);

  Job *job;
  if (getJob(clusterId, jobId, job)) {
    result.response.code = UNKNOWNJOB;

    return SOAP_OK;
  }

  result.response.code = SUCCESS;

  if (job->declare_file(MyString(name), size)) {
    result.response.code = FAIL;
  }

  return SOAP_OK;
}

int
condorSchedd__sendFile(struct soap *soap,
                       struct condorSchedd__Transaction transaction,
                       int clusterId,
                       int jobId,
                       char * filename,
                       int offset,
                       struct xsd__base64Binary *data,
                       struct condorSchedd__StatusResponse & result)
{
  dprintf(D_ALWAYS,"SOAP entering condorSchedd__sendFile() \n");

  if (!valid_transaction_id(transaction.id)) {
    // TODO error - unrecognized transactionId
    result.response.code = INVALIDTRANSACTION;
  }

  extendTransaction(transaction);

  Job *job;
  if (getJob(clusterId, jobId, job)) {
    result.response.code = INVALIDTRANSACTION;

    return SOAP_OK;
  }

  if (0 == job->send_file(MyString(filename),
                          offset,
                          (char *) data->__ptr,
                          data->__size)) {
    result.response.code = SUCCESS;
  } else {
    result.response.code = FAIL;
  }

  return SOAP_OK;
}

int condorSchedd__getFile(struct soap *soap,
                          struct condorSchedd__Transaction transaction,
                          int clusterId,
                          int jobId,
                          char * name,
                          int offset,
                          int length,
                          struct condorSchedd__Base64DataAndStatusResponse & result)
{
  dprintf(D_ALWAYS,"SOAP entering condorSchedd__getFile() \n");

  if (!valid_transaction_id(transaction.id)) {
    // TODO error - unrecognized transactionId
    result.response.status.code = INVALIDTRANSACTION;
  }

  extendTransaction(transaction);

  Job *job;
  if (getJob(clusterId, jobId, job)) {
    result.response.status.code = UNKNOWNJOB;

    return SOAP_OK;
  }

  unsigned char * data =
    (unsigned char *) soap_malloc(soap, length * sizeof(unsigned char));
  if (0 == job->get_file(MyString(name),
                         offset,
                         length,
                         data)) {
    result.response.status.code = SUCCESS;

    result.response.data.__ptr = data;
    result.response.data.__size = length;
  } else {
    result.response.status.code = FAIL;
  }

  return SOAP_OK;
}

int condorSchedd__closeSpool(struct soap *soap,
                             struct condorSchedd__Transaction transaction,
                             xsd__int clusterId,
                             xsd__int jobId,
                             struct condorSchedd__StatusResponse & result)
{
  dprintf(D_ALWAYS,"SOAP entering condorSchedd__closeSpool() \n");

  if (!valid_transaction_id(transaction.id)) {
    // TODO error - unrecognized transactionId
    result.response.code = INVALIDTRANSACTION;
  }

  extendTransaction(transaction);

  if (SetAttribute(clusterId, jobId, "FilesRetrieved", "TRUE")) {
    result.response.code = FAIL;
  } else {
    result.response.code = SUCCESS;
  }

  return SOAP_OK;
}

int
condorSchedd__listSpool(struct soap * soap,
                        struct condorSchedd__Transaction transaction,
                        int clusterId,
                        int jobId,
                        struct condorSchedd__FileInfoArrayAndStatusResponse & result)
{
  dprintf(D_ALWAYS,"SOAP entering condorSchedd__listSpool() \n");

  if (!valid_transaction_id(transaction.id)) {
    // TODO error - unrecognized transactionId
    result.response.status.code = INVALIDTRANSACTION;
  }

  extendTransaction(transaction);

  Job *job;
  if (getJob(clusterId, jobId, job)) {
    result.response.status.code = UNKNOWNJOB;

    return SOAP_OK;
  }
  
  List<FileInfo> files;
  if (job->get_spool_list(files)) {
    result.response.status.code = FAIL;

    return SOAP_OK;
  }

  if (convert_FileInfoList_to_Array(soap, files, result.response.info)) {
    result.response.status.code = SUCCESS;
  } else {
    result.response.status.code = FAIL;
  }

  return SOAP_OK;
}

int
condorSchedd__discoverJobRequirements(struct soap *soap,
                                      struct condorCore__ClassAdStruct * jobAd,
                                      struct condorSchedd__RequirementsAndStatusResponse & result)
{
  LooseFileTransfer fileTransfer;

  ClassAd ad;
  StringList inputFiles;
  char *buffer;

  convert_adStruct_to_ad(soap, &ad, jobAd);

  fileTransfer.SimpleInit(&ad, false, false);

  fileTransfer.getInputFiles(inputFiles);

  result.response.requirements.__size = inputFiles.number();
  result.response.requirements.__ptr =
    (condorSchedd__Requirement *) soap_malloc(soap, result.response.requirements.__size * sizeof(condorSchedd__Requirements));
  inputFiles.rewind();
  int i = 0;
  while ((buffer = inputFiles.next()) &&
         (i < result.response.requirements.__size)) {
    result.response.requirements.__ptr[i] = strdup(buffer);
    i++;
  }

  result.response.status.code = SUCCESS;

  return SOAP_OK;
}

int
condorSchedd__discoverDagRequirements(struct soap *soap,
                                      char *dag,
                                      struct condorSchedd__RequirementsAndStatusResponse & result)
{
  return SOAP_FAULT;
}

int
condorSchedd__createJobTemplate(struct soap *soap,
                                int clusterId,
                                int jobId,
                                char * owner,
                                condorSchedd__UniverseType universe,
                                char * cmd,
                                char * args,
                                struct condorSchedd__ClassAdStructAndStatusResponse & result)
{
  MyString attribute;

  ClassAd *job = new ClassAd();

  attribute = MyString(ATTR_CLUSTER_ID) + " = " + clusterId;
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_PROC_ID) + " = " + jobId;
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_Q_DATE) + " = " + ((int) time((time_t *) 0));
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_COMPLETION_DATE) + " = 0";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_OWNER) + " = \"" + owner + "\"";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_REMOTE_WALL_CLOCK) + " = 0.0";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_LOCAL_USER_CPU) + " = 0.0";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_LOCAL_SYS_CPU) + " = 0.0";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_REMOTE_USER_CPU) + " = 0.0";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_REMOTE_SYS_CPU) + " = 0.0";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_EXIT_STATUS) + " = 0";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_NUM_CKPTS) + " = 0";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_NUM_RESTARTS) + " = 0";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_NUM_SYSTEM_HOLDS) + " = 0";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_COMMITTED_TIME) + " = 0";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_TOTAL_SUSPENSIONS) + " = 0";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_LAST_SUSPENSION_TIME) + " = 0";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_CUMULATIVE_SUSPENSION_TIME) + " = 0";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_ON_EXIT_BY_SIGNAL) + " = FALSE";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_ROOT_DIR) + " = \"/\"";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_UNIVERSE) + " = " + universe;
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_CMD) + " = \"" + cmd + "\"";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_MIN_HOSTS) + " = 1";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_MAX_HOSTS) + " = 1";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_CURRENT_HOSTS) + " = 0";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_WANT_REMOTE_SYSCALLS) + " = FALSE";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_WANT_CHECKPOINT) + " = FALSE";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_STATUS) + " = 1";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_ENTERED_CURRENT_STATE) + " = " + (((int) time((time_t *) 0)) / 1000);
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_PRIO) + " = 0";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_ENVIRONMENT) + " = \"\"";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_NOTIFICATION) + " = 2";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_KILL_SIG) + " = \"SIGTERM\"";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_IMAGE_SIZE) + " = 0";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_INPUT) + " = \"/dev/null\"";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_TRANSFER_INPUT) + " = FALSE";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_OUTPUT) + " = \"/dev/null\"";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_ERROR) + " = \"/dev/null\"";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_BUFFER_SIZE) + " = " + (512 * 1024);
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_BUFFER_BLOCK_SIZE) + " = " + (32 * 1024);
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_SHOULD_TRANSFER_FILES) + " = TRUE";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_TRANSFER_FILES) + " = \"ONEXIT\"";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_WHEN_TO_TRANSFER_OUTPUT) + " = \"ON_EXIT\"";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_REQUIREMENTS) + " = TRUE";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_PERIODIC_HOLD_CHECK) + " = FALSE";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_PERIODIC_RELEASE_CHECK) + " = FALSE";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_PERIODIC_REMOVE_CHECK) + " = FALSE";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_ON_EXIT_HOLD_CHECK) + " = FALSE";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_ON_EXIT_REMOVE_CHECK) + " = TRUE";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString("FilesRetrieved") + " = FALSE";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_LEAVE_IN_QUEUE) + " = FilesRetrieved==FALSE";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_ARGUMENTS) + " = \"" + args + "\"";
  dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  job->Insert(attribute.GetCStr());

  // Need more attributes! Skim submit.C more.

  result.response.status.code = SUCCESS;
  convert_ad_to_adStruct(soap, job, &result.response.classAd);

  int i;
  MyString name, value;
  for (i = 0; i < result.response.classAd.__size; i++) {
    name = result.response.classAd.__ptr[i].name;

    if (!result.response.classAd.__ptr[i].value)
      value = "UNDEFINED";
    else
      value = result.response.classAd.__ptr[i].value;

    if (STRING == result.response.classAd.__ptr[i].type)
      attribute = name + "=\"" + value + "\"";
    else
      attribute = name + "=" + value;

    dprintf(D_ALWAYS, "%s\n", attribute.GetCStr());
  }

  return SOAP_OK;
}

///////////////////////////////////////////////////////////////////////////////
// TODO : This should move into daemonCore once we figure out how we wanna link
///////////////////////////////////////////////////////////////////////////////

/*
int
condorCore__getPlatformString(struct soap *soap,
                              void *,
                              struct condorCore__StringAndStatus &result)
{
  result.response.message = CondorPlatform();
  result.response.status.code = 0;
  return SOAP_OK;
}

int
condorCore__getVersionString(struct soap *soap,
                             void *,
                             struct condorCore__StringAndStatus &result)
{
  result.response.message = CondorVersion();
  result.response.status.code = 0;
  return SOAP_OK;
}

int
condorCore__getInfoAd(struct soap *soap,
                      void *,
                      struct condorCore__ClassAdStructAndStatusResponse & result)
{
  char* todd = "Todd A Tannenbaum";

  result.response.classAd.__size = 3;
  result.response.classAd.__ptr = (struct condorCore__ClassAdStructAttr *)soap_malloc(soap,3 * sizeof(struct condorCore__ClassAdStructAttr));

  result.response.classAd.__ptr[0].name = "Name";
  result.response.classAd.__ptr[0].type = STRING;
  result.response.classAd.__ptr[0].value = todd;

  result.response.classAd.__ptr[1].name = "Age";
  result.response.classAd.__ptr[1].type = INTEGER;
  result.response.classAd.__ptr[1].value = "35";
  int* age = (int*)soap_malloc(soap,sizeof(int));
  *age = 35;

  result.response.classAd.__ptr[2].name = "Friend";
  result.response.classAd.__ptr[2].type = STRING;
  result.response.classAd.__ptr[2].value = todd;

  result.response.status.code = 0;

  return SOAP_OK;
}
*/
