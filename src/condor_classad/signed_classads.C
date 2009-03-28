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
#include "MyString.h"
#include "condor_debug.h"
#include "condor_classad.h"
#include "condor_config.h"
#include "condor_arglist.h"
#include "condor_attributes.h"
#include "openssl_helpers.h"
#include "condor_auth_x509.h"

#include "globus_utils.h"
#if defined(HAVE_EXT_GLOBUS)
#     include "globus_gsi_credential.h"
#     include "globus_gsi_system_config.h"
#     include "globus_gsi_system_config_constants.h"
#     include "gssapi.h"
#     include "globus_gss_assist.h"
#     include "globus_gsi_proxy.h"
#endif


#if defined(HAVE_EXT_OPENSSL) || defined(HAVE_EXT_GLOBUS)
#include "openssl/evp.h"
#include "openssl/err.h"
#include "openssl/pem.h"
#include "condor_auth_ssl.h"
#include "openssl/bio.h"

void
print_ad(ClassAd ad)
{
	MyString debug;
	ad.sPrint(debug);
	dprintf(D_SECURITY, "AD: '%s'\n",debug.Value());
}

/*
 * get_bits
 *
 * Given ascii hex, return actual binary value.
 *
 * returns binary value of input char or 0 if not in range.
 */
inline char
get_bits(char in)
{
    if(in >= '0' && in <= '9') {
        return in-'0';
    }
    if(in >= 'a' && in <= 'f') {
        return in-'a'+10;
    }
    return 0;
}

/*
 * bin_2_hex
 *
 * Given a binary input (sequence of bytes), convert it to ascii hex.
 *
 * Returns true indicating success.
 */
bool
bin_2_hex(unsigned char *input, unsigned int input_len, MyString& output)
{
    static const char ct[] = "0123456789abcdef";
    unsigned int i = 0;
    for(i = 0; i < input_len; i++) {
        int h1 = 0x0f&(input[i]>>4);
        int h2 = 0x0f&input[i];
		output += ct[h1];
		output += ct[h2]; // TODO will this work?
    }
    return true;
}

/*
 * hex2bin
 *
 * Given a hex string (such as a "quoted" signature), return the raw
 * bytes, and the length of those raw bytes.
 *
 * Modifies the length and output parameters.  Returns
 * false if there's an error, or true.
 */
// bool hex_2_bin(const MyString& input, int *len, unsigned char *& output);

bool
hex_2_bin(const MyString &input, int &len, unsigned char *& output)
{
	len = input.Length()/2;
	const char *inp = input.Value();
	output = (unsigned char *)malloc(len);
	if(output == NULL) {
		perror("malloc");
		dprintf(D_SECURITY, "Can't malloc.\n");
		return false;
	}
	int i;
	for(i = 0; i < len; i++) {
		char a = get_bits(inp[i*2]);
		char b = get_bits(inp[i*2+1]);
		output[i] = (a<<4)|b;
	}
	return true;
}

/*
 * sign_data
 *
 * Given a private key, and a string (text of a classad), sign the
 * string using a sha-1 digest.  Put the result in sig_buf, and the
 * length of the result in sig_len.
 *
 * Returns: true for success
 */
bool sign_data(EVP_PKEY *pkey,
			   const MyString& data,
			   MyString &signature);

bool sign_data(EVP_PKEY *pkey,
			   const MyString& data,
			   MyString &signature)
{
	EVP_MD_CTX ctx;
	const char * const data_buf = data.Value();
	int data_len = data.Length();
	while(data_len >= 0 && data_buf[data_len] == 0) { --data_len; }
	if(data_len < 1) {
		dprintf(D_ALWAYS, "Can't sign zero length data!\n");
		return false;
	}
	unsigned char *sig_buf;
	unsigned int sig_len;
	MyString hex_sig_buf;

	if(!EVP_SignInit(&ctx, EVP_sha1())) {
        report_openssl_errors("sign_data");
		return false;
    }
	dprintf(D_SECURITY, "Length: %d: '%s'\n", data_len, data_buf);
    if(!EVP_SignUpdate(&ctx, data_buf, data_len)) {
		report_openssl_errors("sign_data");
		return false;
    }

    sig_len = EVP_PKEY_size((EVP_PKEY *)pkey);
	if(sig_len < 1) {
		report_openssl_errors("sign_data");
        return false;
	}
	dprintf(D_SECURITY, "Result length: %d\n", sig_len);
    sig_buf = (unsigned char *)malloc(sig_len);
    if(!sig_buf) {
	    fprintf(stderr, "Got error from malloc.\n");
	    return false;
	}
    if(!EVP_SignFinal(&ctx, sig_buf, &sig_len, pkey)) {
		report_openssl_errors("sign_data");
		free(sig_buf);
        return false;
    }
	if(!bin_2_hex(sig_buf, sig_len, hex_sig_buf)) {
		dprintf(D_SECURITY, "Couldn't make hex from binary signature.\n");
		free(sig_buf);
		return false;
	}
	/*
	unsigned char *test;
	int len;
	hex_2_bin(hex_sig_buf, len, test);
	if( len == (int)sig_len && !strncmp((const char *)sig_buf, (const char *)test, sig_len) ) {
		dprintf(D_SECURITY, "Error!\n");
	} else {
		dprintf(D_SECURITY, "No Error!  Length is %d\n", len);
		}*/
	signature = hex_sig_buf;
	dprintf(D_SECURITY, "Got signature: '%s'\n", signature.Value());
	//free(hex_sig_buf);
	free(sig_buf);
	return true;
}

/*
 * get_private_key
 *
 * Given a path to a file containing a private key, extract the key
 * and return it.  Assumes that the private key is stored unencrypted.
 *
 * Returns: the private key, or NULL if there was an error.  The
 * caller frees this key with EVP_PKEY_free().
 */
EVP_PKEY *get_private_key(const MyString& filename);

EVP_PKEY *
get_private_key(const MyString& filename)
{
	FILE *fd;
	EVP_PKEY *pkey;

    fd = safe_fopen_wrapper(filename.Value(), "r");
    if(!fd) {
        dprintf(D_SECURITY, "Can't open private key.\n");
        return NULL;
    }
    pkey = PEM_read_PrivateKey(fd, NULL, NULL, NULL);
	fclose(fd);
    if(!pkey) {
		report_openssl_errors("get_private_key");
    }
	return pkey;
}

/*
 * get_file_text
 *
 * Read all the text in a file.
 *
 * Returns false if the file couldn't be opened, otherwise true.
 */
bool
get_file_text(const MyString &filename, MyString &text)
{
	FILE *fd;
	fd = safe_fopen_wrapper(filename.Value(),"r");
	if(!fd) {
		dprintf(D_SECURITY, "Can't open certificate file.\n");
		return false;
	}
	MyString line;
	while(line.readLine(fd)) {
		text += line;
	}
	fclose(fd);
	return true;
}

bool
get_cert_text(const MyString &filename, MyString &text)
{
	MyString full;
	if(!get_file_text(filename, full)) {
		return false;
	}
	bool in_key = false;
	full.Tokenize();
	text = "";
	const char *tmp = NULL;
	while((tmp = full.GetNextToken("\n",false))) {
		//dprintf(D_SECURITY, "get_cert_text: got line '%s'\n", tmp);
		if(strcmp(tmp,"-----BEGIN RSA PRIVATE KEY-----") == 0) {
			in_key = true;
			continue;
		}
		if(strcmp(tmp,"-----END RSA PRIVATE KEY-----") == 0) {
			in_key = false;
			continue;
		}
		if(in_key) {
			continue;
		}
		text += tmp;
		text += "\n";
	}
	return true;
}

/*
 * get_public_key
 *
 * Given a path to an x.509 certificate file, extract the public key
 * and return it.
 *
 * Returns: the public key, or NULL in case of error.  Caller frees.
 */
EVP_PKEY *get_public_key(const MyString& filename);

EVP_PKEY *
get_public_key(const MyString& filename)
{
	FILE *fd;
	EVP_PKEY *pkey = NULL;
	X509 *x509 = NULL;

    fd = safe_fopen_wrapper(filename.Value(), "r");
    if(!fd) {
        dprintf(D_SECURITY, "Can't open public key: '%s'.\n",
				filename.Value());
        return NULL;
    }

    x509 = PEM_read_X509(fd, NULL, NULL, NULL);
	fclose(fd);
    if(!x509) {
		report_openssl_errors("get_public_key");
        return NULL;
    }

    pkey = X509_get_pubkey(x509);
    if(!pkey) {
		report_openssl_errors("get_public_key");
    }
	X509_free(x509);
	return pkey;
}

EVP_PKEY *
get_public_key_from_text(const MyString &text)
{
	BIO *mem;
	char *buf = text.StrDup();
	if(buf == NULL || buf[0] == '\0') {
		dprintf(D_SECURITY, "Can't get text of public key.\n");
		return NULL;
	}
	mem = BIO_new_mem_buf(buf, -1);
	if(!mem) {
		dprintf(D_SECURITY, "Error getting memory buffer for cert pub key.\n");
		free(buf);
		return NULL;
	}
	X509 *s_cert = PEM_read_bio_X509(mem, NULL, NULL, NULL);
	if(!s_cert) {
		report_openssl_errors("get_public_key_from_text");
		BIO_free(mem);
		free(buf);
		return NULL;
	}
	EVP_PKEY *rv = X509_extract_key(s_cert);
	if(!rv) {
		report_openssl_errors("get_public_key_from_text");
	}
	BIO_free(mem);
	free(buf);
	X509_free(s_cert);
	return rv;
}

/*
 * verify_signature
 *
 * Given a public key, a string, and a signature (with associated
 * length), verify that the associated private key performed the
 * signature on the string.
 *
 * Returns true if the signature verifies correctly.
 */
bool verify_signature(EVP_PKEY *pkey, const MyString &data,
					  const MyString &signature);


bool
verify_signature(EVP_PKEY *pkey, const MyString &data,
				 const MyString &signature)
{
	EVP_MD_CTX ctx;

	const char *const data_buf = data.Value();
	int data_len = data.Length();
	while(data_len >= 0 && data_buf[data_len] == 0) { --data_len; }
	if(data_len < 1) {
		dprintf(D_SECURITY, "Can't sign zero length data!\n");
		return false;
	}

	unsigned char *bin_sig;
	int bin_sig_len;
	if(!hex_2_bin(signature, bin_sig_len, bin_sig)) {
		dprintf(D_SECURITY, "Error with hex signature.\n");
		return false;
	}

	if(!EVP_VerifyInit(&ctx, EVP_sha1())) {
		report_openssl_errors("verify_signature");
		free(bin_sig);
		return false;
	}
	dprintf(D_SECURITY, "Length: %d: '%s'\n", data_len, data_buf);
	if(!EVP_VerifyUpdate(&ctx, data_buf, data_len)) {
		report_openssl_errors("verify_signature");
		dprintf(D_SECURITY, "Error in VerifyUpdate.\n");
		free(bin_sig);
		return false;
	}
	if(!EVP_VerifyFinal(&ctx, bin_sig, bin_sig_len, pkey)) {
		report_openssl_errors("verify_signature");
		dprintf(D_SECURITY, "Error in VerifyFinal.\n");
		free(bin_sig);
		return false;
	}
	free(bin_sig);
	return true;
}

/*
 * classad2text
 *
 * Given a classAd, convert it to text.  This does minimal
 * canonicalization, so that the same classAd should always produce
 * the same text.
 *
 * Returns a compact representation of the classAd, or null in case of
 * error.
 */
MyString classad2text(const ClassAd& classAd);

MyString
classad2text(ClassAd &classAd)
{
	MyString output;
	if(classAd.sPrint(output)) {
		return output;
	} else {
		return "";
	}
}

/*
 * text2classad
 *
 * Given the text of the classad, convert it to the actual classad.
 *
 * Returns the classAd.
 */
bool
text2classad(const MyString &text, ClassAd &ad)
{
	char *buf = text.StrDup();
	if(buf == NULL || buf[0] == '\0') {
		dprintf(D_SECURITY, "Couldn't get text of classad.\n");
		return false;
	}
	ad = ClassAd(buf, '\n');
	free(buf);
	return true;
}

/*
 * Given a classad, prepare the "Arguments" attribute.  In existing
 * systems, this attribute is not prepared at the time of submission,
 * but rather at the time when a match is made.  This is due to some
 * change in the way arguments are prepared on different platforms (or
 * were, long, long ago --Dan Bradley tells me this exists for
 * backwards compatibility.  Anyway, now we do it here so that what
 * gets signed by the user will be the same as what gets verified by
 * the starter.
 */
bool prepare_arguments(ClassAd *ad)
{
	ArgList al;
	MyString error;
	MyString args2;
	if(!al.AppendArgsFromClassAd(ad, &error)) {
		dprintf(D_SECURITY, "Couldn't get arguments from ClassAd.\n");
		dprintf(D_SECURITY, "Error: '%s'\n", error.Value());
		return false;
	}
	if(!al.GetArgsStringV2Raw(&args2,&error)) {

//	if(!al.InsertArgsIntoClassAd(ad, NULL, &error)) { // this doesn't work
		dprintf(D_SECURITY, "Couldn't get arguments for ClassAd.\n");
		dprintf(D_SECURITY, "Error: '%s'\n", error.Value());
		return false;
	}
	ad->Assign(ATTR_JOB_ARGUMENTS2,args2.Value());
	/* MyString arg;
	   al.GetArgsStringForDisplay(&arg);
	   fprintf(stderr, "Arglist: '%s'\n", arg.Value());
	 */
	//print_ad(*ad);
	return true;
}

void
limit_classad(ClassAd &in_ad,
			  ClassAd *cached_ad,
			  StringList &include,
			  ClassAd &out_ad)
{
	out_ad = in_ad;
	ExprTree *ad_expr;
	char *attr_name;
	if(include.isEmpty()) {
		dprintf(D_SECURITY, "WARNING: Empty list of attributes to sign.\n");
		return;
	}
	include.rewind();
	while( (attr_name = include.next()) ) {
		dprintf(D_SECURITY, "Trying to add attribute %s to out_ad.\n", attr_name);
		if(!out_ad.Lookup(attr_name)) {
			if(stricmp(attr_name,"Arguments")) {
				if(cached_ad && cached_ad->Lookup(attr_name)) {
					dprintf(D_SECURITY, "Adding attribute from cached ad: %s\n", attr_name);
					ExprTree *t = cached_ad->Lookup(attr_name);
					if(t) {
						ExprTree *n = t->DeepCopy();
						out_ad.Insert(n, true);
					} else {
						dprintf(D_ALWAYS, "This shouldn't happen: lookup works once but fails a second time.\n");
					}
				} else {
					dprintf(D_SECURITY, "WARNING: "
							"attribute '%s' missing from input, "
							"not included in signature.\n", attr_name);
				}
			}
		}
	}
	in_ad.ResetExpr();
	while( (ad_expr = in_ad.NextExpr()) ) {
		attr_name = ((Variable *)ad_expr->LArg())->Name();
		if(stricmp(attr_name, "Args"))  { // delete this later in sign_classad
			if(!include.contains_anycase(attr_name)) {
				out_ad.Delete(attr_name);
			}
		}
	}
	return;
}

bool
verify_same_subset_attributes(const ClassAd &jobAd,
							  const ClassAd &sigAd, StringList &subset)
{
	//print_ad(jobAd);
	//print_ad(sigAd);
	ExprTree *jobAdExpr, *sigAdExpr;
	char *attr_name;
	subset.rewind();
	while( (attr_name = subset.next()) ) {
		jobAdExpr = jobAd.Lookup(attr_name);
		sigAdExpr = sigAd.Lookup(attr_name);
        if( ! jobAdExpr ) {
            dprintf(D_SECURITY,
                    "Job Ad doesn't contain attribute '%s'.\n",
                    attr_name);
            return false;
        }
        if( ! sigAdExpr ) {
            dprintf(D_SECURITY,
                    "Signature doesn't contain attribute '%s'.\n",
                    attr_name);
            return false;
        }
        if( !(*jobAdExpr == *sigAdExpr) ) {
            dprintf(D_SECURITY,
                    "Job attribute '%s' doesn't match signature attribute.\n",
                    attr_name);
            return false;
        }
    }
	return true;
}

/* Opposite of quote_classad_string: convert the input
 * a classad value to output same as gsoap serialization.
 */
MyString
unquote_classad_string(const MyString &input)
{
	int len = input.Length();
	int i;

	MyString output = "";
	if( len < 1 ) {
		dprintf(D_SECURITY, "Can't unquote short input.\n");
		return output;
	}

	for(i = 0; i < len; i++) {
		if(input[i] == '\\') {
			switch(input[i+1]) {
			case 'n':
				output += '\n';
				++i;
				break;
			case '"':
				output += '"';
				++i;
				break;
			case '\\':
				output += '\\';
				++i;
				break;
			default:
				output += '\\';
				break;
			}
		} else {
			output += input[i];
		}
	}
	return output;
}

/* Convert the input (the output of gsoap serialization)
 * to a format appropriate for using as a classad value.
 */
MyString
quote_classad_string(MyString input)
{
	int len = input.Length();
	int i;
	MyString output = "";
	if(len < 1) {
		dprintf(D_SECURITY, "Can't quote short input.\n");
		return output;
	}
	for(i=0; i <= len; i++) {
	    switch(input[i]) {
		case '\n':
			output += '\\';
			output += 'n';
			break;
		case '\"':
			output += '\\';
			output += '\"';
			break;
		case '\\':
			output += '\\';
			output += '\\';
			break;
		default:
			output += input[i];
			break;
		}
	}
	return output;
}

/* These are exposed functions. */

bool
sign_classad(ClassAd &ad,
			 ClassAd *cached_ad,
			 StringList &attributes_to_sign,
			 const MyString &private_key_path,
			 const MyString &public_key_path)
{
	ClassAd sign_subset;
	limit_classad(ad, cached_ad, attributes_to_sign, sign_subset);

	// Attribute "Arguments" is handled differently; see condor_arglist.h
	// We skip deleteing "Args" in limit_classad so we can use it in
	// prepare_arguments below, then delete it after if it's not to be signed.
	if(attributes_to_sign.contains("Arguments")
	   && !prepare_arguments(&sign_subset)) {
		return false;
	}
	if(!attributes_to_sign.contains("Args")) {
		if(sign_subset.Lookup("Args")) {
			sign_subset.Delete("Args");
		}
	}
	EVP_PKEY *priv;
	EVP_PKEY *pub;
	priv = get_private_key(private_key_path);
	if(priv == NULL) {
		dprintf(D_SECURITY, "Couldn't get private key for file '%s'.\n",
				private_key_path.Value());
		return false;
	}
	pub = get_public_key(public_key_path);
	if(pub == NULL) {
		EVP_PKEY_free(priv);
		dprintf(D_SECURITY, "Couldn't get public key for file '%s'.\n",
				public_key_path.Value());
		return false;
	}
	// add certificate text to classad
	MyString cert_text = "";
	dprintf(D_SECURITY, "Getting certificate text from file '%s'\n", public_key_path.Value());
	if(!get_cert_text(public_key_path, cert_text)) {
		dprintf(D_SECURITY, "Couldn't get public key text for file '%s'.\n",
				public_key_path.Value());
		EVP_PKEY_free(priv);
		EVP_PKEY_free(pub);
		return false;
	}
	dprintf(D_SECURITY, "Signing using key in file '%s'\n", private_key_path.Value());
	dprintf(D_FULLDEBUG, "Adding text: '%s'\n", cert_text.Value());
	MyString quoted_cert_text = quote_classad_string(cert_text);
	sign_subset.Assign(ATTR_CLASSAD_SIGNATURE_CERTIFICATE, quoted_cert_text);
	// add version information for signature "1.0a"
	MyString version_info = "1.0a";
	sign_subset.Assign(ATTR_CLASSAD_SIGNATURE_VERSION, version_info);

	MyString text_to_sign = quote_classad_string(classad2text(sign_subset));
	MyString signature_text = "";
	if(text_to_sign.Length() < 1) {
		dprintf(D_SECURITY, "Couldn't prepare text for signing.\n");
		EVP_PKEY_free(priv);
		EVP_PKEY_free(pub);
		return false;
	}
//	dprintf(D_SECURITY, "Here's the text we're signing: '%s'\n", text_to_sign.Value());
	if(!sign_data(priv, text_to_sign, signature_text)) {
		dprintf(D_SECURITY, "Couldn't sign data.\n");
		EVP_PKEY_free(priv);
		EVP_PKEY_free(pub);
		return false;
	}
	ad.Assign(ATTR_CLASSAD_SIGNATURE_TEXT, text_to_sign);
	ad.Assign(ATTR_CLASSAD_SIGNATURE, signature_text);
	//dprintf(D_SECURITY, "Signing text '%s'\n", text_to_sign.Value());

	EVP_PKEY_free(priv);
	EVP_PKEY_free(pub);
	return true;
}

bool
verify_classad(ClassAd& ad,
			   StringList& attributes_to_verify)
{
	MyString signed_text;
	MyString signature;
	MyString adtext;
	ad.sPrint(adtext);
	dprintf(D_SECURITY, "Got ad: %s\n", adtext.Value());
	if(!ad.LookupString(ATTR_CLASSAD_SIGNATURE_TEXT, signed_text)) {
		dprintf(D_SECURITY, "Can't find signed text in signed classad.\n");
		return false;
	}
//	dprintf(D_SECURITY, "Verify text: '%s'\n", signed_text.Value());
	if(!ad.LookupString(ATTR_CLASSAD_SIGNATURE, signature)) {
		dprintf(D_SECURITY, "Can't find signature in signed classad.\n");
		return false;
	}
	dprintf(D_SECURITY, "Verify signature: '%s'\n", signature.Value());
	ClassAd sad;
	MyString sad_string = unquote_classad_string(signed_text);
//	dprintf(D_SECURITY, "Signed string: '%s'\n", sad_string.Value());
	if(!text2classad(sad_string, sad)) {
		dprintf(D_SECURITY, "Can't make signed text into ad.\n");
		return false;
//	} else {
//		MyString saddebug = "";
//		sad.sPrint(saddebug);
//		dprintf(D_SECURITY, "Signed ad is: '%s'\n", saddebug.Value());
	}
	MyString version_info;
	MyString cert_text;
	if(!sad.LookupString(ATTR_CLASSAD_SIGNATURE_VERSION, version_info)) {
		dprintf(D_SECURITY, "Can't get signature version from ad.\n");
		return false;
	}
	if(version_info != "1.0a") {
		dprintf(D_SECURITY, "Can't verify signature version '%s'.\n",
				version_info.Value());
		return false;
	}
	if(!sad.LookupString(ATTR_CLASSAD_SIGNATURE_CERTIFICATE, cert_text)) {
		dprintf(D_SECURITY, "Can't get certificate text from ad.\n");
		return false;
	}
	EVP_PKEY *pub = get_public_key_from_text(unquote_classad_string(cert_text));
	if(pub == NULL) {
		dprintf(D_SECURITY, "Can't make public key from certificate text.\n");
		return false;
	}
	// TODO: associate the certificate with the proxy on hand.
	dprintf(D_SECURITY, "Here.\n");
	bool rv = verify_signature(pub, signed_text, signature);
	//EVP_PKEY_free(pub);
	if(!rv) {
		dprintf(D_SECURITY, "Bad signature on data.\n");
		return false;
	} else {
		dprintf(D_SECURITY, "Good signature.\n");
	}

// now check that the attributes in the list are present.
	return verify_same_subset_attributes(ad, sad, attributes_to_verify);
}

char *
get_signing_certfile(bool use_gsi, ClassAd &ad, ClassAd *cached_ad)
{
	char *ssl_cert_filename = NULL;
	dprintf(D_SECURITY, "Entering get_signing_certfile with use_gsi = %s\n", use_gsi ? "TRUE" : "FALSE");
	if(! use_gsi) {
		ssl_cert_filename = param( AUTH_SSL_CLIENT_CERTFILE_STR );
		if(ssl_cert_filename == NULL) {
			fprintf(stderr,
					"Specify the certificate file for signing using '%s' in the config files.\n",
					AUTH_SSL_CLIENT_CERTFILE_STR);
			return NULL;
		}
		return ssl_cert_filename;
	}
	// OK, must be GSI.
	static const char *ad_type;
	ad_type = ad.GetMyTypeName();
	if(strcmp(ad_type, "Machine")) {
		char *proxy_filename = NULL;
		ad.LookupString("x509userproxy", &proxy_filename);
		if(proxy_filename == NULL) {
			if(cached_ad) {
				cached_ad->LookupString("x509userproxyorig", &proxy_filename);
			}
		}
		if(proxy_filename == NULL) {
			fprintf(stderr,	"Can't get X509UserProxy from job ad.");
			return NULL;
		}
		return proxy_filename;
	} else {
		char *eec_key_filename = NULL;
		eec_key_filename = param( "GSI_DAEMON_CERT" );
		if(eec_key_filename == NULL) {
			fprintf(stderr,
					"Specify the key file for signing using '%s' in the config files.\n",
					"GSI_DAEMON_KEY");
			return NULL;
		}
		return eec_key_filename;
	}
	// unreachable
	// return NULL;

}

char *
get_signing_keyfile(bool use_gsi, ClassAd &ad, ClassAd *cached_ad)
{
	char *ssl_key_filename = NULL;
	if(! use_gsi) {
		ssl_key_filename = param( AUTH_SSL_CLIENT_KEYFILE_STR );
		if(ssl_key_filename == NULL) {
			fprintf(stderr,
					"Specify the key file for signing using '%s' in the config files.\n",
					AUTH_SSL_CLIENT_KEYFILE_STR);
			return NULL;
		}
		return ssl_key_filename;
	}
	// OK, must be GSI.
	// TODO: check if GSI is available, at a higher level.
	static const char *ad_type;
	ad_type = ad.GetMyTypeName();
	if(strcmp(ad_type, "Machine")) {
		char *proxy_filename = NULL;
		ad.LookupString("x509userproxy", &proxy_filename);
		if(proxy_filename == NULL) {
			if(cached_ad) {
				cached_ad->LookupString("x509userproxyorig", &proxy_filename);
				if(proxy_filename) {
					dprintf(D_SECURITY, "Got x509userproxyorig = '%s' from cached ad.\n", proxy_filename);
				}
			}
		} else {
			dprintf(D_SECURITY, "Got x509userproxy = '%s' from job.\n", proxy_filename);
		}
		if(proxy_filename == NULL) {
			fprintf(stderr,
					"Can't get X509UserProxy from job ad.");
			return NULL;
		}
		return proxy_filename;
	} else {
		char *eec_key_filename = NULL;
		eec_key_filename = param( "GSI_DAEMON_KEY" );
		if(eec_key_filename == NULL) {
			fprintf(stderr,
					"Specify the key file for signing using '%s' in the config files.\n",
					"GSI_DAEMON_KEY");
			return NULL;
		}
		return eec_key_filename;
	}
	// unreachable
	// return NULL;
}

#endif /* defined(HAVE_EXT_OPENSSL) || defined(HAVE_EXT_GLOBUS) */

bool
generic_sign_classad(ClassAd &ad, ClassAd *cached_ad, bool is_job_ad)
{
#if defined(HAVE_EXT_OPENSSL) || defined(HAVE_EXT_GLOBUS)

	char *sca_c = param( "SIGN_CLASSADS" );
	if(sca_c == NULL) {
		return true; // It's OK if the config file says not to sign.
	}

	MyString sca(sca_c);
	free(sca_c);

	if(!isTrue(sca.GetCStr())) {
		return true; // Config file says not to sign.
	}

	dprintf(D_SECURITY, "Signing ClassAd.\n");
	char *attr_c = NULL;
	if(is_job_ad) {
		attr_c = param("SIGN_JOB_CLASSAD_ATTRIBUTES");
	} else {
		attr_c = param("SIGN_MACHINE_CLASSAD_ATTRIBUTES");
	}
	if(attr_c == NULL) {
		fprintf(stderr, "Specify attributes to sign using "
				"SIGN_%s_CLASSAD_ATTRIBUTES.\n",
				is_job_ad ? "JOB" : "MACHINE");
		return false;
	}
	StringList include(attr_c);
	free(attr_c);

	bool use_gsi = false;
	char *sca_type = param("CLASSAD_SIGNATURE_CREDENTIAL_TYPE");
	if(sca_type == NULL) {
		fprintf(stderr,
				"Specify the credential type using '%s'.\n",
				"CLASSAD_SIGNATURE_CREDENTIAL_TYPE");
		return false;
	}
	if(!strcmp(sca_type, "GSI")) {
		use_gsi = true;
	} else {
		if(!strcmp(sca_type, "SSL")) {
			use_gsi = false;
		} else {
			fprintf(stderr,
					"Credential type of '%s' must be 'GSI' or 'SSL'.\n",
					"CLASSAD_SIGNATURE_CREDENTIAL_TYPE");
			return false;
		}
	}

	char *keyfile_c = get_signing_keyfile(use_gsi, ad, cached_ad);
	if(keyfile_c == NULL) {
		return false;
	}
	MyString keyfile(keyfile_c);
	free(keyfile_c);
	char *certfile_c = get_signing_certfile(use_gsi, ad, cached_ad);
	if(certfile_c == NULL) {
		return false;
	}
	MyString certfile(certfile_c);
	free(certfile_c);
	if(!sign_classad(ad, cached_ad, include, keyfile, certfile)) {
		fprintf( stderr, "Unable to sign ClassAd.\n");
		return false;
	}
	dprintf(D_SECURITY, "Success signing ClassAd.\n");
	return true;
#else // defined(HAVE_EXT_OPENSSL) || defined(HAVE_EXT_GLOBUS)
	dprintf(D_ALL, "Can't sign ClassAd: not supported on this platform.\n");
	return false;
#endif // defined(HAVE_EXT_OPENSSL) || defined(HAVE_EXT_GLOBUS)
}

bool
generic_verify_classad(ClassAd ad, bool is_job_ad)
{
#if defined(HAVE_EXT_OPENSSL) || defined(HAVE_EXT_GLOBUS)
	char *vsca_c = param( "VERIFY_SIGNED_CLASSADS" );
	if(vsca_c == NULL) {
		return true; // It's OK if the config file says not to sign.
	}

	MyString vsca(vsca_c);
	free(vsca_c);
	if(!isTrue(vsca.GetCStr())) {
		return true; // Config file says not to sign.
	}

	char *attr_c = NULL;
	if(is_job_ad) {
		attr_c = param("VERIFY_JOB_CLASSAD_ATTRIBUTES");
	} else {
		attr_c = param("VERIFY_MACHINE_CLASSAD_ATTRIBUTES");
	}
	if(attr_c == NULL) {
		fprintf(stderr, "Specify attributes to sign using "
				"VERIFY_%s_CLASSAD_ATTRIBUTES.\n",
				is_job_ad ? "JOB" : "MACHINE");
		return false;
	}
	StringList include(attr_c);
	free(attr_c);

	if(!verify_classad(ad, include)) {
		fprintf( stderr, "Unable to verify signed Classad.\n");
		return false;
	}
	dprintf(D_SECURITY, "Success verifying ClassAd.\n");
	return true;
#else // defined(HAVE_EXT_OPENSSL) || defined(HAVE_EXT_GLOBUS)
	dprintf(D_ALL, "Can't verify ClassAd: not supported on this platform.\n");
	return false;
#endif // defined(HAVE_EXT_OPENSSL) || defined(HAVE_EXT_GLOBUS)
}

bool
host_sign_key(char *& policy, EVP_PKEY *proxy_pubkey) {
	ClassAd policy_ad;
	BIO *mb;
	MyString classad_text;

	mb = BIO_new(BIO_s_mem());
	if(!mb) {
		dprintf(D_SECURITY, "Error getting memory buffer for signing proxy.\n");
		return false;
	}
	PEM_write_bio_PUBKEY(mb, proxy_pubkey);
	char *buffer = NULL;
	int buffer_len = 0;
	if(FALSE == bio_to_buffer(mb, &buffer, &buffer_len)) {
		dprintf(D_SECURITY, "Error getting buffer from bio.\n");
		return false;
	}
	buffer[buffer_len] = '\0';
	StringList to_sign;
	to_sign.insert("ProxyPublicKey");
	MyString quoted = quote_classad_string(buffer);
	policy_ad.Assign("ProxyPublicKey", quoted);
	to_sign.insert("Assertion");
	policy_ad.Assign("Assertion","Private key associated with ProxyPublicKey is present on signing host.");

	policy_ad.SetMyTypeName("Machine");
	MyString host_cert_file;
	host_cert_file = get_signing_certfile(true, policy_ad, NULL);
	MyString host_key_file;
	host_key_file = get_signing_keyfile(true, policy_ad, NULL);
/*
	MyString host_cert_text;
	if(!get_file_text(host_certificate_file, host_cert_text)) {
		dprintf(D_SECURITY, "Error getting host certificate text.\n");
		return false;
	}
	to_sign.insert("HostCertificate");
	policy_ad.Assign("HostCertificate", host_cert_text);*/

	if(!sign_classad(policy_ad, NULL, to_sign, host_key_file, host_cert_file)) {
		dprintf(D_SECURITY, "error signing service policy.\n");
		return false;
	}
	MyString buf1, buf2;
	policy_ad.LookupString(ATTR_CLASSAD_SIGNATURE_TEXT, buf1);
	policy_ad.LookupString(ATTR_CLASSAD_SIGNATURE, buf2);
	classad_text = buf1 + ";" + buf2;
	//policy_ad.sPrint(classad_text);
	policy = strdup(classad_text.Value());
	return true;
}

bool
verify_certificate(X509 *cert)
{
    int ret = false;
    X509_STORE *store;
    X509_STORE_CTX *ctx;

	if(!cert) {
		report_openssl_errors("verify_certificate");
		return false;
	}
    ctx = X509_STORE_CTX_new();
    store = X509_STORE_new();
    X509_STORE_set_default_paths(store);
    //X509_STORE_add_cert(store, pnca_cert);
	char *tmp = param("GSI_DAEMON_TRUSTED_CA_DIR");
	if(!tmp) {
		dprintf(D_SECURITY, "Can't get GSI_DAEMON_TRUSTED_CA_DIR to verify certificate.\n");
	} else {
		X509_STORE_load_locations(store, NULL, tmp);
		free(tmp);
		X509_STORE_CTX_init(ctx, store, cert, NULL);
		if (!X509_verify_cert(ctx)) {
			dprintf(D_SECURITY, "Error verifying signature on issued certificate:\n");
			// openssl helper use here.
			//ERR_print_errors_fp (stderr);
			//report_openssl_errors("verify_certificate");
			dprintf(D_ALWAYS, "Error: %s\n", X509_verify_cert_error_string( ctx->error ));
		} else {
			ret = true;
		}
	}
	X509_STORE_CTX_free(ctx);
	X509_STORE_free(store);

    return ret;
}

int
x509_self_delegation( const char *proxy_file,
					  char *new_proxy_file,
					  const char *tsp,
					  const char *policy_oid )
{
#if !defined(HAVE_EXT_GLOBUS)

	_globus_error_message = "This version of Condor doesn't support X509 credentials!" ;
	return -1;

#else
	int rc = 0;
	int error_line = 0;
	globus_result_t result = GLOBUS_SUCCESS;
	globus_gsi_cred_handle_t source_cred =  NULL;
	//globus_gsi_cred_handle_t host_cred = NULL;
	globus_gsi_proxy_handle_t new_proxy = NULL;
	BIO *bio = NULL;
	X509 *cert = NULL;
	STACK_OF(X509) *cert_chain = NULL;
	int idx = 0;
	globus_gsi_cert_utils_cert_type_t cert_type;
	globus_gsi_cred_handle_t proxy_handle =  NULL;
	globus_gsi_proxy_handle_t request_handle = NULL;
	char *policy = NULL;
	int policy_nid = 0;
	EVP_PKEY *req_pubkey;
	X509_REQ *req;
	char *buffer;
	int buffer_len;

/*	if ( activate_globus_gsi() != 0 ) {
		return -1;
	}
*/
	// Send side
	result = globus_gsi_cred_handle_init( &source_cred, NULL );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	result = globus_gsi_proxy_handle_init( &new_proxy, NULL );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	result = globus_gsi_cred_read_proxy( source_cred, proxy_file );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	// Receive side
	result = globus_gsi_proxy_handle_init( &request_handle, NULL );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	bio = BIO_new( BIO_s_mem() );
	if ( bio == NULL ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	result = globus_gsi_proxy_create_req( request_handle, bio );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	if( bio_to_buffer(bio, &buffer, &buffer_len) == FALSE ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	BIO_free(bio);
	bio = NULL;

	// New
	char *tm = (char *)malloc(buffer_len);
	if( !tm ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}
	int bl = buffer_len;
	memcpy(tm, buffer, buffer_len);
	bio = BIO_new( BIO_s_mem() );
	if( bio == NULL ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	if( buffer_to_bio(tm, bl, &bio) == FALSE ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	free(tm);
	tm = NULL;

	// X509_REQ *d2i_X509_REQ_bio(BIO *bp, X509_REQ **x);
	req = NULL;
	if(!(req = d2i_X509_REQ_bio(bio, NULL))) {
		rc = -1;
		error_line = __LINE__;
		//dprintf(D_SECURITY, "Error!\n");
		goto cleanup;
	}

	req_pubkey = X509_REQ_get_pubkey(req);
	//X509_free(req);

	BIO_free(bio);
	bio = NULL;

	// Send
	if( buffer_to_bio(buffer, buffer_len, &bio) == FALSE ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	free( buffer );
	buffer = NULL;

	result = globus_gsi_proxy_inquire_req( new_proxy, bio );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	if(tsp && !strcmp(policy_oid, TSPC_POLICY_OID)) {
		policy = strdup(tsp);
	} else {
		if(!strcmp(policy_oid, SSPC_POLICY_OID)) {

			dprintf(D_SECURITY,
					"**************************************SSPC path.\n");
			dprintf(D_SECURITY, "proxy file is '%s'\n", proxy_file);
			if(!host_sign_key(policy, req_pubkey)) {
				dprintf(D_ALWAYS, "Error, can't get host to sign policy.\n");
			} else {
				dprintf(D_SECURITY, "Got host to sign policy.\n");
			}
		} else {
			dprintf(D_SECURITY, "Unrecognized policy oid: '%s'\n", policy_oid);
		}
		// Free req_pubkey;
	}
	BIO_free( bio );
	bio = NULL;

	policy_nid = OBJ_txt2nid(policy_oid);
	dprintf(D_ALWAYS, "Policy ln: %s %d %d\n", OBJ_nid2ln(policy_nid), policy_nid, GLOBUS_GSI_CERT_UTILS_TYPE_RFC_RESTRICTED_PROXY);

		// modify certificate properties
		// set the appropriate proxy type
	result = globus_gsi_cred_get_cert_type( source_cred, &cert_type );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}
	switch ( cert_type ) {
	case GLOBUS_GSI_CERT_UTILS_TYPE_CA:
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	case GLOBUS_GSI_CERT_UTILS_TYPE_EEC:
	case GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_INDEPENDENT_PROXY:
	case GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_RESTRICTED_PROXY:
		cert_type = GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_IMPERSONATION_PROXY;
		break;
	case GLOBUS_GSI_CERT_UTILS_TYPE_RFC_INDEPENDENT_PROXY:
	case GLOBUS_GSI_CERT_UTILS_TYPE_RFC_RESTRICTED_PROXY:
		cert_type = GLOBUS_GSI_CERT_UTILS_TYPE_RFC_IMPERSONATION_PROXY;
		break;
	case GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_IMPERSONATION_PROXY:
	case GLOBUS_GSI_CERT_UTILS_TYPE_GSI_3_LIMITED_PROXY:
	case GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2_PROXY:
	case GLOBUS_GSI_CERT_UTILS_TYPE_GSI_2_LIMITED_PROXY:
	case GLOBUS_GSI_CERT_UTILS_TYPE_RFC_IMPERSONATION_PROXY:
	case GLOBUS_GSI_CERT_UTILS_TYPE_RFC_LIMITED_PROXY:
	default:
			// Use the same certificate type
		break;
	}

	if( policy != NULL ) {
		cert_type = GLOBUS_GSI_CERT_UTILS_TYPE_RFC_RESTRICTED_PROXY;
	}

	result = globus_gsi_proxy_handle_set_type( new_proxy, cert_type );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}
	fprintf(stderr, "Policy_nid: %d\n", policy_nid);
	fprintf(stderr, "U ext: '%s', '%s'\n", OBJ_nid2ln(655), OBJ_nid2sn(655));
	if( policy != NULL ) {
		result = globus_gsi_proxy_handle_set_policy( new_proxy,
													 (unsigned char *)policy,
													 strlen(policy),
													 policy_nid );
		if( result != GLOBUS_SUCCESS ) {
			rc = -1;
			error_line = __LINE__;
			goto cleanup;
		}
	}

	/* TODO Do we have to destroy and re-create bio, or can we reuse it? */
	bio = BIO_new( BIO_s_mem() );
	if ( bio == NULL ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	result = globus_gsi_proxy_sign_req( new_proxy, source_cred, bio );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

		// Now we need to stuff the certificate chain into in the bio.
		// This consists of the signed certificate and its whole chain.
	result = globus_gsi_cred_get_cert( source_cred, &cert );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}
	i2d_X509_bio( bio, cert );
	X509_free( cert );
	cert = NULL;

	result = globus_gsi_cred_get_cert_chain( source_cred, &cert_chain );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	for( idx = 0; idx < sk_X509_num( cert_chain ); idx++ ) {
		X509 *next_cert;
		next_cert = sk_X509_value( cert_chain, idx );
		i2d_X509_bio( bio, next_cert );
	}
	sk_X509_pop_free( cert_chain, X509_free );
	cert_chain = NULL;

	result = globus_gsi_proxy_assemble_cred( request_handle, &proxy_handle,
											 bio );
	if ( result != GLOBUS_SUCCESS ) {
		rc = -1;
		error_line = __LINE__;
		goto cleanup;
	}

	/* globus_gsi_cred_write_proxy() declares its second argument non-const,
	 * but never modifies it. The cast gets rid of compiler warnings.
	 */
	if( new_proxy_file ) {
		BIO *tmp_bio = BIO_new( BIO_s_mem() );
		result = globus_gsi_cred_write( proxy_handle, tmp_bio );
		if ( result != GLOBUS_SUCCESS ) {
			rc = -1;
			error_line = __LINE__;
			goto cleanup;
		}
		char *tmp_buf;
		int tmp_len;
		if( bio_to_buffer(tmp_bio, &tmp_buf, &tmp_len) == FALSE ) {
			rc = -1;
			error_line = __LINE__;
			goto cleanup;
		}
		int tmp_fd = mkstemp( new_proxy_file );
		if( tmp_fd == -1 ) {
			rc = -1;
			error_line = __LINE__;
			goto cleanup;
		}
		if( write(tmp_fd, tmp_buf, tmp_len) != tmp_len ) {
			rc = -1;
			error_line = __LINE__;
			goto cleanup;
		}
		if(tmp_bio) {
			BIO_free( tmp_bio );
		}
		close(tmp_fd);
		dprintf(D_SECURITY, "Delegation new file is: '%s'\n", new_proxy_file);
	} else {
		result = globus_gsi_cred_write_proxy( proxy_handle, (char *)proxy_file );
	}
	fprintf(stderr, "Got through redelegation.\n");
 cleanup:
	/* TODO Extract Globus error message if result isn't GLOBUS_SUCCESS */
	if ( error_line ) {
		char buff[1024];
		snprintf( buff, sizeof(buff), "x509_self_delegation failed "
				  "at line %d", error_line );
		dprintf(D_SECURITY, "Error: %s\n", buff );
	}

	if ( bio ) {
		BIO_free( bio );
	}
	if ( request_handle ) {
		globus_gsi_proxy_handle_destroy( request_handle );
	}
	if ( proxy_handle ) {
		globus_gsi_cred_handle_destroy( proxy_handle );
	}
	if ( new_proxy ) {
		globus_gsi_proxy_handle_destroy( new_proxy );
	}
	if ( source_cred ) {
		globus_gsi_cred_handle_destroy( source_cred );
	}
	if ( cert ) {
		X509_free( cert );
	}
	if ( cert_chain ) {
		sk_X509_pop_free( cert_chain, X509_free );
	}

	return rc;
#endif
}

