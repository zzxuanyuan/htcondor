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

  
#ifndef AMAZONJOB_H
#define AMAZONJOB_H
  
#include "condor_common.h"
#include "condor_classad.h"
#include "MyString.h"
#include "classad_hashtable.h"

#include "basejob.h"
#include "amazonresource.h"
#include "proxymanager.h"
#include "gahp-client.h"
#include "vm_univ_utils.h"

void AmazonJobInit();
void AmazonJobReconfig();
BaseJob *AmazonJobCreate( ClassAd *jobad );
bool AmazonJobAdMatch( const ClassAd *job_ad );

class AmazonResource;

class AmazonJob : public BaseJob
{
public:

	AmazonJob( ClassAd *classad );
	~AmazonJob();

	void Reconfig();
	int doEvaluateState();
	BaseResource *GetResource();
	void SetKeypairId( const char *keypair_id );
	void SetInstanceId( const char *instance_id );
	void SetRemoteJobId( const char *keypair_id, const char *instance_id );
	void SetSubmitStepInfo(const char * info);
	void SetRemoteVMName(const char * name);
	
	static int probeInterval;
	static int submitInterval;
	static int gahpCallTimeout;
	static int maxConnectFailures;
	static int funcRetryInterval;
	static int pendingWaitTime;
	static int maxRetryTimes;
	
	static void setProbeInterval( int new_interval ) 	{ probeInterval = new_interval; }
	static void setSubmitInterval( int new_interval )	{ submitInterval = new_interval; }
	static void setGahpCallTimeout( int new_timeout )	{ gahpCallTimeout = new_timeout; }
	static void setConnectFailureRetry( int count )		{ maxConnectFailures = count; }

	int gmState;
	time_t lastProbeTime;
	time_t enteredCurrentGmState;
	time_t lastSubmitAttempt;
	int numSubmitAttempts;

	MyString errorString;
	char *remoteJobId;
	MyString remoteJobState;

	AmazonResource *myResource;
	GahpClient *gahp;

	// These get set before file stage out, but don't get handed
	// to JobTerminated() until after file stage out succeeds.
	int exitCode;
	bool normalExit;

private:
	// create dynamic input parameters
	MyString build_ami_id();
	MyString build_keypair();
	StringList* build_groupnames();
	
	char * m_access_key_file;
	char * m_secret_key_file;
	char * m_user_data;
	char * m_user_data_file;
	char * m_instance_type;
	
	int m_submit_step;
	int m_vm_check_times;
	int m_keypair_check_times;
	
	MyString m_ami_id;
	MyString m_key_pair;
	MyString m_key_pair_file_name;
	StringList* m_group_names;
	
	// create temporary names when clients don't assign the values
	char* temporary_keypair_name();
	
	// remove created temporary keypair file
	bool remove_keypair_file(const char* filename);

	// create common temporary name
	const char* get_common_temp_name();
	
	// print out error codes returned from grid_manager
	void print_error_code( const char* error_code, const char* function_name );
	
	void stopcode();
};

#endif
