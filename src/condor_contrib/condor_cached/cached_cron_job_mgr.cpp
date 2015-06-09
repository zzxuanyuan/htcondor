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


#include "condor_common.h"
#include "cached_cron_job_mgr.h"
#include "cached_cron_job.h"
#include "condor_config.h"

// Basic constructor
CachedCronJobMgr::CachedCronJobMgr( void )
		: CronJobMgr( )
{
}

// Basic destructor
CachedCronJobMgr::~CachedCronJobMgr( void )
{
	dprintf( D_FULLDEBUG, "CachedCronJobMgr: Bye\n" );
}

int
CachedCronJobMgr::Initialize( const char *name )
{
	int status;

	SetName( name, name, "_cron" );
	
	status = CronJobMgr::Initialize( name );
	
	return status;
	
}


ClassAdCronJobParams *
CachedCronJobMgr::CreateJobParams( const char *job_name )
{
	return new ClassAdCronJobParams( job_name, *this );
}


CachedCronJob *
CachedCronJobMgr::CreateJob( CronJobParams *job_params )
{
	dprintf( D_FULLDEBUG,
			 "*** Creating Cached Cron job '%s'***\n",
			 job_params->GetName() );

	ClassAdCronJobParams *params =	dynamic_cast<ClassAdCronJobParams *>( job_params );
	return new CachedCronJob( params, *this );
}


int CachedCronJobMgr::Publish(std::string name, compat_classad::ClassAd& ad) {
	
	dprintf(D_FULLDEBUG, "Got pubish in cron job mgr\n");
	
	// Add the incoming ad to the list of ads with name
	m_job_ads[name] = ad;
	
}

int CachedCronJobMgr::GetAds(compat_classad::ClassAd& ad) {
	
	classad_unordered<std::string, compat_classad::ClassAd>::iterator it = m_job_ads.begin();
	
	//m_parent.parent_ad->EvalString(ATTR_NAME, NULL, parent_name);
	
	for (it = m_job_ads.begin(); it != m_job_ads.end(); it++) {
		std::string cache_name = it->first;
		compat_classad::ClassAd job_ad = it->second;
		ad.Update(job_ad);
	}
	
}
