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

#include "condorSchedd.nsmap"
#include "soap_scheddC.cpp"
#include "soap_scheddServer.cpp"

#include "../condor_c++_util/soap_helpers.cpp"

static int current_trans_id = 0;
static int trans_timer_id = -1;

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
  struct condorCore__Status result;

  dprintf(D_ALWAYS,"SOAP in condorSchedd__transtimeout()\n");

  condorSchedd__Transaction transaction;
  transaction.id = current_trans_id;
  condorSchedd__abortTransaction(NULL, transaction, result);
  return TRUE;
}

int
condorSchedd__beginTransaction(struct soap *s,
                               int duration,
                               struct condorSchedd__TransactionAndStatus & result)
{
  if ( current_trans_id ) {
    // if there is an existing transaction, abort it.
    // TODO - support more than one active transaction!!!
    condorCore__Status result;
    condorSchedd__Transaction transaction;
    transaction.id = current_trans_id;
    condorSchedd__abortTransaction(s, transaction, result);
  }
  if ( duration < 1 ) {
    duration = 1;
  }

  trans_timer_id = daemonCore->Register_Timer(duration,
                                              (TimerHandler)&condorSchedd__transtimeout,
                                              "condorSchedd_transtimeout");

  current_trans_id = time(NULL);   // TODO : choose unique id - use time for now
  result.transaction.id = current_trans_id;
  result.transaction.duration = duration;
  result.status.code = 0; // Success! XXX: Define this somewhere!

  setQSock(NULL);	// Tell the qmgmt layer to allow anything -- that is, until
                        // we authenticate the client.

  BeginTransaction();

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__beginTransaction() id=%ld\n",result.transaction.id);

  return SOAP_OK;
}

int
condorSchedd__commitTransaction(struct soap *s,
                                struct condorSchedd__Transaction transaction,
                                struct condorCore__Status & result )
{
  result.code = 0;
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
    result.code = -1;
  }

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__commitTransaction() res=%d\n",result.code);
  return SOAP_OK;
}


int
condorSchedd__abortTransaction(struct soap *s,
                               struct condorSchedd__Transaction transaction,
                               struct condorCore__Status & result )
{
  result.code = 0;
  if ( transaction.id && transaction.id == current_trans_id ) {
    AbortTransactionAndRecomputeClusters();
    current_trans_id = 0;
    transaction.id = 0;
    if ( trans_timer_id != -1 ) {
      daemonCore->Cancel_Timer(trans_timer_id);
      trans_timer_id = -1;
    }
  }
  if ( !valid_transaction_id(transaction.id) ) {
    // TODO error - unrecognized transactionId
    result.code = -1;
  }

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__abortTransaction() res=%d\n",result.code);
  return SOAP_OK;
}


int
condorSchedd__extendTransaction(struct soap *s,
                                struct condorSchedd__Transaction transaction,
                                int duration,
                                struct condorSchedd__TransactionAndStatus & result )
{
  result.status.code = -1;
  if ( transaction.id &&	// must not be 0
       transaction.id == current_trans_id &&	// must be the current transaction
       trans_timer_id != -1 )
    {
      result.status.code = 0;
      if ( duration < 1 ) {
        duration = 1;
      }
      daemonCore->Reset_Timer(trans_timer_id,duration);

      result.transaction.id = transaction.id;
      result.transaction.duration = duration;
    }

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__extendTransaction() res=%d\n",result.status.code);
  return SOAP_OK;
}


int
condorSchedd__newCluster(struct soap *s,
                         struct condorSchedd__Transaction transaction,
                         struct condorSchedd__IntAndStatus & result)
{
  if ( (transaction.id == 0) || (!valid_transaction_id(transaction.id)) ) {
    // TODO error - unrecognized transactionId
  }
  result.integer = NewCluster();
  if ( result.integer == -1 ) {
    // TODO error case
  }

  result.status.code = 0;

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__newCluster() res=%d\n",result.status.code);
  return SOAP_OK;
}


int
condorSchedd__removeCluster(struct soap *s,
                            struct condorSchedd__Transaction transaction,
                            int clusterId,
                            char* reason,
                            struct condorCore__Status & result)
{
  // TODO!!!
  result.code = 0;
  if ( !valid_transaction_id(transaction.id) ) {
    // TODO error - unrecognized transactionId
    result.code = -1;
  } else {
    result.code = DestroyCluster(clusterId,reason);  // returns -1 or 0
  }

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__removeCluster() res=%d\n",result.code);
  return SOAP_OK;
}


int
condorSchedd__newJob(struct soap *s,
                     struct condorSchedd__Transaction transaction,
                     int clusterId,
                     struct condorSchedd__IntAndStatus & result)
{
  result.integer = 0;
  if ( (transaction.id == 0) || (!valid_transaction_id(transaction.id)) ) {
    // TODO error - unrecognized transactionId
  }
  result.integer = NewProc(clusterId);
  if ( result.integer == -1 ) {
    // TODO error case
  }

  result.status.code = 0;

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__newJob() res=%d\n",result.status.code);
  return SOAP_OK;
}


int
condorSchedd__removeJob(struct soap *s,
                        struct condorSchedd__Transaction transaction,
                        int clusterId,
                        int jobId,
                        char* reason,
                        bool force_removal,
                        struct condorCore__Status & result)
{
  // TODO --- do something w/ force_removal flag; it is ignored for now.
  result.code = 0;
  if ( !valid_transaction_id(transaction.id) ) {
    // TODO error - unrecognized transactionId
  }
  if ( !abortJob(clusterId,jobId,reason,transaction.id ? false : true) )
    {
      // TODO error - remove failed
      result.code = -1;
    }

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__removeJob() res=%d\n",result.code);
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
                      struct condorCore__Status & result)
{
  result.code = 0;
  if ( !valid_transaction_id(transaction.id) ) {
    // TODO error - unrecognized transactionId
  }
  if ( !holdJob(clusterId,jobId,reason,transaction.id ? false : true,
                email_user, email_admin, system_hold) )
    {
      // TODO error - remove failed
      result.code = -1;
    }

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__holdJob() res=%d\n",result.code);
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
                         struct condorCore__Status & result)
{
  result.code = 0;
  if ( !valid_transaction_id(transaction.id) ) {
    // TODO error - unrecognized transactionId
  }
  if ( !releaseJob(clusterId,jobId,reason,transaction.id ? false : true,
                   email_user, email_admin) )
    {
      // TODO error - release failed
      result.code = -1;
    }

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__releaseJob() res=%d\n",result.code);
  return SOAP_OK;
}


int
condorSchedd__submit(struct soap *s,
                     struct condorSchedd__Transaction transaction,
                     int clusterId,
                     int jobId,
                     struct condorCore__ClassAdStruct * jobAd,
                     struct condorSchedd__RequirementsAndStatus & result)
{
  result.status.code = 0;

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
      result.status.code = -1;
    }
  }

  dprintf(D_ALWAYS,"SOAP leaving condorSchedd__submit() res=%d\n",result.status.code);
  return SOAP_OK;
}


int
condorSchedd__getJobAds(struct soap *s,
                        struct condorSchedd__Transaction transaction,
                        char *constraint,
                        struct condorCore__ClassAdStructArrayAndStatus & result )
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
  if ( !convert_adlist_to_adStructArray(s,&adList,&result.classAdArray) ) {
    dprintf(D_ALWAYS,"condorSchedd__getJobAds: convert_adlist_to_adStructArray failed!\n");
  }

  result.status.code = 0;

  return SOAP_OK;
}


int
condorSchedd__getJobAd(struct soap *s,
                       struct condorSchedd__Transaction transaction,
                       int clusterId,
                       int jobId,
                       struct condorCore__ClassAdStructAndStatus & result )
{
  // TODO : deal with transaction consistency; currently, job ad is
  // invisible until a commit.  not very ACID compliant, is it? :(

  dprintf(D_ALWAYS,"SOAP entering condorSchedd__getJobAd() \n");

  if ( !valid_transaction_id(transaction.id) ) {
    // TODO error - unrecognized transactionId
  }

  ClassAd *ad = GetJobAd(clusterId,jobId);
  if ( !convert_ad_to_adStruct(s,ad,&result.classAd) ) {
    dprintf(D_ALWAYS,"condorSchedd__getJobAds: convert_adlist_to_adStructArray failed!\n");
  }

  result.status.code = 0;

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
                       struct condorCore__Status & result)
{
  return SOAP_FAULT;
}

int
condorSchedd__discoverJobRequirements(struct soap *soap,
                                      struct condorCore__ClassAdStruct * jobAd,
                                      struct condorSchedd__RequirementsAndStatus & result)
{
  /*
   * This is all test code. It should actually share code with
   * file_transfer.C's SimpleInit not duplicate it!
   */

  ClassAd ad;
  StringList *inputFiles;
  char buffer[ATTRLIST_MAX_EXPRESSION]; // Scary, large buffer?

  convert_adStruct_to_ad(soap, &ad, jobAd);

  // Set inputFiles to be ATTR_TRANSFER_INPUT_FILES plus
  // ATTR_JOB_INPUT, ATTR_JOB_CMD, and ATTR_ULOG_FILE if simple_init.
  if (ad.LookupString(ATTR_TRANSFER_INPUT_FILES, buffer) == 1) {
    inputFiles = new StringList(buffer,",");
  } else {
    inputFiles = new StringList(NULL,",");
  }
  if (ad.LookupString(ATTR_JOB_INPUT, buffer) == 1) {
    // only add to list if not NULL_FILE (i.e. /dev/null)
    if ( !inputFiles->contains(buffer) )
      inputFiles->append(buffer);
  }
  if ( ad.LookupString(ATTR_ULOG_FILE, buffer) == 1 ) {
    // add to input files if sending from submit to the schedd
    if ( !inputFiles->contains(buffer) )
      inputFiles->append(buffer);
  }
  if ( ad.LookupString(ATTR_X509_USER_PROXY, buffer) == 1 ) {
    // add to input files
    if ( !inputFiles->contains(buffer) )
      inputFiles->append(buffer);
  }
  if (ad.LookupString(ATTR_JOB_CMD, buffer) == 1) {
    // add to input files
    if ( !inputFiles->contains(buffer) )
      inputFiles->append(buffer);
  }

  //result =
  //  (condorSchedd__RequirementsAndStatus *)
  //  soap_malloc(soap, sizeof(condorSchedd__RequirementsAndStatus));

  result.requirements.__size = inputFiles->number();
  result.requirements.__ptr =
    (condorSchedd__Requirement *) calloc(sizeof(condorSchedd__Requirement),
                                         result.requirements.__size);
  inputFiles->rewind();
  int i;
  for (i = 0;
       i < result.requirements.__size &&
         NULL != strcpy(buffer, inputFiles->next());
       i++) {
    result.requirements.__ptr[i] = strdup(buffer);
  }

  result.status.code = 0;

  return SOAP_OK;
}

int
condorSchedd__discoverDagRequirements(struct soap *soap,
                                      char *dag,
                                      struct condorSchedd__RequirementsAndStatus & result)
{
  return SOAP_FAULT;
}

///////////////////////////////////////////////////////////////////////////////
// TODO : This should move into daemonCore once we figure out how we wanna link
///////////////////////////////////////////////////////////////////////////////

int
condorCore__getPlatformString(struct soap *soap,
                              void *,
                              struct condorCore__StringAndStatus &result)
{
  result.message = CondorPlatform();
  result.status.code = 0;
  return SOAP_OK;
}

int
condorCore__getVersionString(struct soap *soap,
                             void *,
                             struct condorCore__StringAndStatus &result)
{
  result.message = CondorVersion();
  result.status.code = 0;
  return SOAP_OK;
}

int
condorCore__getInfoAd(struct soap *soap,
                      void *,
                      struct condorCore__ClassAdStructAndStatus & result)
{
  char* todd = "Todd A Tannenbaum";

  result.classAd.__size = 3;
  result.classAd.__ptr = (struct condorCore__ClassAdStructAttr *)soap_malloc(soap,3 * sizeof(struct condorCore__ClassAdStructAttr));

  result.classAd.__ptr[0].name = "Name";
  result.classAd.__ptr[0].type = STRING;
  result.classAd.__ptr[0].value = todd;

  result.classAd.__ptr[1].name = "Age";
  result.classAd.__ptr[1].type = INTEGER;
  result.classAd.__ptr[1].value = "35";
  int* age = (int*)soap_malloc(soap,sizeof(int));
  *age = 35;

  result.classAd.__ptr[2].name = "Friend";
  result.classAd.__ptr[2].type = STRING;
  result.classAd.__ptr[2].value = todd;

  result.status.code = 0;

  return SOAP_OK;
}
