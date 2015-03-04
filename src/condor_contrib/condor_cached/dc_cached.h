/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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

#ifndef _CONDOR_DC_CACHED_H
#define _CONDOR_DC_CACHED_H

#include "CondorError.h"
#include "daemon.h"

class DCCached : public Daemon
{
public:

	DCCached(const char * name = NULL, const char *pool = NULL);
	DCCached(const ClassAd* ad, const char* pool);

	~DCCached() {}

	// On successful invocation, cacheName is replaced with the resulting
	// cacheName and expiry is updated to show the expiration time the server
	// selected.
	int createCacheDir(std::string &cacheName, time_t &expiry, CondorError &err);

	// Upload files to the cache
	// TODO: How to pass a list?  A classad list? std::list?
	int uploadFiles(const std::string &cacheName, const std::list<std::string> files, CondorError &err);

	// Download files from the cache
	// TODO: How to pass a list?  A classad list? std::list?
	int downloadFiles(const std::string &cacheName, const std::string dest, CondorError &err);
	
	// Remove cache directories
	int removeCacheDir(const std::string &cacheName, CondorError &err);
	
	// Set the replication policy
	int setReplicationPolicy(const std::string &cacheName, const std::string &policy, const std::string &methods, CondorError &err);
	
	// Get a list of cache ads.  If both cacheName and requirements are empty, 
	// return ALL caches stored on the cached.
	int listCacheDirs(const std::string &cacheName, const std::string& requirements, std::list<compat_classad::ClassAd>& result_list, CondorError& err);
	
	/**
		*	Requests for a connected cached to replicate a cache
		*
		*/
		
	// Mostly non-blocking version of request local cache.  The protocol states
	// that the cached will return as soon as possible a classad saying something...
	int requestLocalCache(const std::string &cached_server, const std::string &cached_name, compat_classad::ClassAd& response, CondorError& err);
	
	
private:
	
	// Perform a hardlink transfer
	int DoHardlinkTransfer(const std::string cacheName, const std::string dest, ReliSock* rsock, CondorError& err);


};

#endif // _CONDOR_DC_CACHED_H
