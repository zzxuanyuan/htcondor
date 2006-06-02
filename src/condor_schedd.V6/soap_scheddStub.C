/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include "condor_classad.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

// Things to include for the stubs
#include "condor_version.h"
#include "condor_attributes.h"
#include "scheduler.h"
#include "condor_qmgr.h"
#include "CondorError.h"
#include "MyString.h"
#include "internet.h"
#include "condor_perms.h"
#include "classad_helpers.h"
#include "log_transaction.h"

#include "condor_ckpt_name.h"
#include "condor_config.h"

#include "loose_file_transfer.h"

#include "soap_scheddStub.h"
#include "condorSchedd.nsmap"

#include "schedd_api.h"

#include "../condor_c++_util/soap_helpers.cpp"

#include "qmgmt.h"

static int current_trans_id = 0;
static int trans_timer_id = -1;

extern Scheduler scheduler;

static ScheddTransactionManager transactionManager;


/*************************************
	HELPER FUNCTIONS
************************************/
bool
Reschedule()
{
		// XXX: Abstract this, it was stolen from Scheduler::reschedule_negotiator!

	scheduler.timeout();		// update the central manager now

	dprintf(D_FULLDEBUG, "Called Reschedule()\n");

	scheduler.sendReschedule();

	scheduler.StartLocalJobs();

	return true;
}

static bool
verify(DCpermission perm,
	   const struct soap *soap,
	   struct condor__Status *status)
{
	if (soap->user) {
		if (daemonCore->Verify(perm,
							   &soap->peer,
							   (char *) soap->user) == USER_AUTH_FAILURE) {
			dprintf(D_FULLDEBUG,
					"SOAP call rejected, no permission for user %s\n",
					(char *) soap->user);

			status->code = FAIL;
			status->message = "Permission denied";

			return false;
		}
	}

	return true;
}

bool
verify_owner(int clusterId, int jobId, char *owner)
{
	ClassAd *ad = NULL;

	if (!(ad = GetJobAd(clusterId, jobId))) {
		return false;
	}

	return OwnerCheck2(ad, owner);
}

static bool
convert_FileInfoList_to_Array(struct soap * soap,
                              List<FileInfo> & list,
                              struct condor__FileInfoArray & array)
{
	array.__size = list.Number();
	if (0 == array.__size) {
		array.__ptr = NULL;
	} else {
		array.__ptr =
			(struct condor__FileInfo *)
			soap_malloc(soap, array.__size * sizeof(struct condor__FileInfo));
		ASSERT(array.__ptr);

		FileInfo *info;
		list.Rewind();
		for (int i = 0; list.Next(info); i++) {
			array.__ptr[i].name =
				(char *) soap_malloc(soap, strlen(info->name) + 1);
			ASSERT(array.__ptr[i].name);
			strcpy(array.__ptr[i].name, info->name);
			array.__ptr[i].size = info->size;
		}
	}

	return true;
}

static
bool
null_transaction(const struct condor__Transaction *transaction)
{
	return !transaction || !transaction->id;
}

static
int
extendTransaction(const struct condor__Transaction *transaction)
{
	if (!null_transaction(transaction) &&
		transaction->id == current_trans_id &&
		trans_timer_id != -1) {

		if (transaction->duration < 1) {
			return 1;
		}

		daemonCore->Reset_Timer(trans_timer_id, transaction->duration);
		daemonCore->Only_Allow_Soap(transaction->duration);
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

	dprintf(D_FULLDEBUG, "SOAP in condor__transtimeout()\n");

	condor__Transaction transaction;
	transaction.id = current_trans_id;
	condor__abortTransaction(NULL, transaction, result);
	return TRUE;
}


/*************************************
	SOAP STUBS
************************************/

int
condor__beginTransaction(struct soap *soap,
						 int duration,
						 struct condor__beginTransactionResponse & result)
{

	dprintf(D_FULLDEBUG, "SOAP in beginTransaction()\n");

	if (!verify(WRITE, soap, &result.response.status)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}

	if ( current_trans_id ) {
			// if there is an existing transaction, deny the request
			// XXX TODO - support more than one active transaction!!!

		result.response.status.code = FAIL;
		result.response.status.message =
			"Maximum number of transactions reached";

		dprintf(D_FULLDEBUG,
				"SOAP denied new transaction in condor__beginTransaction()\n");

		return SOAP_OK;
	}

	int max = param_integer("MAX_SOAP_TRANSACTION_DURATION", -1);
	if (0 < max) {
		duration = duration > max ? max : duration;
	}

	if ( duration < 1 ) {
		duration = 1;
	}

	trans_timer_id =
		daemonCore->Register_Timer(duration,
								   (TimerHandler)&condor__transtimeout,
								   "condor_transtimeout");
	daemonCore->Only_Allow_Soap(duration);

	int id;
	ScheddTransaction *transaction;
	char *owner = NULL; // Get OWNER from X509 cert...
	if (transactionManager.createTransaction(owner,
											 id,
											 transaction)) {
		result.response.status.code = FAIL;
		result.response.status.message = "Unable to create transaction";
	} else {
		transaction->duration = duration;

		result.response.transaction.id = id;
		result.response.transaction.duration = transaction->duration;
		result.response.status.code = SUCCESS;
		result.response.status.message = "Success";
			// XXX: Todd, what does this do?
			// Tell the qmgmt layer to allow anything -- that is, until we
			// authenticate the client.
		setQSock(NULL);

		if (transaction->begin()) {
			result.response.status.code = FAIL;
			result.response.status.message = "Unable to begin transaction";
		}
	}

	dprintf(D_FULLDEBUG,
			"SOAP leaving condor__beginTransaction() id=%u\n",
			result.response.transaction.id);

	return SOAP_OK;
}

int
condor__commitTransaction(struct soap *soap,
						  struct condor__Transaction transaction,
						  struct condor__commitTransactionResponse & result)
{

	dprintf(D_FULLDEBUG,
			"SOAP entered condor__commitTransaction(), transaction: %u\n",
			transaction.id);

	if (!verify(WRITE, soap, &result.response)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}

	ScheddTransaction *entry;
	if (transactionManager.getTransaction(transaction.id, entry)) {
		result.response.code = INVALIDTRANSACTION;
		result.response.message = "Invalid transaction";
	} else {
		entry->commit();

			// XXX: This should all go away
		current_trans_id = 0;
		if ( trans_timer_id != -1 ) {
			daemonCore->Cancel_Timer(trans_timer_id);
			trans_timer_id = -1;
		}
		daemonCore->Only_Allow_Soap(0);

		result.response.code = SUCCESS;
		result.response.message = "Success";
	}

	dprintf(D_FULLDEBUG,
			"SOAP leaving condor__commitTransaction() res=%d\n",
			result.response.code);
	return SOAP_OK;
}


int
condor__abortTransaction(struct soap *soap,
						 struct condor__Transaction transaction,
						 struct condor__abortTransactionResponse & result )
{
	dprintf(D_FULLDEBUG,
			"SOAP entered condor__abortTransaction(), transaction: %u\n",
			transaction.id);

	if (!verify(WRITE, soap, &result.response)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}

	ScheddTransaction *entry;
	if (transactionManager.getTransaction(transaction.id, entry)) {
		result.response.code = INVALIDTRANSACTION;
		result.response.message = "Invalid transaction";
	} else {
		entry->abort();

		if (transactionManager.destroyTransaction(transaction.id)) {
			dprintf(D_ALWAYS, "condor__abortTransaction cleanup failed\n");
		}

			// XXX: This should go away
		current_trans_id = 0;
		if (trans_timer_id != -1) {
			daemonCore->Cancel_Timer(trans_timer_id);
			trans_timer_id = -1;
		}
		daemonCore->Only_Allow_Soap(0);

		result.response.code = SUCCESS;
		result.response.message = "Success";
	}

	dprintf(D_FULLDEBUG,
			"SOAP leaving condor__abortTransaction() res=%d\n",
			result.response.code);
	return SOAP_OK;
}


int
condor__extendTransaction(struct soap *soap,
						  struct condor__Transaction transaction,
						  int duration,
						  struct condor__extendTransactionResponse & result )
{
	dprintf(D_FULLDEBUG,
			"SOAP entered condor__extendTransaction(), transaction: %u\n",
			transaction.id);

	if (!verify(WRITE, soap, &result.response.status)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}

	ScheddTransaction *entry;
	if (transactionManager.getTransaction(transaction.id, entry)) {
		result.response.status.code = INVALIDTRANSACTION;
		result.response.status.message = "Invalid transaction";
	} else {
		entry->duration = duration;
		if (extendTransaction(&transaction)) {
			result.response.status.code = FAIL;
 			result.response.status.message = "Could not extend transaction";
		} else {
			result.response.transaction.id = transaction.id;
			result.response.transaction.duration = entry->duration;

			result.response.status.code = SUCCESS;
			result.response.status.message = "Success";
		}
	}

	dprintf(D_FULLDEBUG,
			"SOAP leaving condor__extendTransaction() res=%d\n",
			result.response.status.code);
	return SOAP_OK;
}


int
condor__newCluster(struct soap *soap,
				   struct condor__Transaction transaction,
				   struct condor__newClusterResponse & result)
{

	dprintf(D_FULLDEBUG,
			"SOAP entered condor__newCluster(), transaction: %u\n",
			transaction.id);

	if (!verify(WRITE, soap, &result.response.status)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}

	ScheddTransaction *entry;
	if (transactionManager.getTransaction(transaction.id, entry)) {
		result.response.status.code = INVALIDTRANSACTION;
		result.response.status.message = "Invalid transaction";
	} else {
		extendTransaction(&transaction);

		int id;
		if (entry->newCluster(id)) {
			result.response.status.code = FAIL;
			result.response.status.message = "Could not create new cluster";
		} else {
			result.response.integer = id;

			result.response.status.code = SUCCESS;
			result.response.status.message = "Success";
		}
	}

	dprintf(D_FULLDEBUG,
			"SOAP leaving condor__newCluster() res=%d\n",
			result.response.status.code);
	return SOAP_OK;
}


int
condor__removeCluster(struct soap *soap,
					  struct condor__Transaction *transaction,
					  int clusterId,
					  char* reason,
					  struct condor__removeClusterResponse & result)
{
	dprintf(D_FULLDEBUG,
			"SOAP entered condor__removeCluster(), transaction: %u\n",
			transaction->id);

	if (!verify(WRITE, soap, &result.response)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}

	ScheddTransaction *entry;
	if (!null_transaction(transaction) &&
		transactionManager.getTransaction(transaction->id, entry)) {
		result.response.code = INVALIDTRANSACTION;
		result.response.message = "Invalid transaction";
    } else {
        extendTransaction(transaction);

        MyString constraint;
        constraint.sprintf("%s==%d", ATTR_CLUSTER_ID, clusterId);

			// NOTE: There is an assumption here that the owner of the
			// first job in a cluster is the owner of all the jobs in
			// the cluster!
		if (soap->user) {
			ClassAd *an_ad;
			int jobId;
			if (!(an_ad = GetNextJobByConstraint(constraint.GetCStr(), 1))) {
					// Nothing to remove, this is strange
				result.response.code = FAIL;
				result.response.message = "Cluster not found";

				return SOAP_OK;
			}

			if (!an_ad->LookupInteger(ATTR_PROC_ID, jobId)) {
				result.response.code = FAIL;
				result.response.message = "Owner not verifiable";

				return SOAP_OK;
			}

			if (!verify_owner(clusterId, jobId, (char *) soap->user)) {
				dprintf(D_FULLDEBUG,
						"Owner check failed for %s\n",
						(char *) soap->user);

				result.response.code = FAIL;
				result.response.message = "Not cluster owner";

				return SOAP_OK;
			}
		}

		if (!null_transaction(transaction) || // NullObjectPattern
											  // would be better
			entry->removeCluster(clusterId)) {
			result.response.code = FAIL;
			result.response.message = "Failed to cleanup cluster";
		} else {
				// XXX: This should be done within the scheddTransaction's
				// transaction
			if (abortJobsByConstraint(constraint.GetCStr(),
									  reason,
									  transaction->id ? false : true)) {
				result.response.code = FAIL;
				result.response.message = "Failed to abort jobs in the cluster";
			} else {
				result.response.code = SUCCESS;
				result.response.message = "Success";			
			}
		}
	}

    dprintf(D_FULLDEBUG,
			"SOAP leaving condor__removeCluster() res=%d\n",
			result.response.code);
    return SOAP_OK;
}


int
condor__newJob(struct soap *soap,
			   struct condor__Transaction transaction,
			   int clusterId,
			   struct condor__newJobResponse & result)
{
	dprintf(D_FULLDEBUG,
			"SOAP entered condor__newJob(), transaction: %u\n",
			transaction.id);

	if (!verify(WRITE, soap, &result.response.status)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}

	ScheddTransaction *entry;
	if (transactionManager.getTransaction(transaction.id, entry)) {
		result.response.status.code = INVALIDTRANSACTION;
		result.response.status.message = "Invalid transaction";
	} else {
		extendTransaction(&transaction);

		int id;
		CondorError errstack;
		switch (entry->newJob(clusterId, id, errstack)) {
		case 0:
			result.response.integer = id;
			result.response.status.code = SUCCESS;
			result.response.status.message = "Success";
			break;
		case -1:
			result.response.status.code = FAIL;
			result.response.status.message = "Could not create new job";
		case -2:
			result.response.status.code =
				(condor__StatusCode) errstack.code();
			result.response.status.message =
				soap_strdup(soap, errstack.message());
		case -3:
			result.response.status.code = FAIL;
			result.response.status.message = "Could not record new job";
		default:
			result.response.status.code = FAIL;
			result.response.status.message = "Unknown error";
			break;
		} 
	}

	dprintf(D_FULLDEBUG,
			"SOAP leaving condor__newJob() res=%d\n",
			result.response.status.code);
	return SOAP_OK;
}


int
condor__removeJob(struct soap *soap,
				  struct condor__Transaction *transaction,
				  int clusterId,
				  int jobId,
				  char* reason,
				  bool force_removal,
				  struct condor__removeJobResponse & result)
{
		// TODO --- do something w/ force_removal flag; it is ignored for now.
	dprintf(D_FULLDEBUG,
			"SOAP entered condor__removeJob(), transaction: %u\n",
			transaction->id);

	if (!verify(WRITE, soap, &result.response)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}

	if (soap->user) {
		if (!verify_owner(clusterId, jobId, (char *) soap->user)) {
			dprintf(D_FULLDEBUG,
					"Owner check failed for %s\n",
					(char *) soap->user);

			result.response.code = FAIL;
			result.response.message = "Not job owner";

			return SOAP_OK;
		}
	}

	ScheddTransaction *entry;
	if (!null_transaction(transaction) &&
		transactionManager.getTransaction(transaction->id, entry)) {
		result.response.code = INVALIDTRANSACTION;
		result.response.message = "Invalid transaction.";
	} else {
		extendTransaction(transaction);

			// XXX: This should be done within the scheddTransaction's
			// transaction
		PROC_ID id; id.cluster = clusterId; id.proc = jobId;
		if (!null_transaction(transaction) || // NullObjectPattern
											  // would be better
			entry->removeJob(id)) {
				// XXX: Problem: Jobs may be aborted but still exist in memory
			result.response.code = FAIL;
			result.response.message = "Failed to cleanup cluster";
		} else {
			if (!abortJob(clusterId,
						  jobId,
						  reason,
						  transaction->id ? false : true)) {
				result.response.code = FAIL;
				result.response.message = "Failed to abort job";
			} else {
				result.response.code = SUCCESS;
				result.response.message = "Success";
			}
		}
	}

	dprintf(D_FULLDEBUG,
			"SOAP leaving condor__removeJob() res=%d\n",
			result.response.code);
	return SOAP_OK;
}


int
condor__holdJob(struct soap *soap,
				struct condor__Transaction *transaction,
				int clusterId,
				int jobId,
				char* reason,
				bool email_user,
				bool email_admin,
				bool system_hold,
				struct condor__holdJobResponse & result)
{
	dprintf(D_FULLDEBUG,
			"SOAP entered condor__holdJob(), transaction: %u\n",
			transaction->id);

	if (!verify(WRITE, soap, &result.response)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}

	if (soap->user) {
		if (!verify_owner(clusterId, jobId, (char *) soap->user)) {
			dprintf(D_FULLDEBUG,
					"Owner check failed for %s\n",
					(char *) soap->user);

			result.response.code = FAIL;
			result.response.message = "Not job owner";

			return SOAP_OK;
		}
	}

	ScheddTransaction *entry;
	if (!null_transaction(transaction) &&
		transactionManager.getTransaction(transaction->id, entry)) {
		result.response.code = INVALIDTRANSACTION;
		result.response.message = "Invalid transaction.";
	} else {
		extendTransaction(transaction);

			// XXX: ScheddTransaction needs a HoldJob...
		if (!holdJob(clusterId,jobId,reason,transaction->id ? false : true,
					 true, email_user, email_admin, system_hold)) {
			result.response.code = FAIL;
			result.response.message = "Failed to hold job.";
		} else {
			result.response.code = SUCCESS;
			result.response.message = "Success";
		}
	}

	dprintf(D_FULLDEBUG,
			"SOAP leaving condor__holdJob() res=%d\n",
			result.response.code);
	return SOAP_OK;
}


int
condor__releaseJob(struct soap *soap,
				   struct condor__Transaction *transaction,
				   int clusterId,
				   int jobId,
				   char* reason,
				   bool email_user,
				   bool email_admin,
				   struct condor__releaseJobResponse & result)
{
	dprintf(D_FULLDEBUG,
			"SOAP entered condor__releaseJob(), transaction: %u\n",
			transaction->id);

	if (!verify(WRITE, soap, &result.response)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}

	if (soap->user) {
		if (!verify_owner(clusterId, jobId, (char *) soap->user)) {
			dprintf(D_FULLDEBUG,
					"Owner check failed for %s\n",
					(char *) soap->user);

			result.response.code = FAIL;
			result.response.message = "Not job owner";

			return SOAP_OK;
		}
	}

	ScheddTransaction *entry;
	if (!null_transaction(transaction) &&
		transactionManager.getTransaction(transaction->id, entry)) {
		result.response.code = INVALIDTRANSACTION;
		result.response.message = "Invalid transaction.";
	} else {
		extendTransaction(transaction);

			// XXX: ScheddTransaction needs a RelaseJob...
		if (!releaseJob(clusterId,jobId,reason,transaction->id ? false : true,
						email_user, email_admin)) {
			result.response.code = FAIL;
			result.response.message = "Failed to release job.";
		} else {
			result.response.code = SUCCESS;
			result.response.message = "Success";
		}
	}

	dprintf(D_FULLDEBUG,
			"SOAP leaving condor__releaseJob() res=%d\n",
			result.response.code);
	return SOAP_OK;
}


int
condor__submit(struct soap *soap,
			   struct condor__Transaction transaction,
			   int clusterId,
			   int jobId,
			   struct condor__ClassAdStruct * jobAd,
			   struct condor__submitResponse & result)
{
	dprintf(D_FULLDEBUG,
			"SOAP entered condor__submit(), transaction: %u\n",
			transaction.id);
	
	if (!verify(WRITE, soap, &result.response.status)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}

	ScheddTransaction *entry;
	if (transactionManager.getTransaction(transaction.id, entry)) {
		result.response.status.code = INVALIDTRANSACTION;
		result.response.status.message = "Invalid transaction";
	} else {
		extendTransaction(&transaction);

			// authorized if no owner was recorded (i.e. no
			// authentication) or if there is an owner and it matches
			// the authenticated user
		bool authorized = !entry->getOwner() || !strcmp(entry->getOwner(),
														(char *) soap->user);

		if (!authorized) {
			result.response.status.code = FAIL; // XXX: need new error?
			result.response.status.message = "Not authorized";
		} else {
			Job *job;
			PROC_ID id; id.cluster = clusterId; id.proc = jobId;
			if (entry->getJob(id, job)) {
				result.response.status.code = UNKNOWNJOB;
				result.response.status.message = "Unknown cluster or job";
			} else {
					// If the client is authenticated then ignore the
					// ATTR_OWNER attribute it specified and calculate
					// the proper value
					// NOTICE: The test on soap->user only works
					// because soap_core properly sets it to the
					// authenticated users, if one exists
				if (soap->user) {
					for (int i = 0; i < jobAd->__size; i++) {
						if (0 == strcmp(jobAd->__ptr[i].name, ATTR_OWNER)) {
								// No need to free the value because
								// it will be freed when the soap
								// object is freed
							jobAd->__ptr[i].value =
								soap_strdup(soap, (char *) soap->user);

								// WARNING: Don't break out of this
								// loop, there could be multiple
								// ATTR_OWNER attributes!
						}
					}
				}
					// There is certainly some trust going on here, if
					// the client does not send a useful ClassAd we do
					// not care. It would be nicer if clients could
					// not so easily screw themselves.
				CondorError errstack;
				if (job->submit(*jobAd, errstack)) {
					result.response.status.code =
						(condor__StatusCode) errstack.code();
					result.response.status.message =
						(char *)
						soap_malloc(soap, strlen(errstack.message()) + 1);
					strcpy(result.response.status.message,
						   errstack.message());
				} else {
					result.response.status.code = SUCCESS;
					result.response.status.message = "Success";
				}
			}
		}
	}

	dprintf(D_FULLDEBUG,
			"SOAP leaving condor__submit() res=%d\n",
			result.response.status.code);
	return SOAP_OK;
}


int
condor__getJobAds(struct soap *soap,
				  struct condor__Transaction *transaction,
				  char *constraint,
				  struct condor__getJobAdsResponse & result )
{
	dprintf(D_FULLDEBUG,
			"SOAP entered condor__getJobAds(), transaction: %u\n",
			transaction->id);

	if (!verify(READ, soap, &result.response.status)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}

	ScheddTransaction *entry;
	if (!null_transaction(transaction) &&
		transactionManager.getTransaction(transaction->id, entry)) {
		result.response.status.code = INVALIDTRANSACTION;
		result.response.status.message = "Invalid transaction";
	} else {
		extendTransaction(transaction);

		List<ClassAd> adList;
		if (null_transaction(transaction)) {
				// XXX: Duplicate code with ScheddTransaction::queryJobAds
			ClassAd *ad = GetNextJobByConstraint(constraint, 1);
			while (ad) {
				adList.Append(ad);
				ad = GetNextJobByConstraint(constraint, 0);
			}
		} else {
			if (entry->queryJobAds(constraint, adList)) {
				result.response.status.code = FAIL;
				result.response.status.message = "Query failed";
			}
		}

			// fill in our soap struct response
		if (!convert_adlist_to_adStructArray(soap,
											 &adList,
											 &result.response.classAdArray) ) {
			dprintf(D_FULLDEBUG,
					"condor__getJobAds: adlist to adStructArray failed!\n");

			result.response.status.code = FAIL;
			result.response.status.message = "Failed to serialize job ads.";
		} else {
			result.response.status.code = SUCCESS;
			result.response.status.message = "Success";
		}
	}

	return SOAP_OK;
}


int
condor__getJobAd(struct soap *soap,
				 struct condor__Transaction *transaction,
				 int clusterId,
				 int jobId,
				 struct condor__getJobAdResponse & result )
{
		// TODO : deal with transaction consistency; currently, job ad is
		// invisible until a commit.  not very ACID compliant, is it? :(

	dprintf(D_FULLDEBUG,
			"SOAP entered condor__getJobAd(), transaction: %u\n",
			transaction->id);
	
	if (!verify(READ, soap, &result.response.status)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}

	ScheddTransaction *entry;
	if (!null_transaction(transaction) &&
		transactionManager.getTransaction(transaction->id, entry)) {
		result.response.status.code = INVALIDTRANSACTION;
		result.response.status.message = "Invalid transaction.";
	} else {
		extendTransaction(transaction);

		ClassAd *ad;
		if (null_transaction(transaction)) {
				// XXX: Duplicate code with ScheddTransaction::queryJobAd
			ad = GetJobAd(clusterId, jobId);
		} else {
			PROC_ID id; id.cluster = clusterId; id.proc = jobId;
			if (entry->queryJobAd(id, ad)) {
				result.response.status.code = FAIL;
				result.response.status.message = "Query failed";
			}
		}

		if (!ad) {
			result.response.status.code = UNKNOWNJOB;
			result.response.status.message = "Specified job is unknown";
		} else {
			if (!convert_ad_to_adStruct(soap,
										ad,
										&result.response.classAd,
										false)) {
				dprintf(D_FULLDEBUG,
						"condor__getJobAd: ad to adStructArray failed!\n");

				result.response.status.code = FAIL;
				result.response.status.message = "Failed to serialize job ad";
			} else {
				result.response.status.code = SUCCESS;
				result.response.status.message = "Success";
			}
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
	dprintf(D_FULLDEBUG,
			"SOAP entered condor__declareFile(), transaction: %u\n",
			transaction.id);

	if (!verify(WRITE, soap, &result.response)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}

	ScheddTransaction *entry;
	if (transactionManager.getTransaction(transaction.id, entry)) {
		result.response.code = INVALIDTRANSACTION;
		result.response.message = "Invalid transaction.";
	} else {
		extendTransaction(&transaction);

		Job *job;
		PROC_ID id; id.cluster = clusterId; id.proc = jobId;
		if (entry->getJob(id, job)) {
			result.response.code = UNKNOWNJOB;
			result.response.message = "Unknown job";
		} else {
			CondorError errstack;
			if (job->declare_file(MyString(name), size, errstack)) {
				result.response.code =
					(condor__StatusCode) errstack.code();
				result.response.message =
					(char *) soap_malloc(soap, strlen(errstack.message()) + 1);
				strcpy(result.response.message,
					   errstack.message());
			} else {
				result.response.code = SUCCESS;
				result.response.message = "Success";
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
	dprintf(D_FULLDEBUG,
			"SOAP entered condor__sendFile(), transaction: %u\n",
			transaction.id);
	
	if (!verify(WRITE, soap, &result.response)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}
	ScheddTransaction *entry;
	if (transactionManager.getTransaction(transaction.id, entry)) {
		result.response.code = INVALIDTRANSACTION;
		result.response.message = "Invalid transaction.";
	} else {
		extendTransaction(&transaction);

		Job *job;
		PROC_ID id; id.cluster = clusterId; id.proc = jobId;
		if (entry->getJob(id, job)) {
			result.response.code = UNKNOWNJOB;
			result.response.message = "Unknown job";
		} else {
			CondorError errstack;
			char *data_buffer = NULL;
			int data_size = 0;
			if (NULL != data) {
				data_buffer = (char *) data->__ptr;
				data_size = data->__size;
			}
			if (0 == job->put_file(MyString(filename),
								   offset,
								   data_buffer,
								   data_size,
								   errstack)) {
				result.response.code = SUCCESS;
				result.response.message = "Success";
			} else {
				result.response.code =
					(condor__StatusCode) errstack.code();
				result.response.message =
					(char *) soap_malloc(soap, strlen(errstack.message()) + 1);
				strcpy(result.response.message,
					   errstack.message());
			}
		}
	}

	return SOAP_OK;
}

int condor__getFile(struct soap *soap,
					struct condor__Transaction *transaction,
					int clusterId,
					int jobId,
					char * name,
					int offset,
					int length,
					struct condor__getFileResponse & result)
{
	dprintf(D_FULLDEBUG,
			"SOAP entered condor__getFile(), transaction: %u\n",
			transaction->id);
	
	if (!verify(READ, soap, &result.response.status)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}

	if (soap->user) {
		if (!verify_owner(clusterId, jobId, (char *) soap->user)) {
			dprintf(D_FULLDEBUG,
					"Owner check failed for %s\n",
					(char *) soap->user);

			result.response.status.code = FAIL;
			result.response.status.message = "Not job owner";

			return SOAP_OK;
		}
	}

	bool destroy_job = false;

	ScheddTransaction *entry = NULL;
	if (!null_transaction(transaction) &&
		transactionManager.getTransaction(transaction->id, entry)) {
		result.response.status.code = INVALIDTRANSACTION;
		result.response.status.message = "Invalid transaction.";
	} else {
		extendTransaction(transaction);

		Job *job;
		PROC_ID id; id.cluster = clusterId; id.proc = jobId;
		if (!entry || entry->getJob(id, job)) {
				// XXX: Very similar code is in condor__listSpool()
				// All this because you can getFIle outside of a
				// transaction, or get a file from a job that is not
				// part of a transaction, e.g. previously submitted.
			job = new Job(id);
			ASSERT(job);
			CondorError errstack;
			if (job->initialize(errstack)) {
				result.response.status.code =
					(condor__StatusCode) errstack.code();
				result.response.status.message =
					(char *) soap_malloc(soap, strlen(errstack.message()) + 1);
				strcpy(result.response.status.message,
					   errstack.message());

				if (job) {
					delete job;
					job = NULL;
				}

				return SOAP_OK;
			}

			destroy_job = true;
		}

		if (0 >= length) {
			result.response.status.code = FAIL;
			result.response.status.message = "LENGTH must be >= 0";
			dprintf(D_FULLDEBUG, "length is <= 0: %d\n", length);
		} else {
			unsigned char * data =
				(unsigned char *) soap_malloc(soap, length);
			ASSERT(data);

			int status;
			CondorError errstack;
			if (0 == (status = job->get_file(MyString(name),
											 offset,
											 length,
											 data,
											 errstack))) {
				result.response.status.code = SUCCESS;
				result.response.status.message = "Success";

				result.response.data.__ptr = data;
				result.response.data.__size = length;
			} else {
				result.response.status.code =
					(condor__StatusCode) errstack.code();
				result.response.status.message =
					(char *)
					soap_malloc(soap, strlen(errstack.message()) + 1);
				strcpy(result.response.status.message,
					   errstack.message());

				dprintf(D_FULLDEBUG, "get_file failed: %d\n", status);
			}
		}

		if (destroy_job && job) {
			delete job;
			job = NULL;
		}
	}

	return SOAP_OK;
}

int condor__closeSpool(struct soap *soap,
					   struct condor__Transaction *transaction,
					   xsd__int clusterId,
					   xsd__int jobId,
					   struct condor__closeSpoolResponse & result)
{
	dprintf(D_FULLDEBUG,
			"SOAP entered condor__closeSpool(), transaction: %u\n",
			transaction->id);

	if (!verify(WRITE, soap, &result.response)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}

	if (soap->user) {
		if (!verify_owner(clusterId, jobId, (char *) soap->user)) {
			dprintf(D_FULLDEBUG,
					"Owner check failed for %s\n",
					(char *) soap->user);

			result.response.code = FAIL;
			result.response.message = "Not job owner";
			return SOAP_OK;
		}
	}

	ScheddTransaction *entry;
	if (!null_transaction(transaction) &&
		transactionManager.getTransaction(transaction->id, entry)) {
		result.response.code = INVALIDTRANSACTION;
		result.response.message = "Invalid transaction.";
	} else {
		extendTransaction(transaction);

			// XXX: ScheddTransaction needs to do this too, actually,
			// maybe ScheddTransaction should be pushed to a lower
			// level and support all the Set/Get/Hold/crap instead of
			// having versions of those calls that take the
			// transaction they will operate on?
		if (SetAttribute(clusterId, jobId, "FilesRetrieved", "TRUE")) {
			result.response.code = FAIL;
			result.response.message = "Failed to set FilesRetrieved attribute.";
		} else {
			result.response.code = SUCCESS;
			result.response.message = "Success";
		}
	}

	return SOAP_OK;
}

int
condor__listSpool(struct soap * soap,
				  struct condor__Transaction *transaction,
				  int clusterId,
				  int jobId,
				  struct condor__listSpoolResponse & result)
{
	dprintf(D_FULLDEBUG,
			"SOAP entered condor__listSpool(), transaction: %d\n",
			transaction->id);

	if (!verify(READ, soap, &result.response.status)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}

	if (soap->user) {
		if (!verify_owner(clusterId, jobId, (char *) soap->user)) {
			dprintf(D_FULLDEBUG,
					"Owner check failed for %s\n",
					(char *) soap->user);

			result.response.status.code = FAIL;
			result.response.status.message = "Not job owner";

			return SOAP_OK;
		}
	}

	bool destroy_job = false;

	ScheddTransaction *entry = NULL;
	if (!null_transaction(transaction) &&
		transactionManager.getTransaction(transaction->id, entry)) {
		result.response.status.code = INVALIDTRANSACTION;
		result.response.status.message = "Invalid transaction.";
	} else {
		extendTransaction(transaction);

		Job *job;
		PROC_ID id; id.cluster = clusterId; id.proc = jobId;
		if (!entry || entry->getJob(id, job)) {
				// All this because you can listSpool outside of a
				// transaction, or list the spool of a job that is not
				// part of a transaction, e.g. previously submitted.
			job = new Job(id);
			ASSERT(job);
			CondorError errstack;
			if (job->initialize(errstack)) {
				result.response.status.code =
					(condor__StatusCode) errstack.code();
				result.response.status.message =
					(char *) soap_malloc(soap, strlen(errstack.message()) + 1);
				strcpy(result.response.status.message,
					   errstack.message());

				if (job) {
					delete job;
					job = NULL;
				}

				return SOAP_OK;
			}

			destroy_job = true;
		}

		List<FileInfo> files;
		int code;
		CondorError errstack;
		if ((code = job->get_spool_list(files, errstack))) {
			result.response.status.code =
				(condor__StatusCode) errstack.code();
			result.response.status.message =
				(char *) soap_malloc(soap, strlen(errstack.message()) + 1);
			strcpy(result.response.status.message,
				   errstack.message());
			dprintf(D_FULLDEBUG,
					"listSpool: get_spool_list FAILED -- %d\n",
					code);
		} else {
			if (convert_FileInfoList_to_Array(soap,
											  files,
											  result.response.info)) {
				result.response.status.code = SUCCESS;
				result.response.status.message = "Success";
			} else {
				result.response.status.code = FAIL;
				result.response.status.message = "Failed to serialize list.";
				dprintf(D_FULLDEBUG,
						"listSpool: FileInfoList to Array FAILED\n");
			}
		}

			// Cleanup the files.
		FileInfo *info;
		files.Rewind();
		while (files.Next(info)) {
			if (info) {
				delete info;
				info = NULL;
			}
		}

		if (destroy_job && job) {
			delete job;
			job = NULL;
		}
	}

	return SOAP_OK;
}

int
condor__requestReschedule(struct soap *soap,
						  void *,
						  struct condor__requestRescheduleResponse & result)
{
	dprintf(D_FULLDEBUG, "SOAP entered requestReschedule()\n");

	if (!verify(WRITE, soap, &result.response)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}

	if (Reschedule()) {
		result.response.code = SUCCESS;
		result.response.message = "Success";
	} else {
		result.response.code = FAIL;
		result.response.message = "Failed to request reschedule.";
	}

	return SOAP_OK;
}

int
condor__discoverJobRequirements(struct soap *soap,
								struct condor__ClassAdStruct * jobAd,
								struct condor__discoverJobRequirementsResponse & result)
{
	dprintf(D_FULLDEBUG, "SOAP entered discoverJobRequirements()\n");

	if (!verify(READ, soap, &result.response.status)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}

	LooseFileTransfer fileTransfer;

	ClassAd ad;
	StringList inputFiles;
	char *buffer;

	convert_adStruct_to_ad(soap, &ad, jobAd);

		// SimpleInit will bail out if ATTR_JOB_IWD is not set...
	MyString attribute = MyString(ATTR_JOB_IWD) + " = \"/tmp\"";
	if (!ad.Insert(attribute.GetCStr())) {
		result.response.status.code = FAIL;
		result.response.status.message = "Failed to setup temporary Iwd attribute.";

		return SOAP_OK;
	}

	if (!fileTransfer.SimpleInit(&ad, false, false)) {
		result.response.status.code = FAIL;
		result.response.status.message = "Checking for requirements failed.";

		return SOAP_OK;
	}

	fileTransfer.getInputFiles(inputFiles);

	result.response.requirements.__size = inputFiles.number();
	result.response.requirements.__ptr =
		(condor__Requirement *)
		soap_malloc(soap,
					result.response.requirements.__size *
					   sizeof(condor__Requirement));
	ASSERT(result.response.requirements.__ptr);

	inputFiles.rewind();
	int i = 0;
	while ((buffer = inputFiles.next()) &&
		   (i < result.response.requirements.__size)) {
		result.response.requirements.__ptr[i] =
			(char *) soap_malloc(soap, strlen(buffer) + 1);
		ASSERT(result.response.requirements.__ptr[i]);
		strcpy(result.response.requirements.__ptr[i], buffer);
		i++;
	}

	result.response.status.code = SUCCESS;
	result.response.status.message = "Success";

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
	dprintf(D_FULLDEBUG, "SOAP entered createJobTemplate()\n");

	if (!verify(READ, soap, &result.response.status)) {
			// verify() sets the StatusCode/Message
		return SOAP_OK;
	}

	MyString attribute;

	ClassAd *job = CreateJobAd(owner, universe, cmd);

		// CreateJobAd set's ATTR_JOB_IWD, and we can't have that!
	job->Delete(ATTR_JOB_IWD);

	job->Assign(ATTR_VERSION, CondorVersion());
	job->Assign(ATTR_PLATFORM, CondorPlatform());

	job->Assign(ATTR_CLUSTER_ID, clusterId);
	job->Assign(ATTR_PROC_ID, jobId);
	if (requirements) {
		job->AssignExpr(ATTR_REQUIREMENTS, requirements);
	}

		// It is kinda scary but if ATTR_STAGE_IN_START/FINISH are
		// present and non-zero in a Job Ad the Schedd will do the
		// right thing, when run as root, and chown the job's spool
		// directory, thus fixing a long standing permissions problem.
	job->Assign(ATTR_STAGE_IN_START, 1);
	job->Assign(ATTR_STAGE_IN_FINISH, 1);

	job->Assign("FilesRetrieved", false);

	attribute = "FilesRetrieved=?=FALSE";
	char *soapLeaveInQueue = param("SOAP_LEAVE_IN_QUEUE");
	if (soapLeaveInQueue) {
		attribute = attribute + " && (" + soapLeaveInQueue + ")";
	}
	job->AssignExpr(ATTR_JOB_LEAVE_IN_QUEUE, attribute.GetCStr());

		// XXX: Where did this code come from, why is it important?
		// -Matt CW06
	ArgList arglist;
	MyString arg_errors;
	if(!arglist.AppendArgsV2Raw(args,&arg_errors) ||
	   !arglist.InsertArgsIntoClassAd(job,NULL,&arg_errors)) {

 		result.response.status.code = FAIL;
		result.response.status.message = "Invalid arguments string.";
		convert_ad_to_adStruct(soap, job, &result.response.classAd, true);

		dprintf(D_ALWAYS,"Failed to parse job args from soap caller: %s\n",
				arg_errors.Value());

		return SOAP_OK;
	}

	result.response.status.code = SUCCESS;
	result.response.status.message = "Success";
	convert_ad_to_adStruct(soap, job, &result.response.classAd, true);

	return SOAP_OK;
}


///////////////////////////////////////////////////////////////////////////////
// TODO : This should move into daemonCore once we figure out how we wanna link
///////////////////////////////////////////////////////////////////////////////

#include "../condor_daemon_core.V6/soap_daemon_core.cpp"
