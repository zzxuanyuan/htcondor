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

static bool
Reschedule()
{

	scheduler.timeout();		// update the central manager now

	dprintf(D_FULLDEBUG, "Called Reschedule()\n");

	scheduler.sendReschedule();

	scheduler.StartLocalJobs();

	return true;
}

static bool
verify(DCpermission perm,
	   const struct soap *soap,
	   struct condor__Status &status)
{
	ASSERT(soap);


	if (daemonCore->Verify(perm,
				&soap->peer,
				soap->user ? (char*)soap->user : NULL) != USER_AUTH_SUCCESS)
	{
		dprintf(D_FULLDEBUG,
				"SOAP call rejected, no permission for user %s/%s\n",
				soap->user ? (char*)soap->user : "NULL",
				sin_to_string(&soap->peer));

//		if ( status ) {
//			status->code = FAIL;
//			status->message = "Permission denied";
//		}
		status.code = FAIL;
		status.message = "Permission denied";

		return false;
	}

	return true;
}

static bool
verify_owner(int clusterId,
			 int jobId,
			 char *owner,
			 struct condor__Status &status)
{
	ClassAd *ad = NULL;
	bool result;

	if (!(ad = GetJobAd(clusterId, jobId))) {
		// failed to get any info on this job
//		if (status ) status->message = "Failed to find job specified";
		status.message = "Failed to find job specified";
		result = false;
	} else {
		result =  OwnerCheck2(ad, owner);
		if ( result == false ) {
//			if (status) status->message = "Not job owner";
			status.message = "Not job owner";
		}
	}

	if ( result == false ) {
			dprintf(D_FULLDEBUG,
				"SOAP call rejected, user %s cannot modify job %d.%d\n",
				owner ? owner : "NULL",
				clusterId,jobId	);
//			if ( status) {
//				status->code = FAIL;
//				status->message = "Not job owner";
//			}
			status.code = FAIL;
			status.message = "Not job owner";
	}

	return result;
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
static int
transtimeout()
{
	struct condor__abortTransactionResponse result;

	dprintf(D_FULLDEBUG, "SOAP in transtimeout()\n");

	condor__Transaction transaction;
	transaction.id = current_trans_id;
	condor__abortTransaction(NULL, transaction, result);
	return TRUE;
}

static bool
stub_prefix(const char* stub_name,   // IN
			const struct soap *soap, // IN
			const int clusterId,	 // IN
			const int jobId,		 // IN
			const DCpermission perm,  // IN
			const bool must_have_transid,  // IN
			struct condor__Transaction* & transaction, // IN/OUT
			struct condor__Status & result,		// OUT
			ScheddTransaction* & entry )	// OUT
{
	static NullScheddTransaction null_entry(NULL);
	static condor__Transaction null_transaction;

	entry = &null_entry;

	if (transaction == NULL ) {
		// point to a NULL transaction object
		transaction = &null_transaction;
		transaction->id = 0;	// id  means NO transaction
		transaction->duration = 0;
	}

		// print out something to the log if we have stub name
	if ( stub_name ) {
		dprintf(D_FULLDEBUG,
			"SOAP entered %s(), transaction: %u\n",
			stub_name,transaction->id);
	}


		// fail if we must have a transid, and we were not give one
	if ( must_have_transid && !transaction->id ) {
//		if ( result ) {
//			result->code = INVALIDTRANSACTION;
//			result->message = "Invalid transaction";
//		}
		result.code = INVALIDTRANSACTION;
		result.message = "Invalid transaction";

		return false;
	}

		// if we were given a transid, it had better be valid
	if (transaction->id &&
		transactionManager.getTransaction(transaction->id, entry))
	{
//		if ( result ) {
//			result->code = INVALIDTRANSACTION;
//			result->message = "Invalid transaction";
//		}
		result.code = INVALIDTRANSACTION;
		result.message = "Invalid transaction";

		return false;
	}

		// now that we know our transaction is valid, extend it
	if ( transaction->id ) {
		if (extendTransaction(transaction)) {
			result.code = FAIL;
			result.message = "Could not extend transaction";

			return false;
		}
	}

	if (!verify(perm, soap, result)) {
			// verify() sets the result StatusCode/Message
		return false;
	}

	if (soap->user && clusterId) {
		if (!verify_owner(clusterId, jobId, (char *) soap->user, result)) {
				// verify_owner() sets the result StatusCode/Message
			return false;
		}
	}

	return true;
}


static bool
stub_suffix(const char* stub_name,   // IN
			const struct soap *soap, // IN
			const ScheddTransaction* entry,	// IN, could be null
			const struct condor__Status & status) // IN
{
	// Cleanup transaction stuff here

	if (stub_name) {
		dprintf(D_FULLDEBUG,
				"SOAP leaving %s() result=%d\n",
				stub_name,
				status.code);
	}

	return true;
}

/*************************************
	SOAP STUBS
************************************/

int
condor__beginTransaction(struct soap *soap,
						 int duration,
						 struct condor__beginTransactionResponse & result)
{
	struct condor__Transaction *transaction = NULL;
	ScheddTransaction *entry;
	if (!stub_prefix("beginTransaction",
					 soap,
					 0,
					 0,
					 WRITE,
					 false,
					 transaction,
					 result.response.status,
					 entry)) {
		return SOAP_OK;
	}

	if ( current_trans_id ) {
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
								   (TimerHandler)&transtimeout,
								   "condor_transtimeout");
	daemonCore->Only_Allow_Soap(duration);

	int id;
	char *owner = NULL; // Get OWNER from X509 cert...
	if (transactionManager.createTransaction(owner,
											 id,
											 entry)) {
		result.response.status.code = FAIL;
		result.response.status.message = "Unable to create transaction";
	} else {
		entry->duration = duration;

		result.response.transaction.id = id;
		result.response.transaction.duration = entry->duration;
		result.response.status.code = SUCCESS;
		result.response.status.message = "Success";

		setQSock(NULL);

		if (entry->begin()) {
			result.response.status.code = FAIL;
			result.response.status.message = "Unable to begin transaction";
		}
	}

	stub_suffix("beginTransaction", soap, entry, result.response.status);

	return SOAP_OK;
}

int
condor__commitTransaction(struct soap *soap,
						  struct condor__Transaction transaction,
						  struct condor__commitTransactionResponse & result)
{
	condor__Transaction *transaction_ptr = &transaction;
	ScheddTransaction *entry;
	if (!stub_prefix("commitTransaction",
					 soap,
					 0,
					 0,
					 WRITE,
					 true,
					 transaction_ptr,
					 result.response,
					 entry)) {
		return SOAP_OK;
	}

	entry->commit();

	if (transactionManager.destroyTransaction(transaction.id)) {
		dprintf(D_ALWAYS, "condor__commitTransaction cleanup failed\n");
	}

	current_trans_id = 0;
	if ( trans_timer_id != -1 ) {
		daemonCore->Cancel_Timer(trans_timer_id);
		trans_timer_id = -1;
	}
	daemonCore->Only_Allow_Soap(0);

	result.response.code = SUCCESS;
	result.response.message = "Success";

	stub_suffix("commitTransaction", soap, entry, result.response);

	return SOAP_OK;
}


int
condor__abortTransaction(struct soap *soap,
						 struct condor__Transaction transaction,
						 struct condor__abortTransactionResponse & result )
{
	condor__Transaction *transaction_ptr = &transaction;
	ScheddTransaction *entry;
	if (!stub_prefix("commitTransaction",
					 soap,
					 0,
					 0,
					 WRITE,
					 true,
					 transaction_ptr,
					 result.response,
					 entry)) {
		return SOAP_OK;
	}

	entry->abort();

	if (transactionManager.destroyTransaction(transaction.id)) {
		dprintf(D_ALWAYS, "condor__abortTransaction cleanup failed\n");
	}

	current_trans_id = 0;
	if (trans_timer_id != -1) {
		daemonCore->Cancel_Timer(trans_timer_id);
		trans_timer_id = -1;
	}
	daemonCore->Only_Allow_Soap(0);

	result.response.code = SUCCESS;
	result.response.message = "Success";

	stub_suffix("abortTransaction", soap, entry, result.response);

	return SOAP_OK;
}


int
condor__extendTransaction(struct soap *soap,
						  struct condor__Transaction transaction,
						  int duration,
						  struct condor__extendTransactionResponse & result )
{
	condor__Transaction *transaction_ptr = &transaction;
	ScheddTransaction *entry;
	if (!stub_prefix("extendTransaction",
					 soap,
					 0,
					 0,
					 WRITE,
					 true,
					 transaction_ptr,
					 result.response.status,
					 entry)) {
		return SOAP_OK;
	}
 
	entry->duration = duration;

	result.response.transaction.id = transaction.id;
	result.response.transaction.duration = entry->duration;

	result.response.status.code = SUCCESS;
	result.response.status.message = "Success";

	stub_suffix("extendTransaction", soap, entry, result.response.status);

	return SOAP_OK;
}


int
condor__newCluster(struct soap *soap,
				   struct condor__Transaction transaction,
				   struct condor__newClusterResponse & result)
{
	condor__Transaction *transaction_ptr = &transaction;
	ScheddTransaction *entry;
	if (!stub_prefix("newCluster",
					 soap,
					 0,
					 0,
					 WRITE,
					 true,
					 transaction_ptr,
					 result.response.status,
					 entry)) {
		return SOAP_OK;
	}

	int id;
	if (entry->newCluster(id)) {
		result.response.status.code = FAIL;
		result.response.status.message = "Could not create new cluster";
	} else {
		result.response.integer = id;

		result.response.status.code = SUCCESS;
		result.response.status.message = "Success";
	}

	stub_suffix("newCluster", soap, entry, result.response.status);

	return SOAP_OK;
}


int
condor__removeCluster(struct soap *soap,
					  struct condor__Transaction *transaction,
					  int clusterId,
					  char* reason,
					  struct condor__removeClusterResponse & result)
{
	ScheddTransaction *entry;
	if (!stub_prefix("removeCluster",
					 soap,
					 0, // We manually check below...
					 0,
					 WRITE,
					 false,
					 transaction,
					 result.response,
					 entry)) {
		return SOAP_OK;
	}

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

        if (!verify_owner(clusterId,
						  jobId,
						  (char *) soap->user,
						  result.response)) {
			result.response.message = "Not cluster owner";

			return SOAP_OK;
        }
	}

	if (abortJobsByConstraint(constraint.GetCStr(),
							  reason,
							  transaction->id ? false : true)) {
		result.response.code = FAIL;
		result.response.message = "Failed to abort jobs in the cluster";
	} else {
		if (entry->removeCluster(clusterId)) {
			result.response.code = FAIL;
			result.response.message = "Failed to clean up job, abort";
		} else {
			result.response.code = SUCCESS;
			result.response.message = "Success";
		}
	}

	stub_suffix("removeCluster", soap, entry, result.response);

    return SOAP_OK;
}


int
condor__newJob(struct soap *soap,
			   struct condor__Transaction transaction,
			   int clusterId,
			   struct condor__newJobResponse & result)
{
	condor__Transaction *transaction_ptr = &transaction;
	ScheddTransaction *entry;
	if (!stub_prefix("newJob",
					 soap,
					 0,
					 0,
					 WRITE,
					 true,
					 transaction_ptr,
					 result.response.status,
					 entry)) {
		return SOAP_OK;
	}

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

	stub_suffix("newJob", soap, entry, result.response.status);

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
	ScheddTransaction *entry;
	if (!stub_prefix("removeJob",
					 soap,
					 clusterId,
					 jobId,
					 WRITE,
					 false,
					 transaction,
					 result.response,
					 entry)) {
		return SOAP_OK;
	}

	if (!abortJob(clusterId,
				  jobId,
				  reason,
				  transaction->id ? false : true)) {
		result.response.code = FAIL;
		result.response.message = "Failed to abort job";
	} else {
		PROC_ID id; id.cluster = clusterId; id.proc = jobId;
		if (entry->removeJob(id)) {
			result.response.code = FAIL;
			result.response.message = "Failed to clean up job, abort";
		} else {
			result.response.code = SUCCESS;
			result.response.message = "Success";
		}
	}

	stub_suffix("removeJob", soap, entry, result.response);

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
	ScheddTransaction *entry;
	if (!stub_prefix("holdJob",
					 soap,
					 clusterId,
					 jobId,
					 WRITE,
					 false,
					 transaction,
					 result.response,
					 entry)) {
		return SOAP_OK;
	}

	if (!holdJob(clusterId,jobId,reason,transaction->id ? false : true,
				 true, email_user, email_admin, system_hold)) {
		result.response.code = FAIL;
		result.response.message = "Failed to hold job.";
	} else {
		result.response.code = SUCCESS;
		result.response.message = "Success";
	}

	stub_suffix("holdJob", soap, entry, result.response);

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
	ScheddTransaction *entry;
	if (!stub_prefix("releaseJob",
					 soap,
					 clusterId,
					 jobId,
					 WRITE,
					 false,
					 transaction,
					 result.response,
					 entry)) {
		return SOAP_OK;
	}

	if (!releaseJob(clusterId,jobId,reason,transaction->id ? false : true,
					email_user, email_admin)) {
		result.response.code = FAIL;
		result.response.message = "Failed to release job.";
	} else {
		result.response.code = SUCCESS;
		result.response.message = "Success";
	}

	stub_suffix("releaseJob", soap, entry, result.response);

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
	condor__Transaction *transaction_ptr = &transaction;
	ScheddTransaction *entry;
	if (!stub_prefix("submit",
					 soap,
					 clusterId,
					 jobId,
					 WRITE,
					 true,
					 transaction_ptr,
					 result.response.status,
					 entry)) {
		return SOAP_OK;
	}

		// authorized if no owner was recorded (i.e. no
		// authentication) or if there is an owner and it matches
		// the authenticated user
	bool authorized = !entry->getOwner() || !strcmp(entry->getOwner(),
													(char *) soap->user);

	if (!authorized) {
		result.response.status.code = FAIL; 
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

	stub_suffix("submit", soap, entry, result.response.status);

	return SOAP_OK;
}


int
condor__getJobAds(struct soap *soap,
				  struct condor__Transaction *transaction,
				  char *constraint,
				  struct condor__getJobAdsResponse & result )
{
	ScheddTransaction *entry;
	if (!stub_prefix("getJobAds",
					 soap,
					 0,
					 0,
					 READ,
					 false,
					 transaction,
					 result.response.status,
					 entry)) {
		return SOAP_OK;
	}


	List<ClassAd> adList;
	if (entry->queryJobAds(constraint, adList)) {
		result.response.status.code = FAIL;
		result.response.status.message = "Query failed";
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

	stub_suffix("getJobAds", soap, entry, result.response.status);

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

	ScheddTransaction *entry;
	if (!stub_prefix("getJobAd",
					 soap,
					 clusterId,
					 jobId,
					 READ,
					 false,
					 transaction,
					 result.response.status,
					 entry)) {
		return SOAP_OK;
	}

	ClassAd *ad;
	PROC_ID id; id.cluster = clusterId; id.proc = jobId;
	if (entry->queryJobAd(id, ad)) {
		result.response.status.code = FAIL;
		result.response.status.message = "Query failed";
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

	stub_suffix("getJobAd", soap, entry, result.response.status);

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
	condor__Transaction *transaction_ptr = &transaction;
	ScheddTransaction *entry;
	if (!stub_prefix("declareFile",
					 soap,
					 clusterId,
					 jobId,
					 WRITE,
					 true,
					 transaction_ptr,
					 result.response,
					 entry)) {
		return SOAP_OK;
	}

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

	stub_suffix("declareFile", soap, entry, result.response);

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
	condor__Transaction *transaction_ptr = &transaction;
	ScheddTransaction *entry;
	if (!stub_prefix("sendFile",
					 soap,
					 clusterId,
					 jobId,
					 WRITE,
					 true,
					 transaction_ptr,
					 result.response,
					 entry)) {
		return SOAP_OK;
	}

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

	stub_suffix("sendFile", soap, entry, result.response);

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
	ScheddTransaction *entry;
	if (!stub_prefix("getFile",
					 soap,
					 clusterId,
					 jobId,
					 READ,
					 false,
					 transaction,
					 result.response.status,
					 entry)) {
		return SOAP_OK;
	}

	bool destroy_job = false;

	Job *job;
	PROC_ID id; id.cluster = clusterId; id.proc = jobId;
	if (entry->getJob(id, job)) {
			// All this because you can getFile outside of a
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

	stub_suffix("getFile", soap, entry, result.response.status);

	return SOAP_OK;
}

int condor__closeSpool(struct soap *soap,
					   struct condor__Transaction *transaction,
					   xsd__int clusterId,
					   xsd__int jobId,
					   struct condor__closeSpoolResponse & result)
{
	ScheddTransaction *entry;
	if (!stub_prefix("closeSpool",
					 soap,
					 clusterId,
					 jobId,
					 WRITE,
					 false,
					 transaction,
					 result.response,
					 entry)) {
		return SOAP_OK;
	}

	if (SetAttribute(clusterId, jobId, "FilesRetrieved", "TRUE")) {
		result.response.code = FAIL;
		result.response.message = "Failed to set FilesRetrieved attribute.";
	} else {
		result.response.code = SUCCESS;
		result.response.message = "Success";
	}

	stub_suffix("closeSpool", soap, entry, result.response);

	return SOAP_OK;
}

int
condor__listSpool(struct soap * soap,
				  struct condor__Transaction *transaction,
				  int clusterId,
				  int jobId,
				  struct condor__listSpoolResponse & result)
{
	ScheddTransaction *entry;
	if (!stub_prefix("listSpool",
					 soap,
					 clusterId,
					 jobId,
					 READ,
					 false,
					 transaction,
					 result.response.status,
					 entry)) {
		return SOAP_OK;
	}

	bool destroy_job = false;

	Job *job;
	PROC_ID id; id.cluster = clusterId; id.proc = jobId;
	if (entry->getJob(id, job)) {
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

	stub_suffix("listSpool", soap, entry, result.response.status);

	return SOAP_OK;
}

int
condor__requestReschedule(struct soap *soap,
						  void *,
						  struct condor__requestRescheduleResponse & result)
{
	struct condor__Transaction *transaction = NULL;
	ScheddTransaction *entry;
	if (!stub_prefix("requestReschedule",
					 soap,
					 0,
					 0,
					 WRITE,
					 false,
					 transaction,
					 result.response,
					 entry)) {
		return SOAP_OK;
	}

	if (Reschedule()) {
		result.response.code = SUCCESS;
		result.response.message = "Success";
	} else {
		result.response.code = FAIL;
		result.response.message = "Failed to request reschedule.";
	}

	stub_suffix("requestReschedule", soap, entry, result.response);

	return SOAP_OK;
}

int
condor__discoverJobRequirements(struct soap *soap,
								struct condor__ClassAdStruct * jobAd,
								struct condor__discoverJobRequirementsResponse & result)
{
	struct condor__Transaction *transaction = NULL;
	ScheddTransaction *entry;
	if (!stub_prefix("discoverJobRequirements",
					 soap,
					 0,
					 0,
					 READ,
					 false,
					 transaction,
					 result.response.status,
					 entry)) {
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

	stub_suffix("discoverJobRequirements", soap, entry, result.response.status);

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
	struct condor__Transaction *transaction = NULL;
	ScheddTransaction *entry;
	if (!stub_prefix("createJobTemplate",
					 soap,
					 0,
					 0,
					 READ,
					 false,
					 transaction,
					 result.response.status,
					 entry)) {
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

	stub_suffix("createJobTemplate", soap, entry, result.response.status);

	return SOAP_OK;
}


///////////////////////////////////////////////////////////////////////////////
// TODO : This should move into daemonCore once we figure out how we wanna link
///////////////////////////////////////////////////////////////////////////////

#include "../condor_daemon_core.V6/soap_daemon_core.cpp"
