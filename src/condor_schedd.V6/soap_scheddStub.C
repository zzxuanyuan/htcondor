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

#include "../condor_c++_util/soap_helpers.cpp"

// XXX: There should be checks to see if the cluster/job Ids are valid
// in the transaction they are being used...


static int current_trans_id = 0;
static int trans_timer_id = -1;

class JobFile
{
public:
  FILE * file;
  MyString name;
  int size;
  int currentOffset;
};

template class HashTable<MyString, JobFile>;
HashTable<MyString, JobFile> jobFiles = HashTable<MyString, JobFile>(64, MyStringHash, rejectDuplicateKeys);

MyString *spoolDirectory = NULL;

static bool valid_transaction_id(int id)
{
  if (current_trans_id == id || 0 == id ) {
    return true;
  } else {
    return false;
  }
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

  // Let's just forget everything about the job. This will result in
  // the spool directory sitting around until we find it later, which
  // we can always do with the cluter&proc IDs.
  delete spoolDirectory;
  spoolDirectory = NULL;
  jobFiles.clear();

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

    // Make sure we delete all the files that were sent over.
    JobFile jobFile;
    jobFiles.startIterations();
    while (jobFiles.iterate(jobFile)) {
      fclose(jobFile.file);
      remove(jobFile.name.GetCStr());
    }

    if (NULL != spoolDirectory) {
      // Now let's delete the spoolDirectory itself.
      remove(spoolDirectory->GetCStr());
      delete spoolDirectory;
      spoolDirectory = NULL; // Actually do a rmdir too!
    }

    // Let's forget about all the file associated with the transaction.
    jobFiles.clear(); /* Memory leak? */

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
  result.response.status.code = FAIL;
  if ( transaction.id &&	// must not be 0
       transaction.id == current_trans_id &&	// must be the current transaction
       trans_timer_id != -1 )
    {
      result.response.status.code = SUCCESS;
      if ( duration < 1 ) {
        duration = 1;
      }
      daemonCore->Reset_Timer(trans_timer_id,duration);

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
    if (0 == DestroyCluster(clusterId,reason)) {  // returns -1 or 0
      result.response.code = SUCCESS;
    } else {
      result.response.code = FAIL;
    }
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
  result.response.integer = NewProc(clusterId);
  if ( result.response.integer == -1 ) {
    // TODO error case
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
  if ( !abortJob(clusterId,jobId,reason,transaction.id ? false : true) )
    {
      // TODO error - remove failed
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

  int i,rval;
  for (i=0; i < jobAd->__size; i++ ) {
    const char* name = jobAd->__ptr[i].name;
    const char* value = jobAd->__ptr[i].value;
    if (!name) continue;
    if (!value) value="UNDEFINED";

    if ( jobAd->__ptr[i].type == 's' ) {
      // string type - put value in quotes as hint for ClassAd parser
      rval = SetAttributeString(clusterId,jobId,name,value);
    } else {
      // all other types can be deduced by the ClassAd parser
      rval = SetAttribute(clusterId,jobId,name,value);
    }
    if ( rval < 0 ) {
      result.response.status.code = FAIL;
    }
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
  }

  JobFile jobFile;
  jobFile.size = size;
  jobFile.currentOffset = 0;

  FILE *file;

  if (NULL == spoolDirectory) {
    // XXX: Share code with file_transfer.C
    char * Spool = param("SPOOL");
    if (Spool) {
      spoolDirectory = new MyString(strdup(gen_ckpt_name(Spool, clusterId, jobId, 0)));
      if ((mkdir(spoolDirectory->GetCStr(), 0777) < 0)) {
        // mkdir can return 17 = EEXIST (dirname exists) or 2 = ENOENT (path not found)
        dprintf(D_FULLDEBUG,
                "condorSchedd__declareFile: mkdir(%s) failed, errno: %d\n",
                spoolDirectory->GetCStr(),
                errno);
      }
    }
  }

  // XXX: Handle errors!
  // XXX: How do I get the FS separator?
  jobFile.name = *spoolDirectory + "/" + name;
  file = fopen(jobFile.name.GetCStr(), "w");
  jobFile.file = file;
  jobFiles.insert(MyString(name), jobFile);

  result.response.code = SUCCESS; // OK, send it. 1 means don't?

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

  JobFile jobFile;
  if (-1 == jobFiles.lookup(MyString(filename), jobFile)) {
    result.response.code = UNKNOWNFILE; // File unknown.

    return SOAP_OK;
  }

  // XXX: Handle errors!
  fseek(jobFile.file, offset, SEEK_SET);
  fwrite(data->__ptr, sizeof(unsigned char), data->__size, jobFile.file);
  fflush(jobFile.file);

  result.response.code = SUCCESS; // OK, send it. 1 means don't?

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
    (condorSchedd__Requirement *) calloc(sizeof(condorSchedd__Requirement),
                                         result.response.requirements.__size);
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
                                xsd__int clusterId,
                                xsd__int jobId,
                                xsd__string submitDescription,
                                xsd__string owner,
                                condorSchedd__UniverseType universe,
                                struct condorSchedd__ClassAdStructAndStatusResponse & result)
{
  MyString attribute;

  ClassAd job;

  job.SetMyTypeName (JOB_ADTYPE);
  job.SetTargetTypeName (STARTD_ADTYPE);


  attribute = MyString(ATTR_Q_DATE) + " = " + ((int) time((time_t *) 0));
  job.Insert(attribute.GetCStr());

  attribute = MyString(ATTR_COMPLETION_DATE) + " = 0";
  job.Insert(attribute.GetCStr());

  attribute = MyString(ATTR_OWNER) + " = \"" + owner + "\"";
  job.Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_REMOTE_WALL_CLOCK) + " = 0.0";
  job.Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_LOCAL_USER_CPU) + " = 0.0";
  job.Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_LOCAL_SYS_CPU) + " = 0.0";
  job.Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_REMOTE_USER_CPU) + " = 0.0";
  job.Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_REMOTE_SYS_CPU) + " = 0.0";
  job.Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_EXIT_STATUS) + " = 0";
  job.Insert(attribute.GetCStr());

  attribute = MyString(ATTR_NUM_CKPTS) + " = 0";
  job.Insert(attribute.GetCStr());

  attribute = MyString(ATTR_NUM_RESTARTS) + " = 0";
  job.Insert(attribute.GetCStr());

  attribute = MyString(ATTR_NUM_SYSTEM_HOLDS) + " = 0";
  job.Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_COMMITTED_TIME) + " = 0";
  job.Insert(attribute.GetCStr());

  attribute = MyString(ATTR_TOTAL_SUSPENSIONS) + " = 0";
  job.Insert(attribute.GetCStr());

  attribute = MyString(ATTR_LAST_SUSPENSION_TIME) + " = 0";
  job.Insert(attribute.GetCStr());

  attribute = MyString(ATTR_CUMULATIVE_SUSPENSION_TIME) + " = 0";
  job.Insert(attribute.GetCStr());

  attribute = MyString(ATTR_ON_EXIT_BY_SIGNAL) + " = FALSE";
  job.Insert(attribute.GetCStr());

  // Need more attributes! Skim submit.C more.

  result.response.status.code = SUCCESS;
  convert_ad_to_adStruct(soap, &job, &result.response.classAd);

  // This isn't really implemted yet. The stuff above is just test code.
  return SOAP_FAULT;
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
