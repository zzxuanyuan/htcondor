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
#include "collector.h"
#include "condor_attributes.h"

#include "condorCollector.nsmap"
#include "soap_collectorC.cpp"
#include "soap_collectorServer.cpp"

static int receive_query_soap(int command,struct soap *s,char *constraint,
	struct ClassAdStructArray &ads)
{

	// check for authorization here

	// construct a query classad from the constraint
	ClassAd query_ad;
	query_ad.SetMyTypeName(QUERY_ADTYPE);
	query_ad.SetTargetTypeName(ANY_ADTYPE);
	MyString req = ATTR_REQUIREMENTS;
	req += " = ";
	if ( constraint && constraint[0] ) {
		req += constraint;
	} else {
		req += "True";
	}
	query_ad.Insert(req.Value());

	// actually process the query
	List<ClassAd> adList;
	int result = CollectorDaemon::receive_query_public(command,&query_ad,&adList);

	// and send fill in our soap struct response
	ExprTree *tree, *rhs, *lhs;	
	ClassAd *curr_ad = NULL;
	ads.__size = adList.Number();
	ads.__ptr = (struct ClassAdStruct *) soap_malloc(s, 
							ads.__size * sizeof(struct ClassAdStruct));
	adList.Rewind();
	int ad_index = 0;
	int attr_index = 0;
	int num_attrs = 0;
	bool skip_attr = false;
	int tmpint;
	float tmpfloat;
	bool tmpbool;
	char *tmpstr;

	while ( (curr_ad=adList.Next()) ) 
    {
			// first pass: count attrs
		num_attrs = 0;
		curr_ad->ResetExpr();
		while( (tree = curr_ad->NextExpr()) ) {
			lhs = tree->LArg();
			rhs = tree->RArg();
			if( lhs && rhs ) { 
				num_attrs++;
			}
		}
		if ( num_attrs == 0 ) {
			continue;
		}

			// allocate space
		ads.__ptr[ad_index].__size = num_attrs;
		ads.__ptr[ad_index].__ptr = (condorCore__ClassAdStructAttr *)
				soap_malloc(s,num_attrs * sizeof(condorCore__ClassAdStructAttr));

		
			// second pass: serialize attrs
		attr_index = 0;		
		curr_ad->ResetExpr();
		while( (tree = curr_ad->NextExpr()) ) {
			rhs = tree->RArg();
			ads.__ptr[ad_index].__ptr[attr_index].valueInt = NULL;
			ads.__ptr[ad_index].__ptr[attr_index].valueFloat = NULL;
			ads.__ptr[ad_index].__ptr[attr_index].valueBool = NULL;
			ads.__ptr[ad_index].__ptr[attr_index].valueExpr = NULL;
			skip_attr = false;
			switch ( rhs->MyType() ) {
			case LX_STRING:
				ads.__ptr[ad_index].__ptr[attr_index].value = ((String*)rhs)->Value();
				ads.__ptr[ad_index].__ptr[attr_index].type = 's';
				break;
			case LX_INTEGER:
				tmpint = ((Integer*)rhs)->Value();
				ads.__ptr[ad_index].__ptr[attr_index].value = (char*)soap_malloc(s,20);
				snprintf(ads.__ptr[ad_index].__ptr[attr_index].value,20,"%d",tmpint);
				ads.__ptr[ad_index].__ptr[attr_index].valueInt = (int*)soap_malloc(s,sizeof(int));
				*(ads.__ptr[ad_index].__ptr[attr_index].valueInt) = tmpint;
				ads.__ptr[ad_index].__ptr[attr_index].type = 'n';
				break;
			case LX_FLOAT:
				tmpfloat = ((Float*)rhs)->Value();
				ads.__ptr[ad_index].__ptr[attr_index].value = (char*)soap_malloc(s,20);
				snprintf(ads.__ptr[ad_index].__ptr[attr_index].value,20,"%f",tmpfloat);
				ads.__ptr[ad_index].__ptr[attr_index].valueFloat = (float*)soap_malloc(s,sizeof(float));
				*(ads.__ptr[ad_index].__ptr[attr_index].valueFloat) = tmpfloat;
				ads.__ptr[ad_index].__ptr[attr_index].type = 'f';
				break;
			case LX_BOOL:
				tmpbool = ((ClassadBoolean*)rhs)->Value() ? true : false;
				if ( tmpbool ) {
					ads.__ptr[ad_index].__ptr[attr_index].value = "TRUE";
				} else {
					ads.__ptr[ad_index].__ptr[attr_index].value = "FALSE";
				}
				ads.__ptr[ad_index].__ptr[attr_index].valueBool = (bool*)soap_malloc(s,sizeof(bool));
				*(ads.__ptr[ad_index].__ptr[attr_index].valueBool) = tmpbool;
				ads.__ptr[ad_index].__ptr[attr_index].type = 'b';
				break;
			case LX_NULL:
			case LX_UNDEFINED:
			case LX_ERROR:
					// if we cannot deal with this type, skip this attribute
				skip_attr = true;
				break;
			default:
					// assume everything else is some sort of expression
				tmpstr = NULL;
				rhs->PrintToNewStr( &tmpstr );
				if ( !tmpstr ) {
					skip_attr = true;
				} else {
					ads.__ptr[ad_index].__ptr[attr_index].value = tmpstr;
					ads.__ptr[ad_index].__ptr[attr_index].valueExpr = tmpstr;
					soap_link(s,(void*)tmpstr,0,1);
					ads.__ptr[ad_index].__ptr[attr_index].type = 'x';
				}
				break;
			}

				// skip this attr is requested to do so...
			if ( skip_attr ) continue;	

				// serialize the attribute name, and finally increment our counter.
			ads.__ptr[ad_index].__ptr[attr_index].name = ((Variable*)tree->LArg())->Name();
			attr_index++;
			ads.__ptr[ad_index].__size = attr_index;
		}
			
			// loop up to serialize the next ad
		ad_index++;		
	}

	ads.__size = ad_index;

	return SOAP_OK;
}

int condorCollector__queryStartdAds(struct soap *s,char *constraint,
	struct ClassAdStructArray & ads)
{
	int command = QUERY_STARTD_ADS;
	return receive_query_soap(command,s,constraint,ads);
}

int condorCollector__queryScheddAds(struct soap *s,char *constraint,
	struct ClassAdStructArray & ads)
{
	int command = QUERY_SCHEDD_ADS;
	return receive_query_soap(command,s,constraint,ads);
}

int condorCollector__queryMasterAds(struct soap *s,char *constraint,
	struct ClassAdStructArray & ads)
{
	int command = QUERY_MASTER_ADS;
	return receive_query_soap(command,s,constraint,ads);
}

int condorCollector__querySubmittorAds(struct soap *s,char *constraint,
	struct ClassAdStructArray & ads)
{
	int command = QUERY_SUBMITTOR_ADS;
	return receive_query_soap(command,s,constraint,ads);
}

int condorCollector__queryLicenseAds(struct soap *s,char *constraint,
	struct ClassAdStructArray & ads)
{
	int command = QUERY_LICENSE_ADS;
	return receive_query_soap(command,s,constraint,ads);
}

int condorCollector__queryStorageAds(struct soap *s,char *constraint,
	struct ClassAdStructArray & ads)
{
	int command = QUERY_STORAGE_ADS;
	return receive_query_soap(command,s,constraint,ads);
}

int condorCollector__queryAnyAds(struct soap *s,char *constraint,
	struct ClassAdStructArray & ads)
{
	int command = QUERY_ANY_ADS;
	return receive_query_soap(command,s,constraint,ads);
}


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
