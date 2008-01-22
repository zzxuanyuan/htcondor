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
#ifndef _CONDOR_FETCH_WORK_MGR_H
#define _CONDOR_FETCH_WORK_MGR_H

#include "condor_common.h"
#include "startd.h"

class FetchClient;

/**
   The FetchWorkMgr manages all attempts to fetch work via hooks.
*/

class FetchWorkMgr : public Service
{
public:
	FetchWorkMgr(); 
	~FetchWorkMgr();

	bool init();
	bool reconfig();

		/**
		   Figure out if we should try to fetch work, invoke the hook
		   with a slot classad, and register a handler for the reply.

		   This never initiates a state change, only the handler does.

		   @param rip Pointer to a Resource object to try to fetch work for.
		   @return True if a request was sent and a handler registered.
		*/
	bool fetchWork(Resource* rip);

		/**
		   Handle a reply to a request to fetch work.
		   
		   2 Possible responses:
		   #1 Indication there's no work to do:
		   ?? See if another server might have work and try another fetch.
		   -- Return false.
		   #2 ClassAd describing the job to run:
		   -- Verify the slot is still available or return false.
           ?? If not, do matchmaking on all other slots to find a fit?
		   -- Create or update the current Claim and Client objects.
		   -- Initiate a state change to Claimed/Busy.
		   -- Return true.

		   @param fetch_client Pointer to the FetchClient that replied.

		   @return True if work was accepted.
		*/
	bool handleFetchResult(FetchClient* fetch_client);

		/**

		 */
	void sendClaimReply(bool claimed, ClassAd* job_ad, ClassAd* slot_ad);

		/**

		 */
	bool claimRemoved(Claim* claim);


private:
	FetchClient* buildFetchClient(Resource* rip);

	bool removeFetchClient(FetchClient* fetch_client);

	SimpleList<FetchClient*> m_fetch_clients;

	void clearHookPaths( void );
	char* initHookPath( const char* hook_param );
	char* m_hook_fetch_work;
	char* m_hook_claim_response;
	char* m_hook_claim_destroy;
};


/**
   Each FetchClient object manages an invocation of the fetch work hook.
*/
class FetchClient : public Service
{
public:
	friend class FetchWorkMgr;

	FetchClient(Resource* rip);
	virtual ~FetchClient();

	bool startFetch();
	ClassAd* reply();

protected:
	Resource* m_rip;
	ClassAd* m_job_ad;
};


#endif /* _CONDOR_FETCH_WORK_MGR_H */
