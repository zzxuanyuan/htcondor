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
  
#ifndef AMAZONRESOURCE_H
#define AMAZONRESOURCE_H
    
#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "amazonjob.h"
#include "baseresource.h"
#include "gahp-client.h"

#define AMAZON_RESOURCE_NAME "amazon"
  
class AmazonJob;
class AmazonResource;

class AmazonResource : public BaseResource
{
public:
	void Reconfig();
	
	static const char *HashName( const char * resource_name, 
								 const char * access_key_file, 
								 const char * secret_key_file );
	
	static AmazonResource* FindOrCreateResource( const char * resource_name, 
												 const char * access_key_file, 
												 const char * secret_key_file );

	GahpClient *gahp;

	AmazonResource(const char * resource_name, 
				   const char * access_key_file, 
				   const char * secret_key_file );
	
	~AmazonResource();	

	static HashTable <HashKey, AmazonResource *> ResourcesByName;
	
private:
	void DoPing(time_t & ping_delay, 
				bool & ping_complete, 
				bool & ping_succeeded  );
	
	char* m_access_key_file;
	char* m_secret_key_file;	
};    
  
#endif
