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
  
#ifndef AMAZONRESOURCE_H
#define AMAZONRESOURCE_H
    
#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "amazonjob.h"
#include "baseresource.h"
#include "gahp-client.h"

#define AMAZON_RESOURCE_NAME "AMAZON"
  
class AmazonJob;
class AmazonResource;

class AmazonResource : public BaseResource
{
public:
	void Reconfig();

	static AmazonResource* FindOrCreateResource( const char *resource_name );

	GahpClient *gahp;

	// in future add status_all() for all jobs. It will display all the jobs running on the Amazon resource.

protected:
	// the constructor should be protected so that we can use Singleton Pattern
	AmazonResource(const char *resource_name );
	~AmazonResource();	
	
private:
	void DoPing(time_t& ping_delay, bool& ping_complete, bool& ping_succeeded  );
	
	static AmazonResource* _instance;	// for singleton pattern
};    
  
#endif
