
#include "shadow_wrangler.h"

// TODO: To be eliminated
extern bool is_reconnect;

ShadowWrangler::ShadowWrangler() : 
	m_shadows(),
	m_sendUpdatesToSchedd(true)
{}

void
ShadowWrangler::config()
{
	ProcShadowMap::const_iterator it;
	for (it = m_shadows.begin(); it != m_shadows.end(); ++it) {
		it->second->config();
	}
}

void
ShadowWrangler::shutDown(int info)
{
	ProcShadowMap::const_iterator it;
	for (it = m_shadows.begin(); it != m_shadows.end(); ++it) {
		it->second->shutDown( info );
	}
}

void
ShadowWrangler::gracefulShutDown()
{
	ProcShadowMap::const_iterator it;
	for (it = m_shadows.begin(); it != m_shadows.end(); ++it) {
		it->second->gracefulShutDown();
	}
}

bool
inline
getJobID(ClassAd* job, PROC_ID& id) {
	if (!job) {
		return false;
	}
	if (job->LookupInteger(ATTR_CLUSTER_ID, id.cluster) == 0 ||
			job->LookupInteger(ATTR_PROC_ID, id.proc) == 0) {
		return false;
	}
	return true;
}

inline
BaseShadow*
ShadowWrangler::getOne()
{
	if (m_shadows.size() == 1) {
		return m_shadows.begin()->second;
	}
	return NULL;
}

BaseShadow*
ShadowWrangler::getShadow(ClassAd * job)
{
	PROC_ID id;
	if (!getJobID(job, id)) {
		return getOne();
	}
	return m_shadows[id];
}

void
ShadowWrangler::putShadow(ClassAd* job, BaseShadow* Shadow)
{
	PROC_ID id;
	if (!getJobID(job, id)) {
		return;
	}
	m_shadows[id] = Shadow;
}

void
ShadowWrangler::handleRemoveJob(Stream* stream)
{
	ReliSock* rsock = (ReliSock*)stream;

	ClassAd respad;

	CondorError errstack;
	if( ! rsock->triedAuthentication() ) {
		if( ! SecMan::authenticate_sock(rsock, WRITE, &errstack) ) {
			errstack.push( "SHADOW", SHADOW_ERR_REMOVE_FAILED,
			"Failure to remove job - Authentication failed" );
			dprintf( D_ALWAYS, "handleRemoveJob() aborting: %s\n",
			errstack.getFullText() );

			respad.Assign(ATTR_SHADOW_INVALID_REQUEST, TRUE);
			respad.Assign(ATTR_SHADOW_INVALID_REASON, "Authentication failed.");
			respad.put(*rsock);
			rsock->end_of_message();

			return;
		}
	}

	ClassAd reqad;

	rsock->decode();

	reqad.initFromStream(*rsock);
	rsock->end_of_message();

	int sig;
	if (reqad.LookupInteger(ATTR_SHADOW_SIGNAL, sig) == 0) {
		errstack.push( "SHADOW", SHADOW_ERR_REMOVE_FAILED,
		"Failure to remove job - missing signal" );
		dprintf( D_ALWAYS, "handleRemoveJob() aborting: %s\n",
		errstack.getFullText() );

		respad.Assign(ATTR_SHADOW_INVALID_REQUEST, TRUE);
		respad.Assign(ATTR_SHADOW_INVALID_REASON, "Missing signal.");
		respad.put(*rsock);
		rsock->end_of_message();

		return;
	}
	BaseShadow* Shadow = getShadow(&reqad);

	int result = 0;
	if (Shadow) {
		result = Shadow->handleJobRemoval(sig);
	} else {
		errstack.push( "SHADOW", SHADOW_ERR_REMOVE_FAILED,
		"Failure to remove job - unknown shadow" );
		dprintf( D_ALWAYS, "handleRemoveJob() aborting: %s\n",
		errstack.getFullText() );

		respad.Assign(ATTR_SHADOW_INVALID_REQUEST, TRUE);
		respad.Assign(ATTR_SHADOW_INVALID_REASON, "Unknown shadow.");
		respad.put(*rsock);
		rsock->end_of_message();
		return;
	}

	respad.Assign(ATTR_SHADOW_INVALID_REQUEST, FALSE);
	respad.Assign(ATTR_SHADOW_RESULT, result);
	respad.put(*rsock);
	rsock->end_of_message();
	return;
	
}

void
ShadowWrangler::handleCreateJob(Stream* stream)
{

	ReliSock* rsock = (ReliSock*)stream;

	ClassAd respad;

	CondorError errstack;
	if( ! rsock->triedAuthentication() ) {
		if( ! SecMan::authenticate_sock(rsock, WRITE, &errstack) ) {
			errstack.push( "SHADOW", SHADOW_ERR_CREATE_FAILED,
			"Failure to create job - Authentication failed" );
			dprintf( D_ALWAYS, "handleCreateJob() aborting: %s\n",
			errstack.getFullText() );

			respad.Assign(ATTR_SHADOW_INVALID_REQUEST, TRUE);
			respad.Assign(ATTR_SHADOW_INVALID_REASON, "Authentication failed.");
			respad.put(*rsock);
			rsock->end_of_message();

			return;
		}
	}

	ClassAd reqad;

	rsock->decode();

	reqad.initFromStream(*rsock);
	rsock->end_of_message();

	// TODO: Sanity check: verify owner of this shadow is the owner of the job
	// (Probably should do the same thing in recycleJob)
	PROC_ID id;
	reqad.LookupInteger(ATTR_CLUSTER_ID, id.cluster);
	reqad.LookupInteger(ATTR_PROC_ID, id.proc);
	dprintf(D_ALWAYS,"Starting new job %d.%d\n", id.cluster, id.proc);
	
	// TODO: Handle the reconnect case!
	is_reconnect = false;

	// TODO: needs to be made asynchronous!
	startShadow(&reqad);

	respad.Assign(ATTR_SHADOW_INVALID_REQUEST, FALSE);
	respad.Assign(ATTR_SHADOW_RESULT, 0);
	respad.put(*rsock);
	rsock->end_of_message();

}

void
ShadowWrangler::handleUpdateJob(Stream* stream)
{
	ReliSock* rsock = (ReliSock*)stream;

	ClassAd respad;

	CondorError errstack;
	if( ! rsock->triedAuthentication() ) {
		if( ! SecMan::authenticate_sock(rsock, WRITE, &errstack) ) {
			errstack.push( "SHADOW", SHADOW_ERR_UPDATE_FAILED,
			"Failure to update job - Authentication failed" );
			dprintf( D_ALWAYS, "handleUpdateJob() aborting: %s\n",
			errstack.getFullText() );

			respad.Assign(ATTR_SHADOW_INVALID_REQUEST, TRUE);
			respad.Assign(ATTR_SHADOW_INVALID_REASON, "Authentication failed.");
			respad.put(*rsock);
			rsock->end_of_message();

			return;
		}
	}

	ClassAd reqad;

	rsock->decode();

	reqad.initFromStream(*rsock);
	rsock->end_of_message();

	int sig;
	if (reqad.LookupInteger(ATTR_SHADOW_SIGNAL, sig) == 0) {
		errstack.push( "SHADOW", SHADOW_ERR_UPDATE_FAILED,
		"Failure to update job - missing signal" );
		dprintf( D_ALWAYS, "handleUpdateJob() aborting: %s\n",
		errstack.getFullText() );

		respad.Assign(ATTR_SHADOW_INVALID_REQUEST, TRUE);
		respad.Assign(ATTR_SHADOW_INVALID_REASON, "Missing signal.");
		respad.put(*rsock);
		rsock->end_of_message();

		return;
	}
	BaseShadow* Shadow = getShadow(&reqad);

	int result = 0;
	if (Shadow) {
		result = Shadow->handleUpdateJobAd(sig);
	} else {
		errstack.push( "SHADOW", SHADOW_ERR_REMOVE_FAILED,
		"Failure to update job - unknown shadow" );
		dprintf( D_ALWAYS, "handleUpdateJob() aborting: %s\n",
		errstack.getFullText() );

		respad.Assign(ATTR_SHADOW_INVALID_REQUEST, TRUE);
		respad.Assign(ATTR_SHADOW_INVALID_REASON, "Unknown shadow.");
		respad.put(*rsock);
		rsock->end_of_message();
		return;
	}

	respad.Assign(ATTR_SHADOW_INVALID_REQUEST, FALSE);
	respad.Assign(ATTR_SHADOW_RESULT, result);
	respad.put(*rsock);
	rsock->end_of_message();
	return;

}

