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

#include "condorCore.nsmap"  // #include "condorMaster.nsmap"
#include "soap_masterC.cpp"
#include "soap_masterServer.cpp"


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
