/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

// Note: This class is largely deprecated.  This is because condor_q++
// now talks directly to postgres instead of calling this command

#include <time.h>
#include "condor_common.h"
#include "condor_io.h"
#include "condor_fix_assert.h"
#include "requestservice.h"
#include "../condor_schedd.V6/qmgmt_constants.h"
#include "classad_collection.h"

#include "condor_attributes.h"

#if defined(assert)
#undef assert
#endif

#define assert(x) if (!(x)) return -1;

//! constructor
/*! \param DBConn DB connection string
 */
RequestService::RequestService(const char* DBConn) {
	jqSnapshot = new JobQueueSnapshot(DBConn);
}

//! destructor
RequestService::~RequestService() {
	jqSnapshot->release();
	delete jqSnapshot;
}

//! service requests from condor_q via socket
/*  NOTE:	
	Much of this method is borrowed from do_Q_request() in qmgmt.C
*/
int
RequestService::service(ReliSock *syscall_sock) {
  //dprintf(D_ALWAYS, "called service\n");
  int request_num;
  int rval;
  
  syscall_sock->decode();
  
  assert(syscall_sock->code(request_num));
  
  dprintf(D_SYSCALLS, "Got request #%d\n", request_num);
  
  switch(request_num) {
    // The impl of this case is done.
  case CONDOR_InitializeReadOnlyConnection:
    {
      //
      // NOTE:
      // I looked into qmgmt.C file.
      // However, the below call doesn't do anything.
      // So I just do nothing, here. 
      // (By Youngsang)
      //
      /* ----- start of the original code 
	 
      // same as InitializeConnection but no authenticate()
      InitializeConnection(NULL, NULL);
      
      end of the original code ---- */
      return 0;
    }
  case CONDOR_CloseConnection:
    {
      int terrno;
      
      assert( syscall_sock->end_of_message() );;
      
      errno = 0;
      rval = closeConnection( );
      terrno = errno;
      dprintf( D_SYSCALLS, "\trval = %d, errno = %d\n", rval, terrno );
      
      syscall_sock->encode();
      assert( syscall_sock->code(rval) );
      if( rval < 0 ) {
	assert( syscall_sock->code(terrno) );
      }
      assert( syscall_sock->end_of_message() );;
      
      //return 0;
      return -2;
    }
    // This case must be implemented. 4/28
  case CONDOR_GetNextJobByConstraint:
    {
      
      char *constraint=NULL;
      ClassAd *ad;
      int initScan;
      int terrno;
      
      assert( syscall_sock->code(initScan) );
      
      if ( !(syscall_sock->code(constraint)) ) {
	if (constraint != NULL) {
	  free(constraint);
	  constraint = NULL;
	}
	return -1;
      }
      assert( syscall_sock->end_of_message() );

      errno = 0;

      ad = getNextJobByConstraint( constraint, initScan );

      /* added by ameet while testing response time of quill
	 int cluster, proc, date, status, prio, image_size;

	 if (!ad->EvalInteger (ATTR_CLUSTER_ID, NULL, cluster)  ||
	 !ad->EvalInteger (ATTR_PROC_ID, NULL, proc)        ||
	 !ad->EvalInteger (ATTR_Q_DATE, NULL, date)         ||
	 !ad->EvalInteger (ATTR_JOB_STATUS, NULL, status)   ||
	 !ad->EvalInteger (ATTR_JOB_PRIO, NULL, prio)       ||
	 !ad->EvalInteger (ATTR_IMAGE_SIZE, NULL, image_size))   
	 {
	 dprintf (D_ALWAYS, " --- ???? --- \n");
	 }
	 else {
	 dprintf(D_ALWAYS, "%4d.%-3d %-2c %-3d %-4.1f\n", 
	 cluster,
	 proc,
	 status,
	 prio,
	 image_size);
	 
	 }
	 end added */
 
      terrno = errno;

      rval = ad ? 0 : -1;
      
      syscall_sock->encode();
      assert( syscall_sock->code(rval) );
      if( rval < 0 ) {
	assert( syscall_sock->code(terrno) );
      }
      if( rval >= 0 ) {
	assert( ad->put(*syscall_sock) );
      }
      freeJobAd(ad);
      free( (char *)constraint );
      assert( syscall_sock->end_of_message() );;
      
      return 0;
    }
  }

  return -1;
}

bool
RequestService::parseConstraint(const char *constraint, 
				int &cluster, int &proc, char *owner) {
  char *ptrC, *ptrP, *ptrO, *ptrT;
  int index_rparen=0, index_equals=0, length=0;
  bool isfullscan = false;
  char *temp_constraint = 
    (char *) malloc((strlen(constraint) + 1) * sizeof(char));
  temp_constraint = strcpy(temp_constraint, constraint);
  
  ptrC = strstr( temp_constraint, "ClusterId == ");
  if(ptrC != NULL) {
    index_rparen = strchr(ptrC, ')') - ptrC;
    ptrC += 13;
    sscanf(ptrC, "%d", &cluster);
    ptrC -= 13;
    for(int i=0; i < index_rparen; i++) ptrC[i] = ' ';
  }
  ptrP = strstr( temp_constraint, "ProcId == ");
  if(ptrP != NULL) {
    index_rparen = strchr(ptrP, ')') - ptrP;
    ptrP += 10;
    sscanf(ptrP, "%d", &proc);
    ptrP -= 10;
    for(int i=0; i < index_rparen; i++) ptrP[i] = ' ';
  }
  
  /* turns out that since we have a vertical schema, we can only
     push the cluster,proc constraint down to SQL.  The owner would
     be a vertical attribute and can't be pushed down.

  ptrO = strstr( temp_constraint, "TARGET.Owner == ");
  if(ptrO != NULL) {
    index_rparen = strchr(ptrO, ')') - ptrO;
    ptrO += 17;
    sscanf(ptrO, "%s", owner);
    ptrO -= 17;
    for(int i=0; i < index_rparen; i++) ptrO[i] = ' ';
    index_equals = strchr(owner, '"') - owner;
    owner[index_equals] = '\0';
  }
  */

  ptrT = strstr( temp_constraint, "TRUE");
  if(ptrT != NULL) {
    for(int i=0; i < 4; i++) ptrT[i] = ' ';
  }
    
  length = strlen(temp_constraint);
  for(int i=0; i < length; i++) {
    if(isalnum(temp_constraint[i])) {
      isfullscan = true;
      break;
    }
  }
  
  //printf("temp_constraint after parsing = %s\n", temp_constraint);
  //printf("cluster = %d, proc = %d, owner = %s\n", cluster, proc, owner);
  free(temp_constraint);
  return isfullscan;
}

//! handle GetNextJobByConstraint request 
/*! \param constraint query 
 *	\param initScan is it the first call?
 */
ClassAd*
RequestService::getNextJobByConstraint(const char* constraint, int initScan)
{
	ClassAd *ad;
	HashKey key;
	bool isfullscan = false;
	int cluster=-1, proc=-1;
	char owner[20] = "";

	if (initScan) { // is it the first request?
	  //printf("constraint = %s\n", constraint);
	  //printf("before: cluster=%d, proc=%d, owner=%s\n", 
	  //	 cluster, proc, owner);  
	  isfullscan = parseConstraint(constraint, cluster, proc, owner);
	  //printf("before: cluster=%d, proc=%d, owner=%s isFullScan=%d\n", 
	  //	 cluster, proc, owner, isfullscan);  
	  if (jqSnapshot->startIterateAllClassAds(cluster, 
						  proc, 
						  owner, 
						  isfullscan) <= 0)
	    return NULL;
	  //time_t start;
	  //time(&start);
	  //dprintf(D_ALWAYS, "cur date/time before interation =  %s\n", ctime(&start));
	  
	}



	//------------
	// NOTICE!!!!
	//------------
	//
	// This part is naive...
	//
	// constraint could be applied as query when the result was 
	// retrieved. 
	//
	// For this, the revision is desirable....
	//

	//dprintf(D_ALWAYS, "called iterateAllClassAds\n");

	while(jqSnapshot->iterateAllClassAds(ad)) {

		if (*(key.value()) != '0' && // avoid cluster and header ads
			(!constraint || !constraint[0] || evalBool(ad, constraint))) {
			return ad;		      
		}
		
		freeJobAd(ad);
	}

	return NULL;
}


//! check the ad is valid under the constraint
/*
	This part should be improved.
	We can apply this constraint into SQL when the query is put.
*/
bool 
RequestService::evalBool(ClassAd *ad, const char *constraint)
{
    static ExprTree *tree = NULL;
    static char * saved_constraint = NULL;
    EvalResult result;
    bool constraint_changed = true;

    if ( saved_constraint ) {
        if ( strcmp(saved_constraint,constraint) == 0 ) {
            constraint_changed = false;
        }
    }

    if ( constraint_changed ) {
        // constraint has changed, or saved_constraint is NULL
        if ( saved_constraint ) {
            free(saved_constraint);
            saved_constraint = NULL;
        }
        if ( tree ) {
            delete tree;
            tree = NULL;
        }
        if (Parse(constraint, tree) != 0) {
            dprintf(D_ALWAYS,
                "can't parse constraint: %s\n", constraint);
            return false;
        }
        saved_constraint = strdup(constraint);
    }

    // Evaluate constraint with ad in the target scope so that constraints
    // have the same semantics as the collector queries.  --RR
    if (!tree->EvalTree(NULL, ad, &result)) {
        dprintf(D_ALWAYS, "can't evaluate constraint: %s\n", constraint);
        return false;
    }
    if (result.type == LX_INTEGER) {
        return (bool)result.i;
    }
    dprintf(D_ALWAYS, "contraint (%s) does not evaluate to bool\n",
        constraint);
    return false;
}




/*
	Currently this function is borrowed from qmgmt.C
	But, it's weird. There is no freeing stuff, but just assignment 
	of NULL value.
*/
void
RequestService::freeJobAd(ClassAd*& ad) 
{
	if (ad != NULL) {
	  ad->clear();
	  delete ad;
	}
	ad = NULL;
}

/*
	Here, nothing is done.
*/
int
RequestService::closeConnection()
{
/*
	JobbQueue->CommitTransaction();
        // If this failed, the schedd will EXCEPT.  So, if we got this
        // far, we can always return success.  -Derek Wright 4/2/99

    // Now that the transaction has been commited, we need to chain proc
    // ads to cluster ads if any new clusters have been submitted.
    if ( old_cluster_num != next_cluster_num ) {
        int cluster_id;
        int     *numOfProcs = NULL;
        int i;
        ClassAd *procad;
        ClassAd *clusterad;

        for ( cluster_id=old_cluster_num; cluster_id < next_cluster_num; cluster_id++ ) {
            if ( (JobQueue->LookupClassAd(IdToStr(cluster_id,-1), clusterad)) &&
                 (ClusterSizeHashTable->lookup(cluster_id,numOfProcs) != -1) )
            {
                for ( i = 0; i < *numOfProcs; i++ ) {
                    if (JobQueue->LookupClassAd(IdToStr(cluster_id,i),procad)) {
                        procad->ChainToAd(clusterad);
                    }
                }   // end of loop thru all proc in cluster cluster_id
            }
        }   // end of loop thru clusters
    }   // end of if a new cluster(s) submitted
    old_cluster_num = next_cluster_num;
*/
    return 0;
}
