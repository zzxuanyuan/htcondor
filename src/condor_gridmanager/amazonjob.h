/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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
	void SetRemoteJobId( const char *job_id );

	static int probeInterval;
	static int submitInterval;
	static int gahpCallTimeout;
	static int maxConnectFailures;
	static int maxReTries;

	static void setProbeInterval( int new_interval ) 	{ probeInterval = new_interval; }
	static void setSubmitInterval( int new_interval )	{ submitInterval = new_interval; }
	static void setGahpCallTimeout( int new_timeout )	{ gahpCallTimeout = new_timeout; }
	static void setConnectFailureRetry( int count )		{ maxConnectFailures = count; }

	int gmState;
	time_t lastProbeTime;
	bool probeNow;
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
	MyString* build_ami_id();
	MyString* build_keypair();
	MyString* build_keypairfilename();
	MyString* build_dirname();
	char* build_xml_file(const char* dirname);
	StringList* build_groupnames();
	
	char * m_access_key_file;
	char * m_secret_key_file;
	char * m_bucket_name;
	char* m_xml_file;
	MyString* m_ami_id;
	MyString* m_key_pair;
	MyString* m_key_pair_file_name;
	MyString* m_dir_name;
	StringList* m_group_names;	
	
	// create temporary names when clients don't assign the values
	const char* temporary_keypair_name();
	const char* temporary_keypair_file();
	const char* temporary_security_group();
	const char* temporary_bucket_name();
	
	// remove created temporary keypair file
	bool remove_keypair_file(const char* filename);
	// create common temporary name
	const char* get_common_temp_name();
};

#endif
