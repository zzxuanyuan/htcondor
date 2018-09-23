/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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

#ifndef _CACHED_CRON_JOB_MGR_H
#define _CACHED_CRON_JOB_MGR_H

#include "enum_utils.h"
#include "condor_cron_job_mgr.h"
#include "cached_cron_job.h"
//#include "startd_cron_job_params.h"

// Define a simple class to run child tasks periodically.
class CachedCronJobMgr : public CronJobMgr
{
  public:
		CachedCronJobMgr( void );
	~CachedCronJobMgr( void );
	int Initialize( const char *name );
	
	int Publish(std::string name, compat_classad::ClassAd& ad);
	
	int GetAds(compat_classad::ClassAd& ad);

  protected:
	//StartdCronJobParams *CreateJobParams( const char *job_name );
	CachedCronJob *CreateJob( CronJobParams *job_params );
	ClassAdCronJobParams * CreateJobParams( const char *job_name );

  private:
	classad_unordered<std::string, compat_classad::ClassAd> m_job_ads;
		
	
};

#endif /* _CACHED_CRON_JOB_MGR_H */
