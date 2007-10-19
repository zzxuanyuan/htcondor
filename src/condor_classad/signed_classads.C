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
#include "openssl/evp.h"
#include "openssl/err.h"
#include "condor_debug.h"
#include "openssl/pem.h"
#include "condor_classad.h"
#include "condor_auth_ssl.h"
#include "condor_config.h"
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
 * mystring2charstar
 *
 * Utility function: creates a newly allocated char * for the value of the 
 * MyString; for type safety.  Caller frees!
 *
 * Returns malloced string the same size as the input or null.
 */
char *
mystring2charstar(const MyString &m) 
{
	char *buf;
	int buflen = m.Length();
	if(m.Length() == 0 || m.GetCStr() == NULL) {
		return NULL;
	}
	buflen++;
	if(!(buf = (char *)malloc(buflen))) {
		dprintf(D_SECURITY, "Can't malloc.\n");
        return NULL;
    }
    strncpy(buf, m.Value(), buflen);
	return buf;
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
		dprintf(D_SECURITY, "Can't sign zero length data!\n");
		return false;
	}
	unsigned char *sig_buf;
	unsigned int sig_len;
	MyString hex_sig_buf;

	if(!EVP_SignInit(&ctx, EVP_sha1())) {
        ERR_print_errors_fp(stderr); // TODO: how to get this to dprintf?
        return false;
    }
	dprintf(D_SECURITY, "Length: %d: '%s'\n", data_len, data_buf);
    if(!EVP_SignUpdate(&ctx, data_buf, data_len)) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    sig_len = EVP_PKEY_size((EVP_PKEY *)pkey);
    sig_buf = (unsigned char *)malloc(sig_len);
    if(!sig_buf) {
	    fprintf(stderr, "Got error from malloc.\n");
	    return false;
	}
    if(!EVP_SignFinal(&ctx, sig_buf, &sig_len, pkey)) {
        ERR_print_errors_fp(stderr);
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
        ERR_print_errors_fp(stderr); // TODO dprintf
        return NULL;
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
	EVP_PKEY *pkey;
	X509 *x509; // TODO: does this need to be freed?

    fd = safe_fopen_wrapper(filename.Value(), "r");
    if(!fd) {
        dprintf(D_SECURITY, "Can't open public key: '%s'.\n", 
				filename.Value());
        return NULL;
    }

    x509 = PEM_read_X509(fd, NULL, NULL, NULL);
	fclose(fd);
    if(!x509) {
        ERR_print_errors_fp(stderr); // TODO: dprintf
        return NULL;
    }

    pkey = X509_get_pubkey(x509);
	X509_free(x509); // TODO: valgrind to see if this is appropriate.
    if(!pkey) {
        ERR_print_errors_fp(stderr); // TODO: dprintf
        return NULL;
    }
	return pkey;
}

EVP_PKEY *
get_public_key_from_text(const MyString &text)
{
	BIO *mem;
	char *buf = mystring2charstar(text);
	if(buf == NULL) {
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
		dprintf(D_SECURITY, "Can't read cert.\n");
		BIO_free(mem);
		free(buf);
		return NULL;
	}
	EVP_PKEY *rv = X509_extract_key(s_cert);
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

	const char *const data_buf = data.Value(); // TODO: can I do this?
	int data_len = data.Length();
	//assert(data_buf[data_len] == '\0');
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
		ERR_print_errors_fp(stderr);
		dprintf(D_SECURITY, "Error in VerifyInit.\n");
		free(bin_sig);
		return false;
	}
	dprintf(D_SECURITY, "Length: %d: '%s'\n", data_len, data_buf);
	if(!EVP_VerifyUpdate(&ctx, data_buf, data_len)) {
		ERR_print_errors_fp(stderr);
		dprintf(D_SECURITY, "Error in VerifyUpdate.\n");
		free(bin_sig);
		return false;
	}
	if(!EVP_VerifyFinal(&ctx, bin_sig, bin_sig_len, pkey)) {
		ERR_print_errors_fp(stderr);
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
	char *buf = mystring2charstar(text);
	if(buf == NULL) {
		dprintf(D_SECURITY, "Couldn't get text of classad.\n");
		return false;
	}
	//dprintf(D_SECURITY, "BUF='%s'\n", buf);
	ad = ClassAd(buf, '\n');
	//MyString debug = "";
	//ad.sPrint(debug);
	//dprintf(D_SECURITY, "AD='%s'\n", debug.Value());
	free(buf);
	return true;
}

/*
EVP_PKEY *get_private_key(const MyString& filename) 
{
	EVP_PKEY *pkey;
	FILE *fd = safe_fopen_wrapper(filename.Value(), "r");
	if(!fd) {
		dprintf(D_SECURITY, "Couldn't open file '%s'.\n", 
				filename.Value());
		perror("fopen");
		return NULL;
	}
	pkey = PEM_read_PrivateKey(fd, NULL, NULL, NULL);
	if(!pkey) {
		ERR_print_errors_fp(stderr);
		return NULL;
	}
	fclose(fd);
	return pkey;
}
*/



void
limit_classad(ClassAd &in_ad, 
			  StringList &include,
			  ClassAd &out_ad)
{
	out_ad = in_ad; // TODO: this makes a copy, right?
	ExprTree *ad_expr;
	char *attr_name;
	if(include.isEmpty()) {
		dprintf(D_SECURITY, "WARNING: Empty list of attributes to sign.\n");
		return;
	}
	include.rewind();
	while( (attr_name = include.next()) ) {
		if(!out_ad.Lookup(attr_name)) {
			dprintf(D_SECURITY, "WARNING: attribute '%s' missing from input, "
					"not included in signature.\n", attr_name);
		}
	}
	in_ad.ResetExpr();
	while( (ad_expr = in_ad.NextExpr()) ) {
		attr_name = ((Variable *)ad_expr->LArg())->Name();
		if(!include.contains_anycase(attr_name)) {
			out_ad.Delete(attr_name);
		}
	}
	return;
}

void
print_ad(ClassAd ad) 
{
	MyString debug;
	ad.sPrint(debug);
	dprintf(D_SECURITY, "AD: '%s'\n",debug.Value());
}

bool 
verify_same_subset_attributes(const ClassAd &jobAd, 
							  const ClassAd &sigAd, StringList &subset)
{
	print_ad(jobAd);
	print_ad(sigAd);
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
			 StringList &attributes_to_sign,
			 const MyString &private_key_path,
			 const MyString &public_key_path) 
{
	ClassAd sign_subset;
	limit_classad(ad, attributes_to_sign, sign_subset);
	EVP_PKEY *priv;
	EVP_PKEY *pub;
	priv = get_private_key(private_key_path);
	if(priv == NULL) {
		dprintf(D_SECURITY, "Couldn't get private key for file '%s'.\n", private_key_path.Value());
		return false;
	}
	pub = get_public_key(public_key_path);
	if(pub == NULL) {
		EVP_PKEY_free(priv);
		dprintf(D_SECURITY, "Couldn't get public key for file '%s'.\n", public_key_path.Value());
		return false;
	}
	// add certificate text to classad
	MyString cert_text = "";
	if(!get_file_text(public_key_path, cert_text)) {
		dprintf(D_SECURITY, "Couldn't get public key text for file '%s'.\n", public_key_path.Value());
		EVP_PKEY_free(priv);
		EVP_PKEY_free(pub);
		return false;
	}
	MyString quoted_cert_text = quote_classad_string(cert_text);
	sign_subset.Assign("ClassAdSignatureCertificate", quoted_cert_text);
	// add version information for signature "1.0a"
	MyString version_info = "1.0a";
	sign_subset.Assign("ClassAdSignatureVersion", version_info);

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
	ad.Assign("ClassAdSignatureText", text_to_sign);
	ad.Assign("ClassAdSignature", signature_text);
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
//	MyString adtext;
//	ad.sPrint(adtext);
//	dprintf(D_SECURITY, "Got ad: %s\n", adtext.Value());
	if(!ad.LookupString("ClassAdSignatureText", signed_text)) {
		dprintf(D_SECURITY, "Can't find signed text in signed classad.\n");
		return false;
	}
//	dprintf(D_SECURITY, "Verify text: '%s'\n", signed_text.Value());
	if(!ad.LookupString("ClassAdSignature", signature)) {
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
	if(!sad.LookupString("ClassAdSignatureVersion", version_info)) {
		dprintf(D_SECURITY, "Can't get signature version from ad.\n");
		return false;
	}
	if(version_info != "1.0a") {
		dprintf(D_SECURITY, "Can't verify signature version '%s'.\n", version_info.Value());
		return false;
	}
	if(!sad.LookupString("ClassAdSignatureCertificate", cert_text)) {
		dprintf(D_SECURITY, "Can't get certificate text from ad.\n");
		return false;
	}
	EVP_PKEY *pub = get_public_key_from_text(unquote_classad_string(cert_text));
	if(pub == NULL) {
		dprintf(D_SECURITY, "Can't make public key from certificate text.\n");
		return false;
	}
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

bool
generic_sign_classad(ClassAd &ad)
{
	dprintf(D_SECURITY, "Here's where we sign the classad.\n");
	char *sca_c = param( "SIGN_CLASSADS" );
	if(sca_c == NULL) {
		return true; // It's OK if the config file says not to sign.
	}

	MyString sca(sca_c);
	free(sca_c);
	if(!(sca[0] == 'Y' || sca[0] == 'y')) { // Is this the right idiom?  TODO
		return true; // Config file says not to sign.
	}
	
	char *attr_c = param("SIGN_CLASSAD_ATTRIBUTES");
	if(attr_c == NULL) {
		fprintf(stderr, "Specify attributes to sign using "
				"SIGN_CLASSAD_ATTRIBUTES.\n");
		return false;
	}
	StringList include(attr_c);
	free(attr_c);
	char *keyfile_c = param( AUTH_SSL_CLIENT_KEYFILE_STR );
	if(keyfile_c == NULL) {
		fprintf(stderr, 
				"Specify the key file for signing using '%s'.\n", 
				AUTH_SSL_CLIENT_KEYFILE_STR);
		return false;
	}
	MyString keyfile(keyfile_c);
	free(keyfile_c);
	char *certfile_c = param( AUTH_SSL_CLIENT_CERTFILE_STR );
	if(certfile_c == NULL) {
		fprintf(stderr, 
				"Specify the certificate file for signed ClassAds "
				"using '%s'.\n", AUTH_SSL_CLIENT_CERTFILE_STR );
		return false;
	}
	MyString certfile(certfile_c);
	free(certfile_c);
	if(!sign_classad(ad, include, keyfile, certfile)) {
		fprintf( stderr, "Unable to sign ClassAd.\n");
		return false;
	}
	/*
	  if(!verify_classad(ad, include)) {
	  fprintf( stderr, "Unable to verify signed Classad.\n");
	  return false;
	  }
	*/
	return true;
}

bool
generic_verify_classad(ClassAd ad)
{
	dprintf(D_SECURITY, "Here's where we verify the classad.\n");
	char *vsca_c = param( "VERIFY_SIGNED_CLASSADS" );
	if(vsca_c == NULL) {
		return true; // It's OK if the config file says not to sign.
	}

	MyString vsca(vsca_c);
	free(vsca_c);
	if(!(vsca[0] == 'Y' || vsca[0] == 'y')) { // Is this the right idiom?  TODO
		return true; // Config file says not to sign.
	}
	
	char *attr_c = param("VERIFY_CLASSAD_ATTRIBUTES");
	if(attr_c == NULL) {
		fprintf(stderr, "Specify attributes to sign using "
				"VERIFY_CLASSAD_ATTRIBUTES.\n");
		return false;
	}
	StringList include(attr_c);
	free(attr_c);

	if(!verify_classad(ad, include)) {
		fprintf( stderr, "Unable to verify signed Classad.\n");
		return false;
	}

	return true;
}
