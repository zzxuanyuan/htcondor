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

#ifndef __CACHED_SERVER_H__
#define __CACHED_SERVER_H__

struct sqlite3;

class CachedServer: Service {
 public:
    CachedServer();
    ~CachedServer();
    
    void InitAndReconfig();
    
    
 private:
   
   // CMD API's
   int CreateCacheDir(int cmd, Stream *sock);
   int UploadFiles(int cmd, Stream *sock);
   int DownloadFiles(int cmd, Stream *sock);
   int RemoveCacheDir(int cmd, Stream *sock);
   int UpdateLease(int cmd, Stream *sock);
   int ListCacheDirs(int cmd, Stream *sock);
   int ListFilesByPath(int cmd, Stream *sock);
   int CheckConsistency(int cmd, Stream *sock);
   int SetReplicationPolicy(int cmd, Stream *sock);
   int GetReplicationPolicy(int cmd, Stream *sock);
   int CreateReplica(int cmd, Stream *sock);
   
	std::string m_db_fname;
	sqlite3 *m_db;
	bool m_registered_handlers;
    
    
};





#endif

