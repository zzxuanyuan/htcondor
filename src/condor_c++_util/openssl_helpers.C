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

#include "openssl/bio.h"
#include "openssl/err.h"

#define LINE_LENGTH 70


/*
 * report_openssl_errors
 * 
 * Pulls errors from the ERR functions in OpenSSL and puts them into
 * the standard Condor dprintf.
 */
void
report_openssl_errors(const char *func_name) {
    char *err_buf;
    BIO *err_mem_bio = NULL;
    err_mem_bio = BIO_new(BIO_s_mem());
    if(!err_mem_bio) {
        dprintf(D_ALWAYS, "Error creating buffer for error reporting (%s).\n",
                func_name);
        return;
    }
    err_buf = (char *)malloc(LINE_LENGTH+1);
    if(!err_buf) {
        dprintf(D_ALWAYS, "Malloc error (%s).\n", func_name);
        BIO_free(err_mem_bio);
        return;
    }
    ERR_print_errors(err_mem_bio);
    while(BIO_gets(err_mem_bio, err_buf, LINE_LENGTH)) {
        dprintf(D_ALWAYS, "OpenSSL error (%s): '%s'\n", func_name, err_buf);
    }
    BIO_free(err_mem_bio);
    free(err_buf);
}

#endif /* defined(HAVE_EXT_OPENSSL) */
