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

#include "condor_common.h"

#include "condor_debug.h"

#if defined(HAVE_EXT_OPENSSL)
#include "openssl_helpers.h"

#include "openssl/bio.h"
#include "openssl/err.h"
#include "openssl/evp.h"


#define BUFSIZE 1024*8

enum hashstate {
	HASH_UNINITIALIZED,
	HASH_COMPLETED,
	HASH_ERROR
};

struct hash {
	const char *file_name;
	char *hash_value;
	const EVP_MD *hash_type;
	enum hashstate hash_state;
};

/*
 * Calculate the hash_value of the file in file_name.  Uses type
 * hash_type, sets hash_state as a return value.
 * 
 * TODO: make the errors go to dprintf.
 */
struct hash *
get_hash(const char *fn, const char *type) 
{
	BIO *bmd = NULL;
	BIO *in, *err, *out;
	struct hash *h = (struct hash *)malloc(sizeof(struct hash));
	if(!h) return h;
	unsigned char buf[BUFSIZE];
	unsigned int len;
	int i;
	const EVP_MD *md = NULL;

	md = EVP_get_digestbyname(type);
	if(md == NULL) {
		dprintf(D_ALWAYS, "Can't get digest with name '%s'\n", type);
		return h;
	}

	h->hash_state = HASH_UNINITIALIZED;
	h->file_name = fn;
	h->hash_type = md;
	h->hash_value = NULL;		

	err = BIO_new(BIO_s_file());
	if(!err) {
		dprintf(D_ALWAYS, "Can't create error bio.\n");
		return h;
	}

	BIO_set_fp(err,stderr,BIO_NOCLOSE|BIO_FP_TEXT);
	//printf("%s\n", h->file_name);
	in = BIO_new(BIO_s_file());
	bmd = BIO_new(BIO_f_md());
	if(in == NULL || bmd == NULL) {
		report_openssl_errors("get_hash");		
		return h;
	}
	out = BIO_new_fp(stdout, BIO_NOCLOSE);
	if(out == NULL) {
		report_openssl_errors("get_hash");
		return h;
	}
	if(!BIO_set_md(bmd,h->hash_type)) {
		dprintf(D_ALWAYS, "Can't set digest type '%s'.\n", type);
		report_openssl_errors("get_hash");
		return h;
	}
	in = BIO_push(bmd,in);
	if(BIO_read_filename(in, fn) <= 0) {
		perror(fn);
		return h;
	}
	for(;;) {
		i = BIO_read(in,(char *)buf, BUFSIZE);
		if(i < 0) {
			dprintf(D_ALWAYS, "Read error.\n");
			report_openssl_errors("get_hash");
			return h;
		}
		if(i == 0) {
			break;
		}
	}
	len = BIO_gets(in, (char *)buf, BUFSIZE);
	h->hash_value = (char *)malloc(len*2+1);
	if(!h->hash_value) {
		dprintf(D_ALWAYS, "Out of memory.\n");
		return h;
	}
	h->hash_state = HASH_ERROR;
	for(i = 0; i < (int)len; i++) {
		sprintf(h->hash_value+2*i, "%02x",buf[i]);
	}
	h->hash_state = HASH_COMPLETED;
	return h;
}

char *
get_hash_of_file(const char *file_name, const char *hash_type)
{
	struct hash *h = get_hash(file_name, hash_type);
    if(h->hash_state == HASH_COMPLETED) {
		return h->hash_value; // caller frees;
	} 
	return NULL;
}

#endif /* defined(HAVE_EXT_OPENSSL) */
