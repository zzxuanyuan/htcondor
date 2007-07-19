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

#include <sys/mman.h>
//#include <unistd.h>
#include <sys/types.h>

#include "soap_submitH.h"
#include "soap_submitStub.h"
#include "ns.nsmap"
#include "smdevp.h"
#include "wsseapi.h"
#include "condor_common.h"
#include "condor_classad.h"
#include "condor_classad_util.h"
#include "condor_attributes.h"
#include "list.h"

#include "../condor_c++_util/soap_helpers.cpp"

// Four megs enough for you?
#define SCA_MEMBUF_SIZE 4194304

template class List<ClassAd>;
template class Item<ClassAd>;

//TODO: close, free, etc.

char *quote_classad_string(char *input) {
	int len = strlen(input)+1;
	int i, ctr = 0;
	char *output;

	if(len < 1) {
		dprintf(D_SECURITY, "Error in quote_classad_string: short input.\n");
		return 0;
	}
	output = (char *)malloc(len*2);
	if(output == NULL) {
		dprintf(D_SECURITY, "Malloc error.\n");
		return 0;
	}
	memset(output, 0, len*2);
	for(i=0; i <= len; i++) {
	    switch(input[i]) {
		case '\n':
			output[ctr++] = '\\';
			output[ctr] = 'n';
			break;
		case '\"':
			output[ctr++] = '\\';
			output[ctr] = '\"';
			break;
		case '\\':
			output[ctr++] = '\\';
			output[ctr] = '\\';
		default:
			output[ctr] = input[i];
			break;
		}
		++ctr;
	}
	return output;
}
char *unquote_classad_string(char *input) {
	int len = strlen(input)+1;
	int i, ctr = 0;
	char *output;

	if( len < 1 ) {
		dprintf(D_SECURITY, "Error in unquote_classad_string: short input.\n");
		return 0;
	}
	output = (char *)malloc(len);
	if(output == NULL) {
		dprintf(D_SECURITY, "Malloc error.\n");
		return 0;
	}
	memset(output, 0, len);
	for(i = 0; i <= len; i++) {
		if(input[i] == '\\') {
			switch(input[i+1]) {
			case 'n':
				output[ctr] = '\n';
				++i;
				break;
			case '"':
				output[ctr] = '"';
				++i;
				break;
			case '\\':
				output[ctr] = '\\';
				++i;
				break;
			default:
				output[ctr] = '\\';
				break;
			}
		} else {
			output[ctr] = input[i];
		}
		++ctr;
	}
	return output;
}

MyString getClassAdSig(ClassAd *ad)
{
    struct soap *soap;
    soap = soap_new();
	char tmp_file[] = "/tmp/.condor_scaXXXXXX";
	int tmp_fd;
	char *sca_mem;
	struct condor__ClassAdStruct *adStruct;
	struct _condor__signatureRequest req;
	char *quoted_sca;
	MyString rv=NULL;

    soap_register_plugin(soap, soap_wsse);
    //soap_omode(&soap, SOAP_ENC_ZLIB | SOAP_XML_GRAPH); // see 8.12
    //soap_omode(soap, SOAP_ENC_XML | SOAP_XML_GRAPH | SOAP_XML_INDENT);
    soap_omode(soap, SOAP_ENC_XML | SOAP_XML_GRAPH | SOAP_XML_CANONICAL);

	tmp_fd = mkstemp(tmp_file);
	if(tmp_fd == -1) {
		perror("mkstemp");
		dprintf(D_SECURITY, "Couldn't create temp file in signClassAd.\n");
		goto return_error;
	}
	
	soap->sendfd = tmp_fd;
	sca_mem = (char *)mmap(0, SCA_MEMBUF_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE, tmp_fd, 0);
	if(sca_mem == MAP_FAILED) {
		perror("mmap");
		fprintf(stderr, "Dying: mmap in signClassAd.\n");
		}	
			
    soap_begin(soap);
		//soap_wsse_add_Security(&soap);
    //soap_wsse_add_Timestamp(&soap, "Time", 1000);
    static char hmac_key[16] =
        { 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88,
          0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00 };
    if(soap_wsse_sign_body(soap, SOAP_SMD_HMAC_SHA1,
                           hmac_key, sizeof(hmac_key))) {
        soap_print_fault(soap, stderr);
    }	
	
    soap_wsse_verify_auto(soap, SOAP_SMD_HMAC_SHA1, hmac_key, sizeof(hmac_key));
	adStruct = (struct condor__ClassAdStruct *)
		soap_malloc(soap, sizeof(struct condor__ClassAdStruct));
	
	convert_ad_to_adStruct(soap, ad, adStruct, true);
	req.classAd = adStruct;
    if(soap_send___condor__signature(soap, "http://", NULL, &req) == SOAP_OK) {
//    if(soap_call___ns1__successor(soap, "http://ferdinand.cs.wisc.edu:31310", NULL, x, &succ) == SOAP_OK) {
		soap_end_send(soap);
		soap_closesock(soap);
		soap_end(soap);
		soap_done(soap);
		free(soap);
		close(tmp_fd);
		dprintf(D_SECURITY, "Signature length: %d\n", strlen(sca_mem));
		dprintf(D_SECURITY, "Signature bytes: %d %d %d\n", *sca_mem, *(sca_mem+1), *(sca_mem+2));
		quoted_sca = quote_classad_string(sca_mem);
		if(!quoted_sca) {
			dprintf(D_SECURITY, "Error quoting string '%s'.\n",sca_mem);
			goto return_error;
		}
		//InsertIntoAd(ad,"ClassAdSignature",*quoted_sca);
		rv = MyString(quoted_sca);
		free(quoted_sca);
		return rv;
    } else {
		dprintf(D_SECURITY, "Problem with soap_send___condor__signature.\n");
        soap_print_fault(soap, stderr);
    }
//    soap_destroy(soap)
 return_error:
    soap_end(soap);
    soap_done(soap);
    free(soap);
    return rv;
}

ClassAd limitClassAd(ClassAd *ad, StringList *include) 
{
	ClassAd rv(*ad);
	ExprTree *ad_expr;
	char *attr_name;
	
	if(!include) {
		return rv;
	}
	ad->ResetExpr();
	//rv.ResetExpr();

	while( (ad_expr = ad->NextExpr()) ) {
		attr_name = ((Variable*)ad_expr->LArg())->Name();
		if(!include->contains_anycase(attr_name)) {
			// TODO: do we have to test for presence before deleting?
			rv.Delete(attr_name);
		}
	}
	// Suggestion: check the include list for attributes not found in the ad?
	return rv;
}
    
void signClassAd(ClassAd *ad, StringList *include, MyString &signature)
{
	ClassAd new_ad = limitClassAd(ad, include);
	signature = getClassAdSig(&new_ad);
}

