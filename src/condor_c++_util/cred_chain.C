/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "cred_chain.h"
#include "condor_common.h"
#include "condor_string.h"
#include "globus_utils.h"
#include "signed_classads.h"
#include "condor_attributes.h"
#include "condor_debug.h"

bool
CredChain::initCredChainHandles() 
{
	if(!valid) return false;

	if(globus_gsi_cred_handle_attrs_init(&handle_attrs)) {
		dprintf(D_SECURITY, "problem during internal initialization.\n");
		valid = false;
		return false;
	}
	if(globus_gsi_cred_handle_init(&handle, handle_attrs)) {
		dprintf(D_SECURITY, "problem during internal initialization.\n" );
		valid = false;
		return false;
	}
	return true;
}

MyString *
CredChain::getFirstPolicy()
{
	MyString *rv = new MyString();
	if(!valid) {
		return rv;
	}
	STACK *policies;
	if(globus_gsi_cred_get_policies(handle, &policies)) {
		dprintf(D_SECURITY, "Problem getting policies.\n");
		return rv;
	}
	int j;
	for(j = 0; j < sk_num(policies); j++) {
		MyString policy = sk_value(policies, j);
		if((policy != "") && (policy != "GLOBUS_NULL_POLICY")) {
			delete(rv);
			rv = new MyString(policy);
			goto cleanup;
		}
	}
 cleanup:
	if(policies) {
		sk_free(policies);
	}
	return rv;

}
MyString *
CredChain::getLastPolicy()
{
	MyString *rv = new MyString();
	if(!valid) {
		return rv;
	}
	STACK *policies;
	if(globus_gsi_cred_get_policies(handle, &policies)) {
		dprintf(D_SECURITY, "Problem getting policies.\n");
		return rv;
	}
	int j;
	for(j = 0; j < sk_num(policies); j++) {
		MyString policy = sk_value(policies, j);
		if(policy != "") {
			delete(rv);
			rv = new MyString(policy);
		}
	}
// cleanup:
	if(policies) {
		sk_free(policies);
	}
	return rv;

}

char *
CredChain::getExecutionHostDN()
{
	if(!valid) {
		return NULL;
	}
	// Get SSP.
	MyString *ssp = getLastPolicy();

	// Check signature on SSP ClassAd.
	ClassAd ssp_ad;
	if(!text2classad(ssp->Value(), ssp_ad)) {
		dprintf(D_SECURITY, "Can't get classad from text.\n");
		delete(ssp);
		return NULL;
	}

	delete(ssp);
	StringList sl;
	sl.insert("ProxyPublicKey");
	sl.insert("Assertion");
	if(!verify_classad(ssp_ad, sl)) {
		dprintf(D_ALWAYS, "Error verifying certificate.\n");
		return NULL;
	}

	// Extract signing certificate.
	MyString cert;
	if(!ssp_ad.LookupString(ATTR_CLASSAD_SIGNATURE_CERTIFICATE, cert)) {
		dprintf(D_ALWAYS, "Can't get signature certificate.\n");
		return NULL;
	}

	// Verify certificate.
	char *buf = strdup(cert.Value());
	BIO *mem = BIO_new_mem_buf(buf, -1);
	if(!mem) {
		dprintf(D_SECURITY, "Error getting memory buffer for cert pub key.\n");
		free(buf);
		return NULL;
	}
	X509 *co = PEM_read_bio_X509(mem, NULL, NULL, NULL);
	if(!verify_certificate(co)) {
		dprintf(D_ALWAYS,"Can't verify signing certificate on SSP.\n");
		return NULL;
	}
	return X509_NAME_oneline(X509_get_subject_name(co), NULL, 1000);
	// Extract and return DN from certificate.
}


bool
CredChain::hasMatchingPolicy(MyString policy_to_match)
{
	if(!valid) {
		return false;
	}
	bool rv = false;
	STACK *policies;
	if(globus_gsi_cred_get_policies(handle, &policies)) {
		dprintf(D_SECURITY, "Problem getting policies.\n");
		return false;
	}
	int j;
	for(j = 0; j < sk_num(policies); j++) {
		MyString policy = sk_value(policies, j);
		if(policy == policy_to_match) {
			rv = true;
			goto cleanup;
		}
	}
 cleanup:
	if(policies) {
		sk_free(policies);
	}
	return rv;
}

int
CredChain::getNumPolicies()
{
	if(!valid) {
		return -1;
	}
	int rv = 0;
	STACK *policies;
	if(globus_gsi_cred_get_policies(handle, &policies)) {
		dprintf(D_SECURITY, "Problem getting policies.\n");
		return false;
	}
	int j;
	for(j = 0; j < sk_num(policies); j++) {
		MyString policy = sk_value(policies, j);
		if((policy != "") && (policy != "GLOBUS_NULL_POLICY")) {
			rv++;
		}
	}
	if(policies) {
		sk_free(policies);
	}
	dprintf(D_SECURITY, "Policy count: %d\n", rv);
	return rv;
}

CredChain::CredChain(const char *proxy_file_path) 
{
	valid = true;
	if( check_x509_proxy(proxy_file_path) != 0 ) {
		dprintf(D_SECURITY, "Error with proxy file.\n");
		valid = false;
		return;
	}
	initCredChainHandles();
	if(globus_gsi_cred_read_proxy(handle, proxy_file_path)) {
		dprintf(D_SECURITY, "problem reading credential.\n");
		valid = false;
	}
}

CredChain::CredChain(const MyString full_chain) {
	valid = true;
	// Hack to activate_globus_gsi();
	char *proxy_file_name = get_x509_proxy_filename();
	if(proxy_file_name) {
		free(proxy_file_name);
	}
	BIO *bp;
	char *tmp = strdup(full_chain.Value());
	if(!buffer_to_bio(tmp, full_chain.Length(), &bp)) {
		dprintf(D_SECURITY, "Error converting buffer to bio.\n");
		valid = false;
		free(tmp);
		return;
	}
	initCredChainHandles();
	if(globus_gsi_cred_read_proxy_bio(handle, bp)) {
		valid = false;
		dprintf(D_SECURITY, "Problem reading credential text.\n");
	}
}
