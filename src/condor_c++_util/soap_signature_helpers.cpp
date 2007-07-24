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

char 
get_bits(char in) {
	if(in >= '0' && in <= '9') {
		return in-'0';
	}
	if(in >= 'a' && in <= 'f') {
		return in-'a'+10;
	}
	fprintf(stderr, "Illegal character in signature: %d.\n", in-'0');
	exit(1);
}

int
bin2hex(char *input, int input_len, char **output) 
{
	char *hex = (char *)malloc(2*input_len+2);
	if(!hex) {
		perror("malloc");
		fprintf(stderr, "bin2hex: Couldn't malloc.\n");
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

char *unquote_classad_string(char *input) {
	int len = strlen(input);
	int i, ctr = 0;
	char *output;

	if( len < 1 ) {
		dprintf(D_SECURITY, "Error in unquote_classad_string: short input.\n");
		return 0;
	}
	output = (char *)malloc(len+1);
	if(output == NULL) {
		dprintf(D_SECURITY, "Malloc error.\n");
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

char *quote_classad_string(const char *input) {
	int len = strlen(input)+1;
	int i, ctr = 0;
	char *output;

	// I'm not sure if this is the right debug level for fatal errors.
	if(len < 1) {
		fprintf(stderr, "Error in quote_classad_string: short input.\n");
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
//			dprintf(D_SECURITY, "quoted \\n\n");
			break;
		case '\"':
			output[ctr++] = '\\';
			output[ctr] = '\"';
//			dprintf(D_SECURITY, "quoted \\\"\n");
			break;
		case '\\':
			output[ctr++] = '\\';
			output[ctr] = '\\';
//			dprintf(D_SECURITY, "quoted \\\\\n");
			break;
		default:
			output[ctr] = input[i];
			break;
		}
		++ctr;
	}
	//dprintf(D_SECURITY, "Quote: was %d, now %d\n", len-1, strlen(output));
	return output;
}

/* int verify_unquotedWSSE(char *unquoted_sig) 
{
	struct soap *soap;
	//soap = soap_new();
	char tmp_file[] = "/tmp/.condor_scaXXXXXX";
	int tmp_fd;
	//struct condor__ClassAdStruct *adStruct;
	struct __condor__signature sig;

	int rv = 0;

	soap = soap_new1(SCA_XML_FORMAT);
    soap_register_plugin(soap, soap_wsse);
	soap_wsse_verify_auto(soap, SOAP_SMD_HMAC_SHA1, key, sizeof(key));
	
	tmp_fd = mkstemp(tmp_file);
	if(tmp_fd == -1) {
		perror("mkstemp");
		dprintf(D_SECURITY, "Couldn't create temp file in verifySignedClassAd.\n");
		goto return_error;
	}

	fprintf(stderr, "Signature length: %d\n", strlen(unquoted_sig));
	//fprintf(stderr, "Sig: '%s'\n", unquoted_sig);
	write(tmp_fd, unquoted_sig, strlen(unquoted_sig)+1);
	close(tmp_fd);
	tmp_fd = safe_open_wrapper(tmp_file, O_RDONLY);
	if(tmp_fd == -1) {
		perror("open");
		dprintf(D_SECURITY, "Couldn't read temp file in verifySignedClassAd.\n");
		goto return_error;
	}
	soap->recvfd = tmp_fd;
	if(soap_recv___condor__signature(soap, &sig)) {
		fprintf(stderr, "The signature didn't verify.\n");
		goto return_error;
	} else {
		fprintf(stderr, "The signature verified OK.\n");
		rv = 1;
	}
 return_error:
    soap_end(soap);
    soap_done(soap);
    free(soap);
    return rv;

}  */

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
		dprintf(D_SECURITY, "get_raw_sig: "
				"Couldn't create temp file: '%s'\n", tmp_file);
		goto return_error_end;
	}		
	//fprintf(stderr, "Serializing to file %s\n", tmp_file);

	soap->sendfd = tmp_fd;
	
	smd_size = soap_smd_size(SOAP_SMD_SIGN_RSA_SHA1, key);
	//dprintf(D_SECURITY, "Allocating buffer size %d for smd.\n", smd_size);
	sig = (char *)
		SOAP_MALLOC(soap, smd_size);
	if(!sig) {
		perror("malloc");
		fprintf(stderr, "Couldn't malloc for signed ClassAd signature.\n");
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
		fprintf(stderr, "error performing signature.\n");
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
MyString 
getClassAdSigWSSE(ClassAd *ad)
{
    //struct soap *soap;
    //soap = soap_new1(SCA_XML_FORMAT);
	char tmp_file[] = "/tmp/.condor_scaXXXXXX";
	int tmp_fd;
	char *sca_mem;
	struct condor__ClassAdStruct *adStruct;
	struct _condor__signatureRequest req;
	char *quoted_sca;
	MyString rv;

	struct soap *soap = soap_new1(SCA_XML_FORMAT);
    soap_register_plugin(soap, soap_wsse);
    //soap_omode(&soap, SOAP_ENC_ZLIB | SOAP_XML_GRAPH); // see 8.12
    //soap_omode(soap, SOAP_ENC_XML | SOAP_XML_GRAPH | SOAP_XML_INDENT);
    //soap_omode(soap, SOAP_ENC_XML | SOAP_XML_GRAPH | SOAP_XML_CANONICAL);

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

    if(soap_wsse_sign_body(soap, SOAP_SMD_HMAC_SHA1,
                           key, sizeof(key))) {
        soap_print_fault(soap, stderr);
    }	
	
    soap_wsse_verify_auto(soap, SOAP_SMD_HMAC_SHA1, key, sizeof(key));
	adStruct = (struct condor__ClassAdStruct *)
		soap_malloc(soap, sizeof(struct condor__ClassAdStruct));
	
	convert_ad_to_adStruct(soap, ad, adStruct, true);
	req.classAd = adStruct;
	if(soap_wsse_sign_body(soap, SOAP_SMD_HMAC_SHA1, key, sizeof(key))) {
		dprintf(D_SECURITY, "Soap error occurred.\n");
		goto return_error;
	}
    if(soap_send___condor__signature(soap, "http://", NULL, &req) == SOAP_OK) {
//    if(soap_call___ns1__successor(soap, "http://ferdinand.cs.wisc.edu:31310", NULL, x, &succ) == SOAP_OK) {
		soap_end_send(soap);
		soap_closesock(soap);
		soap_end(soap);
		soap_done(soap);
		free(soap);
		close(tmp_fd);
		verify_unquotedWSSE(sca_mem);
		dprintf(D_SECURITY, "Signature length: %d\n", strlen(sca_mem));
		//dprintf(D_SECURITY, "Signature bytes: %s\n", sca_mem);
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
}*/

EVP_PKEY *
get_proxy_cert_from_ad(ClassAd *ad) 
{
	char *tmp_cert = NULL;
	ad->LookupString("ClassAdSignatureCert",&tmp_cert); // free tmp_cert
	if(tmp_cert == NULL) {
		fprintf(stderr, "Can't get signing cert from ad.\n");
		return NULL;
	}
	MyString cert = tmp_cert;
	free(tmp_cert); // freed tmp_cert
	while(cert.replaceString("\\n","\n")) { ; }
	//dprintf(D_SECURITY, "Got cert: '%s'\n", cert.GetCStr());

	char *tmp = (char *)malloc(cert.Length()+1);
	if(!tmp) {
		perror("malloc");
		fprintf(stderr, "Malloc error.\n");
		return NULL;
	}
	sprintf(tmp, "%s", cert.GetCStr());

	BIO *mem;
	mem = BIO_new_mem_buf(tmp, -1);
	if(!mem) {
		fprintf(stderr, "Error getting memory buffer for cert pub key.\n");
		free(tmp);
		return NULL;
	}
	X509 *s_cert = PEM_read_bio_X509(mem, NULL, NULL, NULL);
	if(!s_cert) {
		fprintf(stderr, "Can't read cert.\n");
		BIO_free(mem);
		free(tmp);
		return NULL;
	}
	EVP_PKEY *rv = X509_extract_key(s_cert);
	BIO_free(mem);		
	X509_free(s_cert);
	free(tmp);
	return rv;
}

/*	char tmp_file[] = "/tmp/scaXXXXXX";
	int tmp_fd;
	tmp_fd = mkstemp(tmp_file); // close tmp_fd
	if(tmp_fd == -1) {
		perror("mkstemp");
		fprintf(stderr, "Couldn't create temp file.\n");
		return NULL;
	}
	int data_len = cert.Length();
	if(write(tmp_fd, cert.GetCStr(), data_len) != data_len) {
		fprintf(stderr, "Write error.\n");
		close(tmp_fd);
		return NULL;
	}
	close(tmp_fd);
	tmp_fd = safe_open_wrapper(tmp_file, O_RDONLY);
	if(tmp_fd == -1) {
		perror("open");
		fprintf(stderr, "Error opening temp file.\n");
		return NULL;
		}*/

int
insertProxyCertIntoAd(ClassAd *ad) 
{
	char *proxy_filename = NULL;
	ad->LookupString("x509userproxy", &proxy_filename); // free me
	if(proxy_filename == NULL) {
		fprintf(stderr, "Can't get x509userproxy from ClassAd.\n");
		return 0;
	}
	FILE *fp = safe_fopen_wrapper(proxy_filename, "r"); // close me
	if(!fp) {
		fprintf(stderr, "insert_proxy_cert_into_ad: "
				"Can't open the x509 user proxy file '%s'.\n",
				proxy_filename);
		free(proxy_filename);
		return 0;
	}
	free(proxy_filename);
/*	char *line = (char *)malloc(100); // free me;
	if(line == NULL) {
		perror("malloc");
		fprintf(stderr, "Couldn't malloc in signClassAd.\n");
		fclose(fp);
		return 0;
	}
	int mrv = 0; */
	MyString cert = "";
/*	size_t line_len = 0;
	while((mrv = getline(&line, &line_len, fp)) > 0) {
*/
	char *line = NULL;
	while((line = getline(fp))) {
		cert += line;
		cert += "\n";
		//dprintf(D_SECURITY, "Got line '%s'\n",line);
		//free(line);
		if(!strcmp(line, "-----END CERTIFICATE-----")) {
			break;
		}
	}
	fclose(fp);
/*	free(line);
	if(mrv <= 0) {
		fprintf(stderr, "Error reading from x509 user proxy file.\n");
		return 0;
		} */
	while(cert.replaceString("\n", "\\n")) { ; }
	//fprintf(stderr, "Inserting cert into ad: '%s'\n", cert.GetCStr());
	InsertIntoAd(ad,"ClassAdSignatureCert",cert.GetCStr());
	return 1;
}

EVP_PKEY *
get_proxy_private_key(ClassAd *ad)
{
	char *proxy_filename = NULL;
	ad->LookupString("x509userproxy", &proxy_filename);
	if(proxy_filename == NULL) {
		fprintf(stderr, "Can't get x509userproxy from ClassAd.\n");
		return NULL;
	}
	FILE *fd = safe_fopen_wrapper(proxy_filename, "r");
	if(!fd) {
		fprintf(stderr, "get_proxy_private_key: "
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

MyString
getClassAdSigSMD(ClassAd *ad)
{
	char *quoted_rv;
	MyString quoted_mrv;
	struct soap *soap = soap_new1(SCA_XML_FORMAT);
	if(!soap) {
		fprintf(stderr, "Error creating soap object.\n");
		return "";
	}

	struct condor__ClassAdStruct *adStruct = (struct condor__ClassAdStruct *)
		soap_malloc(soap, sizeof(struct condor__ClassAdStruct));

	if(!adStruct) {
		perror("malloc");
		soap_end(soap);
		soap_destroy(soap);
		free(soap);
		fprintf(stderr, "Couldn't malloc for signed ClassAd structure.\n");
		return "";
	}
	convert_ad_to_adStruct(soap, ad, adStruct, true);

	// get private key.
	EVP_PKEY *key = get_proxy_private_key(ad);
	if(!key) {
		fprintf(stderr, "Couldn't get x509 user proxy.\n");
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
		fprintf(stderr, "error getting signature.\n");
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
		if(!bin2hex(sig, siglen, &hex)) {
			fprintf(stderr, "Couldn't convert binary signature to hex.\n");
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
			fprintf(stderr, "Couldn't open temp file in signClassAd.\n");
			goto error_free;
		}
		char *line = (char *)malloc(1025);
		if(line == NULL) {
			perror("malloc");
			fprintf(stderr, "Couldn't malloc in signClassAd.\n");
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
				fprintf(stderr, "Error reading from serialized "
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


MyString 
getClassAdSig(ClassAd *ad) 
{
	//dprintf(D_SECURITY, "Signing.\n");
	MyString rv = getClassAdSigSMD(ad);
	//dprintf(D_SECURITY, "Signed.\n");
	return rv;
}

ClassAd *
limitClassAd(ClassAd *ad, StringList *include) 
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
    
MyString
signClassAd(ClassAd *ad, StringList *include)
{
	ClassAd *new_ad = limitClassAd(ad, include);
	if(!new_ad) {
		fprintf(stderr, "Can't get limited class ad.\n");
		return "";
	}
	MyString rv = getClassAdSig(new_ad);
	delete(new_ad);
	return rv;
}

/*
int
verifySignedClassAdWSSE(ClassAd *jobad) 
{
	char *unquoted_sig;
	char *sig_val = NULL;
	
	jobad->LookupString("ClassAdSignature", &sig_val);
	if(sig_val == NULL) {
		fprintf(stderr, "Couldn't get classAd Signature.\n");
		exit(1);
	} else { 
		//fprintf(stderr, "ClassAdSignature = '%s'\n", sig_val);
	}
	unquoted_sig = unquote_classad_string(sig_val);
	return verify_unquotedWSSE(unquoted_sig);

}
*/

int 
getSigAndDataFromAd(ClassAd *ad, char **signature, int *sig_len, 
						MyString *rdata)
{
	// cas = ClassAd signature
	char *unquoted_cas;
	char *cas = NULL;

	*signature = NULL;
	*sig_len = 0;
	
	ad->LookupString("ClassAdSignature", &cas); // free me
	if(cas == NULL) {
		fprintf(stderr, "getSigAndDataFromAd: "
				"Couldn't get classAd Signature.\n");
		return 0;
	}
	unquoted_cas = unquote_classad_string(cas); //free me
	free(cas);
	cas = NULL;
	if(unquoted_cas == NULL) {
		fprintf(stderr, "getSigAndDataFromAd: "
				"Couldn't unquote raw signature.\n");
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
		fprintf(stderr, "getSigAndDataFromAd: "
				"Couldn't find ':' separator in ClassAdSignature.\n");
		free(unquoted_cas);
		return 0;
	}
	// Turn the signature into binary.
	int binary_sig_len = ascii_sig_len/2;
	char *binary_sig = (char *)malloc(binary_sig_len); // caller frees me
	if(binary_sig == NULL) {
		perror("malloc");
		fprintf(stderr, "getSigAndDataFromAd: can't malloc.\n");
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

int
getAdFromSigData(MyString rdata, ClassAd *ret_ad) 
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
		fprintf(stderr, "Couldn't malloc for signed ClassAd structure.\n");
		goto error_soap_done;
	}
	tmp_fd = mkstemp(tmp_file);
	if(tmp_fd == -1) {
		perror("mkstemp");
		fprintf(stderr, "Couldn't create temp file.\n");
		goto error_free_adstruct;
	}

	data_len = rdata.Length();
	if(write(tmp_fd, rdata.GetCStr(), data_len) != data_len) {
		fprintf(stderr, "Write error.\n");
		goto error_close_fd;
	}
	close(tmp_fd);
	tmp_fd = safe_open_wrapper(tmp_file, O_RDONLY);
	if(tmp_fd == -1) {
		perror("open");
		fprintf(stderr, "Couldn't read temp file.\n");
		goto error_unlink_tmp;
	}

	soap_begin(soap);
	soap->recvfd = tmp_fd;
	soap_begin_recv(soap);
	if(!soap_get_PointerTocondor__ClassAdStruct(soap, &adStruct,
												"condor:ClassAd",
												"condor:ClassAdStruct")) {
		soap_print_fault(soap, stderr);
		fprintf(stderr, "Error deserializing.");
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
int
verifySignedClassAdSMD(ClassAd *jobad)
{
	MyString rdata;
	char *signature;
	int sig_len;
	int rv = 0;
	//dprintf(D_SECURITY, "Getting sig and data from ad.\n");
	if(!getSigAndDataFromAd(jobad, 
							&signature, // free signature
							&sig_len, 
							&rdata)) { 
		fprintf(stderr, "Error obtaining signature and data from ClassAd.\n");
		return 0;
	}
	//dprintf(D_SECURITY, "getting ad from sig data.\n");
	ClassAd *sigAd = new ClassAd();
	if(!getAdFromSigData(rdata, sigAd)) {
		fprintf(stderr, "Couldn't extract the signed ad "
				"from the signature.\n");
		free(signature);
		return 0;
	}
	//sigAd->fPrint(stderr);
	//dprintf(D_SECURITY, "Getting key.\n");
//	sigAd->fPrint(stderr);
	EVP_PKEY *key = get_proxy_cert_from_ad(sigAd);
	if(!key) {
		fprintf(stderr, "Couldn't get proxy certificate from ClassAd.\n");
		free(signature);
		delete(sigAd);
		return 0;
	}
	//dprintf(D_SECURITY, "got key.\n");
	struct soap *soap = soap_new1(SCA_XML_FORMAT);
	struct condor__ClassAdStruct *adStruct = (struct condor__ClassAdStruct *)
		soap_malloc(soap, sizeof(struct condor__ClassAdStruct));

	if(!adStruct) {
		perror("malloc");
		free(signature);
		delete(sigAd);
		fprintf(stderr, "Couldn't malloc for signed ClassAd structure.\n");
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
		dprintf(D_SECURITY, "get_raw_sig: "
				"Couldn't create temp file: '%s'\n", tmp_file);
		free(signature);
		delete(sigAd);
		return 0;
	}		
	//fprintf(stderr, "Verify: Serializing to file %s\n", tmp_file);

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
	
/*	MyString sig2 = getClassAdSigSMD(&ad);
	return !strncmp(sig2.GetCStr(), sig_val, 40);
	return 0;
	// Check that the adStruct+key produces the same signature.
	int verify_siglen;
	char *verify_sig;
	char dummy_tmp_file[] = "/tmp/scaXXXXXX";
	fprintf(stderr, "Computing verification sig.\n");
	if(!get_raw_sig(soap, adStruct, &verify_siglen, &verify_sig, 
					dummy_tmp_file)) {
		fprintf(stderr, "error getting signature.\n");
		// goto return_error;
	} else {
		char *pv;
		bin2hex(verify_sig, verify_siglen, &pv);
		fprintf(stderr, "Verification sig: %s\n", pv);
		if(strncmp(verify_sig, binary_sig, verify_siglen)) {
			fprintf(stderr, "Error: signatures don't match.\n");
			// goto return error;
		} else {
			fprintf(stderr, "Success: signatures match.\n");
			return 1;
		}
	}
	// return the ad and the boolean result
	return 0;
*/


int
verifySignedClassAd(ClassAd *jobad) 
{
	return verifySignedClassAdSMD(jobad);
}
