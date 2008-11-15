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

#if defined(HAVE_EXT_OPENSSL) || defined(HAVE_EXT_GLOBUS)
#include "openssl/x509.h"

bool sign_classad(ClassAd &ad,
				  ClassAd *cached_ad,
				  StringList &attributes_to_sign,
				  const MyString &private_key_path,
				  const MyString &public_key_path);

bool verify_classad(ClassAd& ad,
					StringList& attributes_to_verify);

#endif /* defined(HAVE_EXT_OPENSSL) || defined(HAVE_EXT_GLOBUS) */

bool generic_sign_classad(ClassAd &ad, ClassAd *cached_ad, bool is_job_ad);

bool generic_verify_classad(ClassAd ad, bool is_job_ad);

bool get_file_text(MyString &file_name, MyString &text);

bool verify_classad(ClassAd& ad,
					StringList &attributes_to_verify);

bool text2classad(const MyString &text, ClassAd &ad);

MyString unquote_classad_string(const MyString &in);

bool verify_certificate(X509 *cert);

int x509_self_delegation(const char *proxy_loc, char *new_proxy_loc, const char *policy, const char *policy_oid);
