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

#include "soap_scheddStub.h"
#include "condorSchedd.nsmap"

#include "schedd_api.h"

#include "../condor_c++_util/soap_helpers.cpp"

#include "qmgmt.h"
// XXX: There should be checks to see if the cluster/job Ids are valid
// in the transaction they are being used...


static int current_trans_id = 0;
static int trans_timer_id = -1;

extern Scheduler scheduler;

/* XXX: When we finally have multiple transactions it is important
        that each one has a "jobs" hashtable! */
template class HashTable<MyString, Job *>;
HashTable<MyString, Job *> jobs = HashTable<MyString, Job *>(1024, MyStringHash, rejectDuplicateKeys);

static bool
convert_FileInfoList_to_Array(struct soap * soap,
                              List<FileInfo> & list,
                              struct FileInfoArray & array)
{
  array.__size = list.Number();
  array.__ptr = (struct condor__FileInfo *) soap_malloc(soap, array.__size * sizeof(struct condor__FileInfo));

  if (NULL == array.__ptr) {
	  return false;
  }

  FileInfo *info;
  list.Rewind();
  for (int i = 0; list.Next(info); i++) {
		  /* It would be easier to use strdup, but we'd leak memory.
			 array.__ptr[i].name = strdup(info->name.GetCStr()); */
	array.__ptr[i].name = (char *) soap_malloc(soap, info->name.Length() * sizeof(char));
	if (NULL == array.__ptr[i].name) {
		return false;
	}
	strcpy(array.__ptr[i].name, info->name.GetCStr());
    array.__ptr[i].size = (int) info->size;
  }

  return true;
}

static bool null_transaction(const struct condor__Transaction & transaction)
{
  return !transaction.id;
}

static bool valid_transaction(const struct condor__Transaction & transaction)
{
  return current_trans_id == transaction.id;
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
		// XXX: Bug when key is popped off the stack? Does
		// HashTable::Insert() make a copy of the key?
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
extendTransaction(const struct condor__Transaction & transaction)
{
  if (!null_transaction(transaction) &&
      transaction.id == current_trans_id && // must be the current transaction
      trans_timer_id != -1) {

    if (transaction.duration < 1) {
      return 1;
    }

    daemonCore->Reset_Timer(trans_timer_id, transaction.duration);
	daemonCore->Only_Allow_Soap(transaction.duration);
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
condor__transtimeout()
{
  struct condor__abortTransactionResponse result;

  dprintf(D_ALWAYS,"SOAP in condor__transtimeout()\n");

  condor__Transaction transaction;
  transaction.id = current_trans_id;
  condor__abortTransaction(NULL, transaction, result);
  return TRUE;
}

int
condor__beginTransaction(struct soap *s,
                               int duration,
                               struct condor__beginTransactionResponse & result)
{
  if ( current_trans_id ) {
    // if there is an existing transaction, abort it.
    // TODO - support more than one active transaction!!!
    struct condor__abortTransactionResponse response;
    struct condor__Transaction transaction;
    transaction.id = current_trans_id;
    // XXX: handle errors
    condor__abortTransaction(s, transaction, response);
  }
  if ( duration < 1 ) {
    duration = 1;
  }

  trans_timer_id = daemonCore->Register_Timer(duration,
                                              (TimerHandler)&condor__transtimeout,
                                              "condor_transtimeout");
  daemonCore->Only_Allow_Soap(duration);
  current_trans_id = time(NULL);   // TODO : choose unique id - use time for now
  result.response.transaction.id = current_trans_id;
  result.response.transaction.duration = duration;
  result.response.status.code = SUCCESS;

  setQSock(NULL);	// Tell the qmgmt layer to allow anything -- that is, until
                        // we authenticate the client.

  BeginTransaction();

  dprintf(D_ALWAYS,"SOAP leaving condor__beginTransaction() id=%ld\n",result.response.transaction.id);

  return SOAP_OK;
}

int
condor__commitTransaction(struct soap *s,
                                struct condor__Transaction transaction,
                                struct condor__commitTransactionResponse & result )
{
  if (!valid_transaction(transaction) ||
      null_transaction(transaction)) {
    result.response.code = INVALIDTRANSACTION;
  } else {
    CommitTransaction();
    current_trans_id = 0;
    transaction.id = 0;
    if ( trans_timer_id != -1 ) {
      daemonCore->Cancel_Timer(trans_timer_id);
      trans_timer_id = -1;
    }
	daemonCore->Only_Allow_Soap(0);

    result.response.code = SUCCESS;
  }

	  //jobs.clear(); // XXX: Do the destructors get called?

  dprintf(D_ALWAYS,"SOAP leaving condor__commitTransaction() res=%d\n",result.response.code);
  return SOAP_OK;
}


int
condor__abortTransaction(struct soap *s,
                               struct condor__Transaction transaction,
                               struct condor__abortTransactionResponse & result )
{
  if (transaction.id && transaction.id == current_trans_id) {
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
	daemonCore->Only_Allow_Soap(0);
  }

  if (!valid_transaction(transaction) ||
      null_transaction(transaction)) {
    // TODO error - unrecognized transactionId
    result.response.code = INVALIDTRANSACTION;
  } else {
    result.response.code = SUCCESS;
  }

  dprintf(D_ALWAYS,"SOAP leaving condor__abortTransaction() res=%d\n",result.response.code);
  return SOAP_OK;
}


int
condor__extendTransaction(struct soap *s,
                                struct condor__Transaction transaction,
                                int duration,
                                struct condor__extendTransactionResponse & result )
{
  if (!valid_transaction(transaction) ||
      null_transaction(transaction)) {
    result.response.status.code = INVALIDTRANSACTION;
  } else {
    transaction.duration = duration;
    if (extendTransaction(transaction)) {
      result.response.status.code = FAIL;
    } else {
      result.response.transaction.id = transaction.id;
      result.response.transaction.duration = duration;

      result.response.status.code = SUCCESS;
    }
  }

  dprintf(D_ALWAYS,"SOAP leaving condor__extendTransaction() res=%d\n",result.response.status.code);
  return SOAP_OK;
}


int
condor__newCluster(struct soap *s,
                         struct condor__Transaction transaction,
                         struct condor__newClusterResponse & result)
{
  if (!valid_transaction(transaction) ||
      null_transaction(transaction)) {
    // TODO error - unrecognized transactionId
    result.response.status.code = INVALIDTRANSACTION;
  } else {
    extendTransaction(transaction);

    result.response.integer = NewCluster();
    if (result.response.integer == -1) {
      // TODO error case
      result.response.status.code = FAIL;
    } else {
      result.response.status.code = SUCCESS;
    }
  }

  dprintf(D_ALWAYS,"SOAP leaving condor__newCluster() res=%d\n",result.response.status.code);
  return SOAP_OK;
}


int
condor__removeCluster(struct soap *s,
                            struct condor__Transaction transaction,
                            int clusterId,
                            char* reason,
                            struct condor__removeClusterResponse & result)
{
    if ( !valid_transaction(transaction) &&
         !null_transaction(transaction) ) {
            // TODO error - unrecognized transactionId
        result.response.code = INVALIDTRANSACTION;
    } else {
        extendTransaction(transaction);

        MyString constraint;
        constraint.sprintf("%s==%d", ATTR_CLUSTER_ID, clusterId);
        if ( abortJobsByConstraint(constraint.GetCStr(), reason, transaction.id ? false : true) ) {
            if ( removeCluster(clusterId) ) {
                result.response.code = FAIL;
            } else {
                result.response.code = SUCCESS;
            }
        } else {
            result.response.code = FAIL;
        }
    }

    dprintf(D_ALWAYS,"SOAP leaving condor__removeCluster() res=%d\n",result.response.code);
    return SOAP_OK;
}


int
condor__newJob(struct soap *s,
                     struct condor__Transaction transaction,
                     int clusterId,
                     struct condor__newJobResponse & result)
{
  if (!valid_transaction(transaction) ||
      null_transaction(transaction)) {
    // TODO error - unrecognized transactionId
    result.response.status.code = INVALIDTRANSACTION;
  } else {
    extendTransaction(transaction);

    result.response.integer = NewProc(clusterId);
    if (result.response.integer == -1) {
      // TODO error case
      result.response.status.code = FAIL;
    } else {
      // Create a Job for this new job.
      Job *job = new Job(clusterId, result.response.integer);
      if (insertJob(clusterId, result.response.integer, job)) {
        result.response.status.code = FAIL;
      } else {
        result.response.status.code = SUCCESS;
      }
    }
  }

  dprintf(D_ALWAYS,"SOAP leaving condor__newJob() res=%d\n",result.response.status.code);
  return SOAP_OK;
}


int
condor__removeJob(struct soap *s,
                        struct condor__Transaction transaction,
                        int clusterId,
                        int jobId,
                        char* reason,
                        bool force_removal,
                        struct condor__removeJobResponse & result)
{
  // TODO --- do something w/ force_removal flag; it is ignored for now.

  if (!valid_transaction(transaction) &&
      !null_transaction(transaction)) {
    // TODO error - unrecognized transactionId
    result.response.code = INVALIDTRANSACTION;
  } else {
    extendTransaction(transaction);

    if (!abortJob(clusterId,jobId,reason,transaction.id ? false : true)) {
      result.response.code = FAIL;
    } else {
      if (removeJob(clusterId, jobId)) {
        result.response.code = FAIL;
      } else {
        result.response.code = SUCCESS;
      }
    }
  }

  dprintf(D_ALWAYS,"SOAP leaving condor__removeJob() res=%d\n",result.response.code);
  return SOAP_OK;
}


int
condor__holdJob(struct soap *s,
                      struct condor__Transaction transaction,
                      int clusterId,
                      int jobId,
                      char* reason,
                      bool email_user,
                      bool email_admin,
                      bool system_hold,
                      struct condor__holdJobResponse & result)
{
  result.response.code = SUCCESS;
  if (!valid_transaction(transaction) &&
      !null_transaction(transaction)) {
    // TODO error - unrecognized transactionId
    result.response.code = INVALIDTRANSACTION;
  } else {
    extendTransaction(transaction);

    if (!holdJob(clusterId,jobId,reason,transaction.id ? false : true,
                email_user, email_admin, system_hold)) {
      // TODO error - remove failed
      result.response.code = FAIL;
    } else {
      result.response.code = SUCCESS;
    }
  }

  dprintf(D_ALWAYS,"SOAP leaving condor__holdJob() res=%d\n",result.response.code);
  return SOAP_OK;
}


int
condor__releaseJob(struct soap *s,
                         struct condor__Transaction transaction,
                         int clusterId,
                         int jobId,
                         char* reason,
                         bool email_user,
                         bool email_admin,
                         struct condor__releaseJobResponse & result)
{
  if (!valid_transaction(transaction) &&
      !null_transaction(transaction)) {
    // TODO error - unrecognized transactionId
    result.response.code = INVALIDTRANSACTION;
  } else {
    extendTransaction(transaction);

    if (!releaseJob(clusterId,jobId,reason,transaction.id ? false : true,
                    email_user, email_admin)) {
      // TODO error - release failed
      result.response.code = FAIL;
    } else {
      result.response.code = SUCCESS;
    }
  }

  dprintf(D_ALWAYS,"SOAP leaving condor__releaseJob() res=%d\n",result.response.code);
  return SOAP_OK;
}


int
condor__submit(struct soap *s,
                     struct condor__Transaction transaction,
                     int clusterId,
                     int jobId,
                     struct ClassAdStruct * jobAd,
                     struct condor__submitResponse & result)
{
  if (!valid_transaction(transaction) ||
      null_transaction(transaction)) {
    // TODO error - unrecognized transactionId
    result.response.status.code = INVALIDTRANSACTION;
  } else {
    extendTransaction(transaction);

    Job *job;
    if (getJob(clusterId, jobId, job)) {
      result.response.status.code = UNKNOWNJOB;
    } else {
      ClassAd realJobAd;
      if (!convert_adStruct_to_ad(s, &realJobAd, jobAd)) {
        result.response.status.code = FAIL;
      } else {
        if (job->submit(*jobAd)) {
          result.response.status.code = FAIL;
        } else {
          result.response.status.code = SUCCESS;
        }
      }
    }
  }

  dprintf(D_ALWAYS,"SOAP leaving condor__submit() res=%d\n",result.response.status.code);
  return SOAP_OK;
}


int
condor__getJobAds(struct soap *s,
                        struct condor__Transaction transaction,
                        char *constraint,
                        struct condor__getJobAdsResponse & result )
{
  dprintf(D_ALWAYS,"SOAP entering condor__getJobAds() \n");

  if (!valid_transaction(transaction) &&
      !null_transaction(transaction)) {
    // TODO error - unrecognized transactionId
    result.response.status.code = INVALIDTRANSACTION;
  } else {
    extendTransaction(transaction);

    List<ClassAd> adList;
    ClassAd *ad = GetNextJobByConstraint(constraint, 1);
    while (ad) {
      adList.Append(ad);
      ad = GetNextJobByConstraint(constraint, 0);
    }

    // fill in our soap struct response
    if (!convert_adlist_to_adStructArray(s,&adList,&result.response.classAdArray) ) {
      dprintf(D_ALWAYS,"condor__getJobAds: convert_adlist_to_adStructArray failed!\n");

      result.response.status.code = FAIL;
    } else {
      result.response.status.code = SUCCESS;
    }
  }

  return SOAP_OK;
}


int
condor__getJobAd(struct soap *s,
                       struct condor__Transaction transaction,
                       int clusterId,
                       int jobId,
                       struct condor__getJobAdResponse & result )
{
  // TODO : deal with transaction consistency; currently, job ad is
  // invisible until a commit.  not very ACID compliant, is it? :(

  dprintf(D_ALWAYS,"SOAP entering condor__getJobAd() \n");

  if (!valid_transaction(transaction) &&
      !null_transaction(transaction)) {
    // TODO error - unrecognized transactionId
    result.response.status.code = INVALIDTRANSACTION;
  } else {
    extendTransaction(transaction);

    ClassAd *ad = GetJobAd(clusterId,jobId);
    if (!convert_ad_to_adStruct(s,ad,&result.response.classAd)) {
      dprintf(D_ALWAYS,"condor__getJobAds: convert_adlist_to_adStructArray failed!\n");

      result.response.status.code = FAIL;
    } else {
      result.response.status.code = SUCCESS;
    }
  }

  return SOAP_OK;
}

int
condor__declareFile(struct soap *soap,
                          struct condor__Transaction transaction,
                          int clusterId,
                          int jobId,
                          char * name,
                          int size,
                          enum condor__HashType hashType,
                          char * hash,
                          struct condor__declareFileResponse & result)
{
  dprintf(D_ALWAYS,"SOAP entering condor__declareFile() \n");

  if (!valid_transaction(transaction) ||
      null_transaction(transaction)) {
    // TODO error - unrecognized transactionId
    result.response.code = INVALIDTRANSACTION;
  } else {
    extendTransaction(transaction);

    Job *job;
    if (getJob(clusterId, jobId, job)) {
      result.response.code = UNKNOWNJOB;
    } else {
      if (job->declare_file(MyString(name), size)) {
        result.response.code = FAIL;
      } else {
        result.response.code = SUCCESS;
      }
    }
  }

  return SOAP_OK;
}

int
condor__sendFile(struct soap *soap,
                       struct condor__Transaction transaction,
                       int clusterId,
                       int jobId,
                       char * filename,
                       int offset,
                       struct xsd__base64Binary *data,
                       struct condor__sendFileResponse & result)
{
  dprintf(D_ALWAYS,"SOAP entering condor__sendFile() \n");

  if (!valid_transaction(transaction) ||
      null_transaction(transaction)) {
    // TODO error - unrecognized transactionId
    result.response.code = INVALIDTRANSACTION;
  } else {
    extendTransaction(transaction);

    Job *job;
    if (getJob(clusterId, jobId, job)) {
      result.response.code = INVALIDTRANSACTION;
    } else {
      if (0 == job->send_file(MyString(filename),
                              offset,
                              (char *) data->__ptr,
                              data->__size)) {
        result.response.code = SUCCESS;
      } else {
        result.response.code = FAIL;
      }
    }
  }

  return SOAP_OK;
}

int condor__getFile(struct soap *soap,
                          struct condor__Transaction transaction,
                          int clusterId,
                          int jobId,
                          char * name,
                          int offset,
                          int length,
                          struct condor__getFileResponse & result)
{
  dprintf(D_ALWAYS,"SOAP entering condor__getFile() \n");

  if (!valid_transaction(transaction) &&
      !null_transaction(transaction)) {
    // TODO error - unrecognized transactionId
    result.response.status.code = INVALIDTRANSACTION;
  } else {
    extendTransaction(transaction);

    Job *job;
    if (getJob(clusterId, jobId, job)) {
      result.response.status.code = UNKNOWNJOB;
    } else {
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
    }
  }

  return SOAP_OK;
}

int condor__closeSpool(struct soap *soap,
                             struct condor__Transaction transaction,
                             xsd__int clusterId,
                             xsd__int jobId,
                             struct condor__closeSpoolResponse & result)
{
  dprintf(D_ALWAYS,"SOAP entering condor__closeSpool() \n");

  if (!valid_transaction(transaction) &&
      !null_transaction(transaction)) {
    // TODO error - unrecognized transactionId
    result.response.code = INVALIDTRANSACTION;
  } else {
    extendTransaction(transaction);

    if (SetAttribute(clusterId, jobId, "FilesRetrieved", "TRUE")) {
      result.response.code = FAIL;
    } else {
      result.response.code = SUCCESS;
    }
  }

  return SOAP_OK;
}

int
condor__listSpool(struct soap * soap,
                        struct condor__Transaction transaction,
                        int clusterId,
                        int jobId,
                        struct condor__listSpoolResponse & result)
{
	dprintf(D_ALWAYS,"SOAP entering condor__listSpool() \n");

	if (!valid_transaction(transaction) &&
		!null_transaction(transaction)) {
// TODO error - unrecognized transactionId
		result.response.status.code = INVALIDTRANSACTION;
	} else {
		extendTransaction(transaction);

		Job *job;
		if (getJob(clusterId, jobId, job)) {
			result.response.status.code = UNKNOWNJOB;
			dprintf(D_ALWAYS, "listSpool: UNKNOWNJOB\n");
		} else {
			List<FileInfo> files;
			int code;
			if (code = job->get_spool_list(files)) {
				result.response.status.code = FAIL;
				dprintf(D_ALWAYS, "listSpool: get_spool_list FAILED -- %d\n", code);
			} else {
				if (convert_FileInfoList_to_Array(soap, files, result.response.info)) {
					result.response.status.code = SUCCESS;
					dprintf(D_ALWAYS, "listSpool: SUCCESS\n");
				} else {
					result.response.status.code = FAIL;
					dprintf(D_ALWAYS, "listSpool: convert_FileInfoList_to_Array FAILED\n");
				}
			}
		}
	}

	return SOAP_OK;
}

int
condor__requestReschedule(struct soap *soap,
						  void *,
						  struct condor__requestRescheduleResponse & result)
{
	if (Reschedule()) {
		result.response.code = SUCCESS;
	} else {
		result.response.code = FAIL;
	}

	return SOAP_OK;
}

int
condor__discoverJobRequirements(struct soap *soap,
                                      struct ClassAdStruct * jobAd,
                                      struct condor__discoverJobRequirementsResponse & result)
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
    (condor__Requirement *) soap_malloc(soap, result.response.requirements.__size * sizeof(condor__Requirement));
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
condor__createJobTemplate(struct soap *soap,
                                int clusterId,
                                int jobId,
                                char * owner,
                                condor__UniverseType universe,
                                char * cmd,
                                char * args,
                                char * requirements,
                                struct condor__createJobTemplateResponse & result)
{
  MyString attribute;

  ClassAd *job = new ClassAd();

  job->SetMyTypeName(JOB_ADTYPE);
  job->SetTargetTypeName(STARTD_ADTYPE);

  attribute = MyString(ATTR_CLUSTER_ID) + " = " + clusterId;
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_PROC_ID) + " = " + jobId;
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_Q_DATE) + " = " + ((int) time((time_t *) 0));
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_COMPLETION_DATE) + " = 0";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_OWNER) + " = \"" + owner + "\"";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_REMOTE_WALL_CLOCK) + " = 0.0";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_LOCAL_USER_CPU) + " = 0.0";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_LOCAL_SYS_CPU) + " = 0.0";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_REMOTE_USER_CPU) + " = 0.0";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_REMOTE_SYS_CPU) + " = 0.0";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_EXIT_STATUS) + " = 0";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_NUM_CKPTS) + " = 0";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_NUM_RESTARTS) + " = 0";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_NUM_SYSTEM_HOLDS) + " = 0";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_COMMITTED_TIME) + " = 0";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_TOTAL_SUSPENSIONS) + " = 0";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_LAST_SUSPENSION_TIME) + " = 0";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_CUMULATIVE_SUSPENSION_TIME) + " = 0";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_ON_EXIT_BY_SIGNAL) + " = FALSE";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_ROOT_DIR) + " = \"/\"";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_UNIVERSE) + " = " + universe;
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_CMD) + " = \"" + cmd + "\"";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_MIN_HOSTS) + " = 1";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_MAX_HOSTS) + " = 1";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_CURRENT_HOSTS) + " = 0";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_WANT_REMOTE_SYSCALLS) + " = FALSE";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_WANT_CHECKPOINT) + " = FALSE";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_STATUS) + " = 1";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_ENTERED_CURRENT_STATE) + " = " + (((int) time((time_t *) 0)) / 1000);
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_PRIO) + " = 0";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_ENVIRONMENT) + " = \"\"";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_NOTIFICATION) + " = 2";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_KILL_SIG) + " = \"SIGTERM\"";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_IMAGE_SIZE) + " = 0";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_INPUT) + " = \"/dev/null\"";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_TRANSFER_INPUT) + " = FALSE";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_OUTPUT) + " = \"/dev/null\"";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_ERROR) + " = \"/dev/null\"";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_BUFFER_SIZE) + " = " + (512 * 1024);
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_BUFFER_BLOCK_SIZE) + " = " + (32 * 1024);
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_SHOULD_TRANSFER_FILES) + " = TRUE";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_TRANSFER_FILES) + " = \"ONEXIT\"";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_WHEN_TO_TRANSFER_OUTPUT) + " = \"ON_EXIT\"";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_REQUIREMENTS) + " = (";
  if (requirements) {
    attribute = attribute + requirements;
  } else {
    attribute = attribute + " = TRUE";
  }
  attribute = attribute + ")";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_PERIODIC_HOLD_CHECK) + " = FALSE";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_PERIODIC_RELEASE_CHECK) + " = FALSE";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_PERIODIC_REMOVE_CHECK) + " = FALSE";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_ON_EXIT_HOLD_CHECK) + " = FALSE";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_ON_EXIT_REMOVE_CHECK) + " = TRUE";
  job->Insert(attribute.GetCStr());

  attribute = MyString("FilesRetrieved") + " = FALSE";
  job->Insert(attribute.GetCStr());

  attribute = MyString(ATTR_JOB_LEAVE_IN_QUEUE) + " = FilesRetrieved=?=FALSE";
  char *soapLeaveInQueue = param("SOAP_LEAVE_IN_QUEUE");
  if (soapLeaveInQueue) {
    attribute = attribute + " && (" + soapLeaveInQueue + ")";

		//free(soapLeaveInQueue);
  }
  // XXX: This is recoverable!
  assert(job->Insert(attribute.GetCStr()));

  attribute = MyString(ATTR_JOB_ARGUMENTS) + " = \"" + args + "\"";
  job->Insert(attribute.GetCStr());

  // Need more attributes! Skim submit.C more.

  result.response.status.code = SUCCESS;
  convert_ad_to_adStruct(soap, job, &result.response.classAd);

	  //delete job;

  return SOAP_OK;
}

bool
Reschedule()
{
	// XXX: Abstract this, it was stolen from Scheduler::reschedule_negotiator!

	scheduler.timeout();		// update the central manager now

	dprintf( D_ALWAYS, "Called Reschedule()\n" );

	scheduler.sendReschedule();

	scheduler.StartSchedUniverseJobs();

	return true;
}

///////////////////////////////////////////////////////////////////////////////
// TODO : This should move into daemonCore once we figure out how we wanna link
///////////////////////////////////////////////////////////////////////////////

#include "../condor_daemon_core.V6/soap_daemon_core.cpp"
