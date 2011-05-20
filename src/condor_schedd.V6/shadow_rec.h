
// Record for a Shadow object.
// Note multiple shadow_rec's may point to a single process.

#ifndef _CONDOR_SHADOW_REC_H_
#define _CONDOR_SHADOW_REC_H_

#include "condor_daemon_core.h"
#include "proc.h"
#include "stream.h"

class match_rec;

struct shadow_rec
{
	//int		pid;
	PROC_ID		job_id;
	int		universe;
	match_rec*	match;
	int		preempted;
	int		conn_fd;
	int		removed;
	bool		isZombie;	// added for Maui by stolley
	bool		is_reconnect;
		//
		// This flag will cause the schedd to keep certain claim
		// attributes for jobs with leases during a graceful shutdown
		// This ensures that the job can reconnect when we come back up
		//
	bool		keepClaimAttributes;

	PROC_ID		prev_job_id;
	Stream*		recycle_shadow_stream;
	bool		exit_already_handled;

	shadow_rec();
	~shadow_rec();
};

typedef std::vector<shadow_rec*> ShadowRecVec;

#endif

