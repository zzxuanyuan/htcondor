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

#include "condorSchedd.nsmap"
#include "soap_scheddC.cpp"
#include "soap_scheddServer.cpp"

#include "../condor_c++_util/soap_helpers.cpp"

static xsd__long current_trans_id = 0;
static int trans_timer_id = -1;

static bool valid_transaction_id(xsd__long id)
{
	if (current_trans_id == id || 0 == id ) {
		return true;
	} else {
		return false;
	}
}

// TODO : Todd needs to redo all the transaction stuff and get it right.  For now
// it is in horrible "demo" mode with piles of assumptions (i.e. only one client, etc).
// Once it is redone and decent, all the logic should move OUT of the stubs and into the schedd
// proper... since it should all work the same from the cedar side as well.
int
condorSchedd__transtimeout()
{
	int result;

	dprintf(D_ALWAYS,"SOAP in condorSchedd__transtimeout()\n");

	condorSchedd__abortTransaction(NULL,current_trans_id,result);
	return TRUE;
}

int condorSchedd__beginTransaction(struct soap *s,int duration, 
			xsd__long & transactionId)
{
	if ( current_trans_id ) {
		// if there is an existing transaction, abort it.
		// TODO - support more than one active transaction!!!
		int result;
		condorSchedd__abortTransaction(s,current_trans_id,result);
	}
	if ( duration < 1 ) {
		duration = 1;
	}

	trans_timer_id = daemonCore->Register_Timer(duration,(TimerHandler)&condorSchedd__transtimeout,
						"condorSchedd_transtimeout");

	transactionId = time(NULL);   // TODO : choose unique id - use time for now

	setQSock(NULL);	// Tell the qmgmt layer to allow anything -- that is, until
					// we authenticate the client.

	dprintf(D_ALWAYS,"SOAP leaving condorSchedd__beginTransaction() id=%ld\n",transactionId);

	return SOAP_OK;
}

int condorSchedd__commitTransaction(struct soap *s,xsd__long transactionId, 
			int & result )
{
	result = 0;
	if ( transactionId == current_trans_id ) {
		CommitTransaction();
		current_trans_id = 0;
		transactionId = 0;
		if ( trans_timer_id != -1 ) {
			daemonCore->Cancel_Timer(trans_timer_id);
			trans_timer_id = -1;
		}
	}
	if ( !valid_transaction_id(transactionId) ) {
		// TODO error - unrecognized transactionId
		result = -1;
	}

	dprintf(D_ALWAYS,"SOAP leaving condorSchedd__commitTransaction() res=%d\n",result);
	return SOAP_OK;
}


int condorSchedd__abortTransaction(struct soap *,xsd__long transactionId,
			int & result )
{
	result = 0;
	if ( transactionId && transactionId == current_trans_id ) {
		AbortTransactionAndRecomputeClusters();
		current_trans_id = 0;
		transactionId = 0;
		if ( trans_timer_id != -1 ) {
			daemonCore->Cancel_Timer(trans_timer_id);
			trans_timer_id = -1;
		}
	}
	if ( !valid_transaction_id(transactionId) ) {
		// TODO error - unrecognized transactionId
		result = -1;
	}

	dprintf(D_ALWAYS,"SOAP leaving condorSchedd__abortTransaction() res=%d\n",result);
	return SOAP_OK;
}


int condorSchedd__extendTransaction(struct soap *s,xsd__long transactionId,
			int duration,
			int & result )
{
	result = -1;
	if ( transactionId &&	// must not be 0
		 transactionId == current_trans_id &&	// must be the current transaction
		 trans_timer_id != -1 ) 
	{
		result = 0;
		if ( duration < 1 ) {
			duration = 1;
		}
		daemonCore->Reset_Timer(trans_timer_id,duration);
	}

	dprintf(D_ALWAYS,"SOAP leaving condorSchedd__extendTransaction() res=%d\n",result);
	return SOAP_OK;
}


int condorSchedd__newCluster(struct soap *s,xsd__long transactionId,
			int & result)
{
	if ( (transactionId == 0) || (!valid_transaction_id(transactionId)) ) {
		// TODO error - unrecognized transactionId
	}
	result = NewCluster();
	if ( result == -1 ) {
		// TODO error case
	}

	dprintf(D_ALWAYS,"SOAP leaving condorSchedd__newCluster() res=%d\n",result);
	return SOAP_OK;
}


int condorSchedd__removeCluster(struct soap *s,xsd__long transactionId,
			int clusterId, char* reason,
			int & result)
{
	// TODO!!!
	result = 0;
	if ( !valid_transaction_id(transactionId) ) {
		// TODO error - unrecognized transactionId
		result = -1;
	} else {
		result = DestroyCluster(clusterId,reason);  // returns -1 or 0
	}

	dprintf(D_ALWAYS,"SOAP leaving condorSchedd__removeCluster() res=%d\n",result);
	return SOAP_OK;
}


int condorSchedd__newJob(struct soap *s,xsd__long transactionId,
			int clusterId, int & result)
{
	result = 0;
	if ( (transactionId == 0) || (!valid_transaction_id(transactionId)) ) {
		// TODO error - unrecognized transactionId
	}
	result = NewProc(clusterId);
	if ( result == -1 ) {
		// TODO error case
	}

	dprintf(D_ALWAYS,"SOAP leaving condorSchedd__newJob() res=%d\n",result);
	return SOAP_OK;
}


int condorSchedd__removeJob(struct soap *s,xsd__long transactionId,
			int clusterId, int jobId, char* reason, bool force_removal,
			int & result)
{
	// TODO --- do something w/ force_removal flag; it is ignored for now.
	result = 0;
	if ( !valid_transaction_id(transactionId) ) {
		// TODO error - unrecognized transactionId
	}
	if ( !abortJob(clusterId,jobId,reason,transactionId ? false : true) ) 
	{
		// TODO error - remove failed
		result = -1;
	}

	dprintf(D_ALWAYS,"SOAP leaving condorSchedd__removeJob() res=%d\n",result);
	return SOAP_OK;
}


int condorSchedd__holdJob(struct soap *s,xsd__long transactionId,
			int clusterId, int jobId, char* reason,
			bool email_user, bool email_admin, bool system_hold,
			int & result)
{
	result = 0;
	if ( !valid_transaction_id(transactionId) ) {
		// TODO error - unrecognized transactionId
	}
	if ( !holdJob(clusterId,jobId,reason,transactionId ? false : true,
				  email_user, email_admin, system_hold) ) 
	{
		// TODO error - remove failed
		result = -1;
	}

	dprintf(D_ALWAYS,"SOAP leaving condorSchedd__holdJob() res=%d\n",result);
	return SOAP_OK;
}


int condorSchedd__releaseJob(struct soap *s,xsd__long transactionId,
			int clusterId, int jobId, char* reason,
			bool email_user, bool email_admin,
			int & result)
{
	result = 0;
	if ( !valid_transaction_id(transactionId) ) {
		// TODO error - unrecognized transactionId
	}
	if ( !releaseJob(clusterId,jobId,reason,transactionId ? false : true,
				  email_user, email_admin) ) 
	{
		// TODO error - release failed
		result = -1;
	}

	dprintf(D_ALWAYS,"SOAP leaving condorSchedd__releaseJob() res=%d\n",result);
	return SOAP_OK;
}


int condorSchedd__submit(struct soap *s,xsd__long transactionId,
				int clusterId, int jobId,
				struct ClassAdStruct * jobAd,
				int & result)
{
	result = 0;

	if ( (transactionId == 0) || (!valid_transaction_id(transactionId)) ) {
		// TODO error - unrecognized transactionId
	}

	int i,rval;
	MyString buf;
	for (i=0; i < jobAd->__size; i++ ) {
		const char* name = jobAd->__ptr[i].name;
		const char* value = jobAd->__ptr[i].value;
		if (!name) continue;   
		if (!value) value="UNDEFINED";

		if ( jobAd->__ptr[i].type == 's' ) {
			// string type - put value in quotes as hint for ClassAd parser
			buf.sprintf("%s=\"%s\"", name, value);
		} else {
			// all other types can be deduced by the ClassAd parser
			buf.sprintf("%s=%s",name,value);
		}
		rval = SetAttribute(clusterId,jobId,name,value);
		if ( rval < 0 ) {
			result = -1;
		}
	}

	dprintf(D_ALWAYS,"SOAP leaving condorSchedd__submit() res=%d\n",result);
	return SOAP_OK;
}


int condorSchedd__getJobAds(struct soap *s,xsd__long transactionId,
				char *constraint,
				struct ClassAdStructArray & result )
{
	dprintf(D_ALWAYS,"SOAP entering condorSchedd__getJobAds() \n");

	if ( !valid_transaction_id(transactionId) ) {
		// TODO error - unrecognized transactionId
	}

	List<ClassAd> adList;
	ClassAd *ad = GetNextJobByConstraint(constraint,1);
	while ( ad ) {
		adList.Append(ad);
		ad = GetNextJobByConstraint(constraint,0);
	}
	
	// fill in our soap struct response
	if ( !convert_adlist_to_adStructArray(s,&adList,&result) ) {
		dprintf(D_ALWAYS,"condorSchedd__getJobAds: convert_adlist_to_adStructArray failed!\n");
	}

	return SOAP_OK;
}


int condorSchedd__getJobAd(struct soap *s,xsd__long transactionId,
				int clusterId, int jobId,
				struct ClassAdStruct & result )
{
	// TODO : deal with transaction consistency; currently, job ad is 
	// invisible until a commit.  not very ACID compliant, is it? :(

	dprintf(D_ALWAYS,"SOAP entering condorSchedd__getJobAd() \n");

	if ( !valid_transaction_id(transactionId) ) {
		// TODO error - unrecognized transactionId
	}

	ClassAd *ad = GetJobAd(clusterId,jobId);
	if ( !convert_ad_to_adStruct(s,ad,&result) ) {
		dprintf(D_ALWAYS,"condorSchedd__getJobAds: convert_adlist_to_adStructArray failed!\n");
	}

	return SOAP_OK;
}


///////////////////////////////////////////////////////////////////////////////
// TODO : This should move into daemonCore once we figure out how we wanna link
///////////////////////////////////////////////////////////////////////////////

int condorCore__getPlatformString(struct soap *soap,void *,char* &result)
{
	result = CondorPlatform();
	return SOAP_OK;
}

int condorCore__getVersionString(struct soap *soap,void *,char* &result)
{
	result = CondorVersion();
	return SOAP_OK;
}

int condorCore__getInfoAd(struct soap *soap,void *,ClassAdStruct & ad)
{
	char* todd = "Todd A Tannenbaum";

	ad.__size = 3;
	ad.__ptr = (condorCore__ClassAdStructAttr *)soap_malloc(soap,3 * sizeof(condorCore__ClassAdStructAttr));

	ad.__ptr[0].name = "Name";
	ad.__ptr[0].type = 's';
	ad.__ptr[0].value = todd;
	ad.__ptr[0].valueInt = NULL;
	ad.__ptr[0].valueFloat = NULL;
	ad.__ptr[0].valueBool = NULL;
	ad.__ptr[0].valueExpr = NULL;

	ad.__ptr[1].name = "Age";
	ad.__ptr[1].type = 'n';
	ad.__ptr[1].value = "35";
	int* age = (int*)soap_malloc(soap,sizeof(int));
	*age = 35;
	ad.__ptr[1].valueInt = age;
	ad.__ptr[1].valueFloat = NULL;
	ad.__ptr[1].valueBool = NULL;
	ad.__ptr[1].valueExpr = NULL;

	ad.__ptr[2].name = "Friend";
	ad.__ptr[2].type = 's';
	ad.__ptr[2].value = todd;
	ad.__ptr[2].valueInt = NULL;
	ad.__ptr[2].valueFloat = NULL;
	ad.__ptr[2].valueBool = NULL;
	ad.__ptr[2].valueExpr = NULL;

	return SOAP_OK;

}
