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

#include "../condor_c++_util/soap_helpers.cpp"

#define SCA_XML_FORMAT SOAP_XML_CANONICAL
// #define SCA_XML_FORMAT SOAP_XML_CANONICAL|SOAP_XML_INDENT

template class List<ClassAd>;
template class Item<ClassAd>;

/* Given ascii hex, return actual binary value. */
char 
get_bits(char in) 
{
	if(in >= '0' && in <= '9') {
		return in-'0';
	}
	if(in >= 'a' && in <= 'f') {
		return in-'a'+10;
	}
	dprintf(D_SECURITY, "Illegal character in signature: %d.\n", in-'0');
	exit(1);
}

/* Given a binary input (sequence of bytes), convert
 * it to ascii hex.  Doubles the size of the input.
 * Returns 1 indicating success.
 */	
int
bin_2_hex(char *input, int input_len, char **output) 
{
	char *hex = (char *)malloc(2*input_len+2);
	if(!hex) {
		perror("malloc");
		dprintf(D_SECURITY, "bin_2_hex: Couldn't malloc.\n");
		return 0;
	}
	static const char ct[] = "0123456789abcdef";
	int i = 0;
	for(i = 0; i < input_len; i++) {
		int h1 = 0x0f&(input[i]>>4);
		int h2 = 0x0f&input[i];
		hex[2*i] = ct[h1];
		hex[2*i+1] = ct[h2];
	}
	hex[2*i] = '\0';
	*output = hex;
	return 1;
}

/* Opposite of quote_classad_string: convert the input
 * a classad value to output same as gsoap serialization.
 */
char *
unquote_classad_string(char *input) 
{
	int len = strlen(input);
	int i, ctr = 0;
	char *output;

	if( len < 1 ) {
		dprintf(D_SECURITY, "Can't unquote short input.\n");
		return 0;
	}
	output = (char *)malloc(len+1);
	if(output == NULL) {
		dprintf(D_SECURITY, "unquote malloc error.\n");
		return 0;
	}
	memset(output, 0, len);
	for(i = 0; i < len; i++) {
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

/* Convert the input (the output of gsoap serialization) 
 * to a format appropriate for using as a classad value.
 */
char *
quote_classad_string(const char *input) 
{
	int len = strlen(input)+1;
	int i, ctr = 0;
	char *output;

	if(len < 1) {
		dprintf(D_SECURITY, "Can't quote short input.\n");
		return 0;
	}
	output = (char *)malloc(len*2);
	if(output == NULL) {
		dprintf(D_SECURITY, "quote malloc error.\n");
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
			break;
		default:
			output[ctr] = input[i];
			break;
		}
		++ctr;
	}
	return output;
}

/*
 * get_raw_sig: given an adStruct, return the raw signature.
 * returns 1 if ok, 0 on error.
 */
int 
get_raw_sig(soap *soap, EVP_PKEY * key, 
			struct condor__ClassAdStruct *adStruct, 
			int *siglen_out, char **sig_out, char *tmp_file)
{
	int tmp_fd, smd_size;
	char *sig;

	tmp_fd = mkstemp(tmp_file);
	if(tmp_fd == -1) {
		perror("mkstemp");
		dprintf(D_SECURITY, 
				"Couldn't create temp file: '%s'\n", tmp_file);
		goto return_error_end;
	}		
	//dprintf(D_SECURITY, "Serializing to file %s\n", tmp_file);

	soap->sendfd = tmp_fd;
	
	smd_size = soap_smd_size(SOAP_SMD_SIGN_RSA_SHA1, key);
	//dprintf(D_SECURITY, "Allocating buffer size %d for smd.\n", smd_size);
	sig = (char *)
		SOAP_MALLOC(soap, smd_size);
	if(!sig) {
		perror("malloc");
		dprintf(D_SECURITY, "raw signature malloc error.\n");
		goto return_error_close;
	}
	
	int siglen;
	soap_serialize_PointerTocondor__ClassAdStruct(soap, &adStruct);
	if(soap_begin_send(soap)
	   || soap_put_PointerTocondor__ClassAdStruct(soap, &adStruct, 
												  "condor:ClassAd", 
												  "condor:ClassAdStruct")
	   || soap_end_send(soap)
	   || soap_smd_begin(soap, SOAP_SMD_SIGN_RSA_SHA1, key, 0)
	   || soap_put_PointerTocondor__ClassAdStruct(soap, &adStruct, 
												  "condor:ClassAd", 
												  "condor:ClassAdStruct")
	   || soap_smd_end(soap, sig, &siglen)) {
		soap_print_fault(soap, stderr);
		dprintf(D_SECURITY, "error performing signature.\n");
		goto return_error_free;
	} else {
		/*dprintf(D_SECURITY, "Signature computed: "
		  "Signature length %d\n", siglen);*/
		*siglen_out = siglen;
		*sig_out = sig;
		return 1;
	}
 return_error_free:
	free(sig);
 return_error_close:
	close(tmp_fd);
	unlink(tmp_file);
 return_error_end:
	// TODO: free soap object, etc.
	return 0;
}

/* 
 *
 */
bool
substring_of_file(char *str, const char *filename)
{
	if(!filename) {
		dprintf(D_SECURITY, "Null filename.\n");
		return false;
	}
	FILE *fp = safe_fopen_wrapper(filename, "r");
	
	if(!fp) {
		dprintf(D_SECURITY, "Can't open file '%s'\n", filename);
		return false;
	}
	StringList certs;
	MyString one_cert = "";
	char *line = NULL;
	bool in_key = false;
	while((line = getline(fp))) {
		if(in_key) {
			if(!strcmp(line, "-----END RSA PRIVATE KEY-----")) { // key start
				in_key = false;
				one_cert = "";
			}
			// see if we're done with key.
		} else {
			if(!strcmp(line, "-----BEGIN RSA PRIVATE KEY-----")) {
				in_key = true;
			} else {
				one_cert += line;
				one_cert += "\n";
				if(!strcmp(line, "-----END CERTIFICATE-----")) {
					certs.append(one_cert.GetCStr());
					one_cert = "";
				}
				// Do we need to free line?
			}
		}
	}
	certs.rewind();
	int num = certs.number();
	dprintf(D_SECURITY, "Got %d certs in proxy file.\n", num);
	int i = 0;
	char *cert = NULL;
	for (i = 0 ; i < num-1; i++) {
		cert = certs.next();
	}
	if(!strcmp(cert, str)) {
		return true;
	}
	return false;
}

/* Get the key from the classad.  Returns NULL on error.
 * Finds the value in the attribute "ClassAdSignatureCert."
 */
char *
get_proxy_cert_text(ClassAd *ad) 
{
	char *tmp_cert = NULL;
	ad->LookupString("ClassAdSignatureCert",&tmp_cert); // free tmp_cert
	if(tmp_cert == NULL) {
		dprintf(D_SECURITY, "Can't get signing cert from ad.\n");
		return NULL;
	}
	MyString cert = tmp_cert;
	free(tmp_cert); // freed tmp_cert
	while(cert.replaceString("\\n","\n")) { ; }
	//dprintf(D_SECURITY, "Got cert: '%s'\n", cert.GetCStr());

	char *tmp = (char *)malloc(cert.Length()+1);
	if(!tmp) {
		perror("malloc");
		dprintf(D_SECURITY, "Malloc error.\n");
		return NULL;
	}
	sprintf(tmp, "%s", cert.GetCStr());
	return tmp;
}
EVP_PKEY *
get_proxy_cert(char *text) 
{
	BIO *mem;
	mem = BIO_new_mem_buf(text, -1);
	if(!mem) {
		dprintf(D_SECURITY, "Error getting memory buffer for cert pub key.\n");
		return NULL;
	}
	X509 *s_cert = PEM_read_bio_X509(mem, NULL, NULL, NULL);
	if(!s_cert) {
		dprintf(D_SECURITY, "Can't read cert.\n");
		BIO_free(mem);
		return NULL;
	}
	EVP_PKEY *rv = X509_extract_key(s_cert);
	BIO_free(mem);		
	X509_free(s_cert);
	return rv;
}

/* Find the proxy cert (in the filesystem) and add it to
 * the classad, in the ClassAdSignatureCert attribute.
 */
int
insert_proxy_cert_into_ad(ClassAd *ad) 
{
	char *proxy_filename = NULL;
	ad->LookupString("x509userproxy", &proxy_filename); // free me
	if(proxy_filename == NULL) {
		dprintf(D_SECURITY, "Can't get x509userproxy from ClassAd.\n");
		return 0;
	}
	FILE *fp = safe_fopen_wrapper(proxy_filename, "r"); // close me
	if(!fp) {
		dprintf(D_SECURITY, 
				"Can't open the x509 user proxy file '%s'.\n",
				proxy_filename);
		free(proxy_filename);
		return 0;
	}
	free(proxy_filename);
	MyString cert = "";

	char *line = NULL;
	while((line = getline(fp))) {
		cert += line;
		cert += "\n";
		if(!strcmp(line, "-----END CERTIFICATE-----")) {
			break;
		}
	}
	fclose(fp);
	while(cert.replaceString("\n", "\\n")) { ; }
	InsertIntoAd(ad,"ClassAdSignatureCert",cert.GetCStr());
	return 1;
}

/* Get the proxy certificate private key from the filesystem.
 * Returns null on error.
 */
EVP_PKEY *
get_proxy_private_key(ClassAd *ad)
{
	char *proxy_filename = NULL;
	ad->LookupString("x509userproxy", &proxy_filename);
	if(proxy_filename == NULL) {
		dprintf(D_SECURITY, "Can't get x509userproxy from ClassAd.\n");
		return NULL;
	}
	FILE *fd = safe_fopen_wrapper(proxy_filename, "r");
	if(!fd) {
		dprintf(D_SECURITY, 
				"Can't open the x509 user proxy file '%s'.\n",
				proxy_filename);
		free(proxy_filename);
		return NULL;
	}
	free(proxy_filename);
	EVP_PKEY *rv = PEM_read_PrivateKey(fd, NULL, NULL, NULL);
	fclose(fd);
	return rv;
}

/* Get the text of the classad signature.  Includes both the
 * bitstring and the text of what's signed.
 * Returns "" on error.
 */
MyString
get_classad_sig_smd(ClassAd *ad)
{
	char *quoted_rv;
	MyString quoted_mrv;
	struct soap *soap = soap_new1(SCA_XML_FORMAT);
	if(!soap) {
		dprintf(D_SECURITY, "Error creating soap object.\n");
		return "";
	}

	struct condor__ClassAdStruct *adStruct = (struct condor__ClassAdStruct *)
		soap_malloc(soap, sizeof(struct condor__ClassAdStruct));

	if(!adStruct) {
		perror("malloc");
		soap_end(soap);
		soap_destroy(soap);
		free(soap);
		dprintf(D_SECURITY, "Couldn't malloc for signed ClassAd structure.\n");
		return "";
	}
	convert_ad_to_adStruct(soap, ad, adStruct, true);

	// get private key.
	EVP_PKEY *key = get_proxy_private_key(ad);
	if(!key) {
		dprintf(D_SECURITY, "Couldn't get x509 user proxy.\n");
		soap_end(soap);
		soap_destroy(soap);
		free(soap);
		return "";
	}

	int siglen;
	char *sig;
	char tmp_file[] = "/tmp/scaXXXXXX";

	//dprintf(D_SECURITY, "getting raw signature.\n");
	if(!get_raw_sig(soap, key, adStruct, &siglen, &sig, tmp_file)) {
		dprintf(D_SECURITY, "error getting signature.\n");
		soap_end(soap);
		soap_destroy(soap);
		free(soap);
		goto error_free;
	} else {
		soap_end(soap);
		soap_destroy(soap);
		free(soap);
		//dprintf(D_SECURITY, "Got raw signature.\n");
		char *hex;
		if(!bin_2_hex(sig, siglen, &hex)) {
			dprintf(D_SECURITY, "Couldn't convert binary signature to hex.\n");
			free(hex);
			goto error_free;
		}
		free(sig);
		MyString rv = MyString(hex);
		//dprintf(D_SECURITY, "Sig: %s\n",rv.GetCStr());
		free(hex);
		rv +=":";

		int tmp_fd = safe_open_wrapper(tmp_file, O_RDONLY);
		if(tmp_fd == -1) {
			perror("open");
			dprintf(D_SECURITY, "Couldn't open temp file '%s'.\n", tmp_file);
			goto error_free;
		}
		char *line = (char *)malloc(1025);
		if(line == NULL) {
			perror("malloc");
			dprintf(D_SECURITY, "Couldn't malloc for line.\n");
			close(tmp_fd);
			goto error_free;
		}
		int done = 0;
		int mrv = 0;
		while(!done) {
			mrv = read(tmp_fd, line, 1024);
			switch(mrv) {
			case -1:
				perror("read");
				dprintf(D_SECURITY, "Error reading from serialized "
						"signed ClassAd.\n");
				done = 1;
				break;
			case 0:
				done = 1;
				break;
			default:
				line[mrv] = '\0';
				rv += line;
			}
		}
		free(line);
		close(tmp_fd);
		free(key);
		quoted_rv = quote_classad_string(rv.GetCStr());
		quoted_mrv = MyString(quoted_rv);
		free(quoted_rv);
		return quoted_mrv;
	}
 error_free:
	free(key);
	// free(*adStruct); TODO
	return "";
}

/* 
 * Given a ClassAd, extract the value of the ClassAdSignature attribute. 
 */
MyString 
get_classad_sig(ClassAd *ad) 
{
	//dprintf(D_SECURITY, "Signing.\n");
	MyString rv = get_classad_sig_smd(ad);
	//dprintf(D_SECURITY, "Signed.\n");
	return rv;
}

/* 
 * Return a new ClassAd which contains only the attributes listed in the 
 * include list. 
 */
ClassAd *
limit_classad(ClassAd *ad, StringList *include) 
{
	ClassAd *rv = new ClassAd(*ad);
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
			rv->Delete(attr_name);
		}
	}
	// Suggestion: check the include list for attributes not found in the ad?
	return rv;
}

/*
 * Public API: given a ClassAd and list of attributes to sign,
 * produce a signature and add it to the ClassAd.
 */
bool
sign_classad(ClassAd *ad, StringList *include)
{
	MyString signature;

	dprintf(D_SECURITY, "Inserting proxy certificate into job "
			"before signing.\n");
	if(!insert_proxy_cert_into_ad(ad)) {
		dprintf(D_SECURITY, "Error inserting proxy cert into ad.\n");
		return false;
	}
	// Check and return error code: handle how?
	// How to expose GSI credential to this function?
	dprintf(D_SECURITY, "Signing classad.\n");
	ClassAd *new_ad = limit_classad(ad, include);
	if(!new_ad) {
		dprintf(D_SECURITY, "Can't get limited class ad.\n");
		return false;
	}
	signature = get_classad_sig(new_ad);
	delete(new_ad);

	if(!InsertIntoAd(ad, "ClassAdSignature", signature.GetCStr())) {
		dprintf(D_SECURITY, "Error inserting signature into ad.\n");
		return false;
	}
	return true;
}

/*
 * Given a signed ClassAd, extract the signature and the signed data
 * (which will be raw xml format).
 */
int 
get_sig_and_data_from_ad(ClassAd *ad, char **signature, int *sig_len, 
						MyString *rdata)
{
	// cas = ClassAd signature
	char *unquoted_cas;
	char *cas = NULL;

	*signature = NULL;
	*sig_len = 0;
	
	ad->LookupString("ClassAdSignature", &cas); // free me
	if(cas == NULL) {
		dprintf(D_SECURITY, "Couldn't get classAd Signature.\n");
		return 0;
	}
	unquoted_cas = unquote_classad_string(cas); //free me
	free(cas);
	cas = NULL;
	if(unquoted_cas == NULL) {
		dprintf(D_SECURITY, "Couldn't unquote raw signature.\n");
		return 0;
	}

	int u_cas_len = strlen(unquoted_cas);

	// separate the signature and the data.
	char *ascii_sig = unquoted_cas;
	char *data = NULL;
	int ascii_sig_len = 0;
	int i;
	for(i = 0; i < u_cas_len; i++) {
		if(unquoted_cas[i] == ':') {
			unquoted_cas[i] = '\0';
			ascii_sig_len = i;
			data = &unquoted_cas[i+1];
			break;
		}
	}
	if(i >= u_cas_len || data == NULL || ascii_sig_len == 0) {
		dprintf(D_SECURITY, "Couldn't find ':' separator in ClassAdSignature.\n");
		free(unquoted_cas);
		return 0;
	}
	// Turn the signature into binary.
	int binary_sig_len = ascii_sig_len/2;
	char *binary_sig = (char *)malloc(binary_sig_len); // caller frees me
	if(binary_sig == NULL) {
		perror("malloc");
		dprintf(D_SECURITY, "can't malloc.\n");
		free(unquoted_cas);
		return 0;
	}
	for(i = 0; i < binary_sig_len; i++) {
		char a = get_bits(ascii_sig[i*2]);
		char b = get_bits(ascii_sig[i*2+1]);
		//dprintf(D_SECURITY, "bits: %d %d %d\n", i, a, b);
		binary_sig[i] = (a<<4)|b;
		//dprintf(D_SECURITY, "Writing %d\n", binary_sig[i]);
	}			  
	*signature = binary_sig;
	*sig_len = binary_sig_len;
	rdata->sprintf("%s", data);
	free(unquoted_cas);
	return 1;
}

/*
 * Given the raw xml from the signature, convert it to a ClassAd.
 */
int
get_ad_from_sig_data(MyString rdata, ClassAd *ret_ad) 
{
	int rv = 0;
	char tmp_file[] = "/tmp/scaXXXXXX";
	int tmp_fd, data_len;

	// Deserialize the xml into an adStruct.
	//dprintf(D_SECURITY, "Deserializing xml.\n");
	//dprintf(D_SECURITY, "XML: '%s'\n", rdata.GetCStr());
	struct soap *soap = soap_new1(SCA_XML_FORMAT);
	struct condor__ClassAdStruct *adStruct = (struct condor__ClassAdStruct *)
		soap_malloc(soap, sizeof(struct condor__ClassAdStruct));

	if(!adStruct) {
		perror("malloc");
		dprintf(D_SECURITY, "Couldn't malloc for signed ClassAd structure.\n");
		goto error_soap_done;
	}
	tmp_fd = mkstemp(tmp_file);
	if(tmp_fd == -1) {
		perror("mkstemp");
		dprintf(D_SECURITY, "Couldn't create temp file.\n");
		goto error_free_adstruct;
	}

	data_len = rdata.Length();
	if(write(tmp_fd, rdata.GetCStr(), data_len) != data_len) {
		dprintf(D_SECURITY, "Write error.\n");
		goto error_close_fd;
	}
	close(tmp_fd);
	tmp_fd = safe_open_wrapper(tmp_file, O_RDONLY);
	if(tmp_fd == -1) {
		perror("open");
		dprintf(D_SECURITY, "Couldn't read temp file.\n");
		goto error_unlink_tmp;
	}

	soap_begin(soap);
	soap->recvfd = tmp_fd;
	soap_begin_recv(soap);
	if(!soap_get_PointerTocondor__ClassAdStruct(soap, &adStruct,
												"condor:ClassAd",
												"condor:ClassAdStruct")) {
		soap_print_fault(soap, stderr);
		dprintf(D_SECURITY, "Error deserializing.");
		goto error_soap_end_recv;
	}

	convert_adStruct_to_ad(soap, ret_ad, adStruct);

	rv = 1;
 error_soap_end_recv:
	soap_end_recv(soap);
 error_close_fd:
	close(tmp_fd);
 error_unlink_tmp:
	unlink(tmp_file);
 error_free_adstruct:
	soap_destroy(soap);
	soap_end(soap);
 error_soap_done:
	soap_done(soap);
	free(soap);
	return rv;
}

/*
 * Verify that the attributes listed in subset are present and have the
 * same values in both the jobad and the sigad.
 */
bool
verify_same_subset_attributes(ClassAd *jobAd, ClassAd *sigAd, StringList *subset) 
{
	ExprTree *jobAdExpr, *sigAdExpr;
	char *attr_name;
	subset->rewind();
	while( (attr_name = subset->next()) ) {
		jobAdExpr = jobAd->Lookup(attr_name);
		sigAdExpr = sigAd->Lookup(attr_name);
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

/*
 * SMD implementation: verify signature on the classad, for the listed attributes.
 */
bool
verify_signed_classad_smd(ClassAd *jobad, StringList *include)
{
	MyString rdata;
	char *signature;
	int sig_len;
	int rv = 0;
	//dprintf(D_SECURITY, "Getting sig and data from ad.\n");
	if(!get_sig_and_data_from_ad(jobad, 
							&signature, // free signature
							&sig_len, 
							&rdata)) { 
		dprintf(D_SECURITY, "Error obtaining signature and data from ClassAd.\n");
		return 0;
	}
	//dprintf(D_SECURITY, "getting ad from sig data.\n");
	ClassAd *sigAd = new ClassAd();
	if(!get_ad_from_sig_data(rdata, sigAd)) {
		dprintf(D_SECURITY, "Couldn't extract the signed ad "
				"from the signature.\n");
		free(signature);
		return 0;
	}
	if(!verify_same_subset_attributes(jobad, sigAd, include)) {
		dprintf(D_SECURITY, "Signed attributes don't match job attributes.\n");
		free(signature);
		delete(sigAd);
		return 0;
	}

    // If this doesn't actually contain the right cert, maybe
    // the environment will contain a pointer.
	char *proxy_fullname;
	jobad->LookupString("x509userproxy", proxy_fullname);
	if(proxy_fullname == NULL) {
		dprintf(D_SECURITY, "Can't get x509userproxy from ClassAd.\n");
		free(signature);
		delete(sigAd);
		return 0;
	}
	const char *proxy_filename = condor_basename(proxy_fullname);
	char *proxy_text = get_proxy_cert_text(sigAd);
	if(!proxy_text) {
		dprintf(D_SECURITY, "Can't get the certificate from the signature.\n");
		free(signature);
		free(proxy_fullname);
		delete(sigAd);
		return 0;
	}
	if(!substring_of_file(proxy_text, proxy_filename)) {
		dprintf(D_SECURITY, "Can't find signing certificate in current "
				"proxy credential.\n");
		//free(proxy_filename);
		free(proxy_text);
		free(proxy_fullname);
		free(signature);
		delete(sigAd);
		return 0;
	}
	free(proxy_fullname);

	EVP_PKEY *key = get_proxy_cert(proxy_text);
	if(!key) {
		dprintf(D_SECURITY, "Couldn't get proxy certificate from ClassAd.\n");
		free(proxy_text);
		free(signature);
		delete(sigAd);
		return 0;
	}
	free(proxy_text);
	//dprintf(D_SECURITY, "got key.\n");
	struct soap *soap = soap_new1(SCA_XML_FORMAT);
	struct condor__ClassAdStruct *adStruct = (struct condor__ClassAdStruct *)
		soap_malloc(soap, sizeof(struct condor__ClassAdStruct));

	if(!adStruct) {
		perror("malloc");
		free(signature);
		delete(sigAd);
		dprintf(D_SECURITY, "Couldn't malloc for signed ClassAd structure.\n");
		return 0;
	}
	//dprintf(D_SECURITY, "Starting verify sig of len %d.\n", sig_len);
	convert_ad_to_adStruct(soap, sigAd, adStruct, true);
	delete(sigAd);

	int tmp_fd;
	char tmp_file[] = "/tmp/scaXXXXXX";
	tmp_fd = mkstemp(tmp_file);
	if(tmp_fd == -1) {
		perror("mkstemp");
		dprintf(D_SECURITY, 
				"Couldn't create temp file: '%s'\n", tmp_file);
		free(signature);
		delete(sigAd);
		return 0;
	}		
	//dprintf(D_SECURITY, "Verify: Serializing to file %s\n", tmp_file);

	soap->sendfd = tmp_fd;

	soap_serialize_PointerTocondor__ClassAdStruct(soap, &adStruct);
	if(/*soap_begin_send(soap)
	   || soap_put_PointerTocondor__ClassAdStruct(soap, &adStruct, 
												  "condor:ClassAd", 
												  "condor:ClassAdStruct")
	   || soap_end_send(soap)
	   ||*/ 
	   soap_smd_begin(soap, SOAP_SMD_VRFY_RSA_SHA1, key, 0)
	   || soap_put_PointerTocondor__ClassAdStruct(soap, &adStruct,
												  "condor:ClassAd",
												  "condor:ClassAdStruct")
	   || soap_smd_end(soap, signature, &sig_len)) {
		soap_print_fault(soap, stderr);
	} else {
		rv = 1;
	}
	close(tmp_fd);
	unlink(tmp_file);
	free(signature);
	if(key)
		free(key);
	soap_end(soap);
	soap_destroy(soap);
	free(soap);
	return rv;
}

/* Figure out what files are going to get transfered, add them to the 
 * new attribute ClassAdSignatureFiles, and then add a checksum
 * for each to ClassAdSignatureHashes.
 */
bool
add_file_hashes(ClassAd *jobad, StringList *include)
{
	char *cmd = NULL;
	jobad->LookupString("Cmd", &cmd);  // free me
	if(!cmd) {
		dprintf(D_SECURITY, "Can't find cmd attribute for hash signing.\n");
		return false;
	}
	char *cmd_hash = condor_hash_file(cmd); // free me
	InsertIntoAd(jobad,"CmdHash",cmd_hash);
	include->append("CmdHash");
	// need to repeat this for every file in transferfiles
	free(cmd);
	free(cmd_hash);
	

}

/*
 * Public API: verify signature for the listed attributes.
 */
bool
verify_signed_classad(ClassAd *jobad, StringList *include) 
{
	if(!add_file_hashes(jobad, include)) {
		dprintf(D_SECURITY, "Error signing file hashes.\n");
		return false;
	}
	return verify_signed_classad_smd(jobad, include);
}
