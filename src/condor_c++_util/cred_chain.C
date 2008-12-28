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

/**
 * Handle attributes.
 * @ingroup globus_gsi_credential_handle_attrs
 */

/**
 * GSI Credential handle attributes implementation
 * @ingroup globus_gsi_credential_handle
 * @internal
 *
 * This structure contains immutable attributes
 * of a credential handle
 */
typedef struct globus_l_gsi_cred_handle_attrs_s
{
    /* the order to search in for a certificate */
    globus_gsi_cred_type_t *            search_order; /*{PROXY,USER,HOST}*/
} globus_i_gsi_cred_handle_attrs_t;

/**
 * GSI Credential handle implementation
 * @ingroup globus_gsi_credential_handle
 * @internal
 *
 * Contains all the state associated with a credential handle, including
 *
 * @see globus_credential_handle_init(), globus_credential_handle_destroy()
 */
typedef struct globus_l_gsi_cred_handle_s
{
    /** The credential's signed certificate */
    X509 *                              cert;
    /** The private key of the credential */
    EVP_PKEY *                          key;
    /** The chain of signing certificates */
    STACK_OF(X509) *                    cert_chain;
    /** The immutable attributes of the credential handle */
    globus_gsi_cred_handle_attrs_t      attrs;
    /** The amout of time the credential is valid for */
    time_t                              goodtill;
} globus_i_gsi_cred_handle_t;

bool
CredChain::isValid()
{
	return valid;
}

bool
CredChain::initCredChainHandles()
{
	if(!valid) return false;
	handle_attrs = NULL;
	handle = NULL;

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
/*
MyString *
CredChain::get_cert_policy()
{
	MyString *rv = new MyString();
	if(!valid) {
		return rv;
	}
	PROXYCERTINFO pci = get_proxycertinfo(handle->cert);
	PROXY_POLICY policy = PROXYCERTINFO_get_policy(pci);
	policy_string = PROXYPOLICY_get_policy(policy, &policy_string_length);

	}*/

MyString
CredChain::getLastPolicy()
{
	MyString rv;
	if(!valid) {
		return rv;
	}
	STACK *policies;
	if(globus_gsi_cred_get_policies(handle, &policies)) {
		dprintf(D_SECURITY, "Problem getting policies.\n");
		return rv;
	}
	int j;
/*	for(j = 0; j < sk_num(policies); j++) {
		MyString policy = sk_value(policies, j);
		if((policy != "") && (policy != "GLOBUS_NULL_POLICY") && (policy != "(null)") && (policy != NULL)) {
			rv = policy;
			goto cleanup;
		}
	}
*/
	rv = sk_value(policies, 0);
 cleanup:
	if(policies) {
		sk_free(policies);
	}
	return rv;
}
MyString
CredChain::getFirstPolicy()
{
	MyString rv;
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
		if((policy != "") && (policy != "GLOBUS_NULL_POLICY") && (policy != "(null)") && (policy != NULL)) {
			rv = policy;
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
	MyString sspf = getLastPolicy();

	// Check signature on SSP ClassAd.
	char *ad = strdup(sspf.GetCStr());
	dprintf(D_SECURITY,"Last policy: '%s'\n", ad);
	char *sig = rindex(ad, ';');
	if(sig) {
		*sig = '\0';
		++sig;
	}
	sspf = ad;
	//free(ad);
	MyString ssp = unquote_classad_string(sspf);
	ClassAd ssp_ad;
	if(!text2classad(ssp.Value(), ssp_ad)) {
		dprintf(D_SECURITY, "Can't get classad from text.\n");
		return NULL;
	}
	ssp_ad.Assign(ATTR_CLASSAD_SIGNATURE_TEXT, ad);
	ssp_ad.Assign(ATTR_CLASSAD_SIGNATURE, sig);

	StringList sl;
	sl.insert("ProxyPublicKey");
	sl.insert("Assertion");
	if(!verify_classad(ssp_ad, sl)) {
		dprintf(D_ALWAYS, "Error verifying classad.\n");
		return NULL;
	}// Shouldn't we verify that the proxy public key is the one we have in hand?


	// Extract signing certificate.
	MyString cert;
	if(!ssp_ad.LookupString(ATTR_CLASSAD_SIGNATURE_CERTIFICATE, cert)) {
		dprintf(D_ALWAYS, "Can't get signature certificate.\n");
		return NULL;
	}

	// Verify certificate.
	MyString uqcert = unquote_classad_string(cert);
	char *buf = strdup(uqcert.Value());
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
	if(globus_gsi_cred_get_policies(handle, &policies) != GLOBUS_SUCCESS) {
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
	dprintf(D_SECURITY, "Number of credentials (total): %d\n", sk_num(policies));
	for(j = 0; j < sk_num(policies); j++) {
		MyString policy = sk_value(policies, j);
		//dprintf(D_SECURITY, "Policy: '%s'\n", policy.Value());
		if(!((policy.GetCStr() == NULL) ||
			 (policy == "(null)") ||
			 (policy == "") ||
			 (policy == "GLOBUS_NULL_POLICY"))) {
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
	dprintf(D_SECURITY, "Instantiating CredChain with path '%s'\n", proxy_file_path);
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
	// Add top cert to chain:
	// (This is a total hack.)
	sk_X509_insert(handle->cert_chain, X509_dup(handle->cert), sk_num(handle->cert_chain)+1);
}

/*CredChain::CredChain(const MyString full_chain) {
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
	// Add top cert to chain:
	// (This is a total hack.)
	sk_X509_insert(handle->cert_chain,handle->cert, sk_num(handle->cert_chain)+1);
	//dprintf(D_SECURITY, "Got credential '%s'\n", tmp);
	free(tmp);
	}*/

CredChain::CredChain(const MyString full_chain) {
	// Hack to activate_globus_gsi();
	char *proxy_file_name = get_x509_proxy_filename();
	BIO *bp;
	int i = 0;
	X509 *tmp_cert = NULL;
	const char *chain = full_chain.Value();
	char *tmp = strdup(chain);

	valid = true;

	dprintf(D_SECURITY, "Instantiating CredChain with chain '%s'\n", full_chain.Value());
	if(proxy_file_name) {
		free(proxy_file_name);
	}

	if(!tmp) {
		dprintf(D_SECURITY, "Error allocating memory.\n");
		valid = false;
		return;
	}

	if(!buffer_to_bio(tmp, strlen(tmp), &bp)) {
		dprintf(D_SECURITY, "Error converting buffer to bio.\n");
		goto error_exit;
	}
	initCredChainHandles();
	if(handle->cert_chain != NULL) { // Shouldn't be necessary.
		sk_X509_pop_free(handle->cert_chain, X509_free);
		handle->cert_chain = NULL;
	}
	if((handle->cert_chain = sk_X509_new_null()) == NULL) {
		dprintf(D_SECURITY, "Error creating stack for certificates.\n");
		goto error_exit;
	}
	while(!BIO_eof(bp)) {
		tmp_cert = NULL;
		if(!PEM_read_bio_X509(bp, &tmp_cert, NULL, NULL)) {
			break;
		}
		if(!sk_X509_insert(handle->cert_chain, X509_dup(tmp_cert), i)) {
			X509_free(tmp_cert);
			dprintf(D_SECURITY, "Error inserting cert into stack.\n");
			goto error_exit;
		}
		++i;
	}
	free(tmp);
	return;
 error_exit:
	valid = false;
	free(tmp);
	return;
}

