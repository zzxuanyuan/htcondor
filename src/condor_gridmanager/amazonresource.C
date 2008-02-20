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
  
#include "condor_common.h"
#include "condor_config.h"
#include "string_list.h"

#include "amazonresource.h"
#include "gridmanager.h"

#define HASH_TABLE_SIZE	500

HashTable <HashKey, AmazonResource *> 
	AmazonResource::ResourcesByName( HASH_TABLE_SIZE, hashFunction );

const char * AmazonResource::HashName( const char * resource_name,
		const char * access_key_file, const char * secret_key_file )
{								 
	static MyString hash_name;
	hash_name.sprintf( "%s#%s#%s", resource_name, access_key_file, secret_key_file );
	return hash_name.Value();
}


AmazonResource* AmazonResource::FindOrCreateResource(const char * resource_name, 
	const char * access_key_file, const char * secret_key_file )
{
	int rc;
	MyString resource_key;
	AmazonResource *resource = NULL;

	rc = ResourcesByName.lookup( HashKey( HashName( resource_name, access_key_file, secret_key_file ) ), resource );
	if ( rc != 0 ) {
		resource = new AmazonResource( resource_name, access_key_file, secret_key_file );
		ASSERT(resource);
		resource->Reconfig();
		ResourcesByName.insert( HashKey( HashName( resource_name, access_key_file, secret_key_file ) ), resource );
	} else {
		ASSERT(resource);
	}

	return resource;
}


AmazonResource::AmazonResource( const char *resource_name, 
	const char * access_key_file, const char * secret_key_file )
	: BaseResource( resource_name )
{
	// although no one will use resource_name, we still keep it for base class constructor
	
	m_access_key_file = strdup(access_key_file);
	m_secret_key_file = strdup(secret_key_file);
	
	gahp = NULL;

	MyString buff;
	buff.sprintf( AMAZON_RESOURCE_NAME );
	
	char * gahp_path = param( "AMAZON_GAHP" );
	if ( gahp_path == NULL ) {
		dprintf(D_ALWAYS, "AMAZON_GAHP not defined! \n");
		return;
	}
	
	ArgList args;
	args.AppendArg("-f");

	gahp = new GahpClient( buff.Value(), gahp_path, &args );
	gahp->setNotificationTimerId( pingTimerId );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( AmazonJob::gahpCallTimeout );
}


AmazonResource::~AmazonResource()
{
	if ( gahp ) delete gahp;
	if (m_access_key_file) free(m_access_key_file);
	if (m_secret_key_file) free(m_secret_key_file);
}


void AmazonResource::Reconfig()
{
	BaseResource::Reconfig();
	gahp->setTimeout( AmazonJob::gahpCallTimeout );
}


// we will use amazon command "status_all" to do the Ping work
void AmazonResource::DoPing( time_t& ping_delay, bool& ping_complete, bool& ping_succeeded )
{
	// Since Amazon doesn't use proxy, we should use Startup() to replace isInitialized()
	if ( gahp->Startup() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying ping\n" );
		ping_delay = 5;		
		return;
	}
	
	ping_delay = 0;
	
	int rc = gahp->amazon_ping( m_access_key_file, m_secret_key_file );

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		ping_complete = false;
	} 
	else if ( rc != 0 ) {
		ping_complete = true;
		ping_succeeded = false;
	} 
	else {
		ping_complete = true;
		ping_succeeded = true;
	}

	return;
}
