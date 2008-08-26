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

#ifndef __CRED_CHAIN_H__
#define __CRED_CHAIN_H__

#include "condor_common.h"
#ifndef WANT_CLASSAD_NAMESPACE
#define WANT_CLASSAD_NAMESPACE
#endif

#include "MyString.h"
#include "globus_utils.h"
#include "globus_gsi_proxy.h"

class CredChain {
 public:
	CredChain(MyString full_chain);
	CredChain(const char *proxy_file_path);
	CredChain();
//	virtual ~Credential();
	int getNumPolicies();
	bool hasMatchingPolicy(MyString policy_to_match);
	MyString *getFirstPolicy();
	MyString *getLastPolicy();
	char *getExecutionHostDN();

 protected:

	bool initCredChainHandles();
	globus_gsi_cred_handle_t handle;
	globus_gsi_cred_handle_attrs_t handle_attrs;
	bool valid;
};

#endif
