
/**
 *  The ShadowWrangler is responsible for handling the DaemonCore communication
 *  for the various Shadow objects and Shadow lifecycle management.
 *  
 */

#include "baseshadow.h"
#include "proc.h"

#include <ext/hash_map>
namespace ext = __gnu_cxx;

struct eqproc
{
	bool operator()(const PROC_ID& p1, const PROC_ID& p2) const
	{
		return (p1.cluster == p2.cluster) && (p1.proc == p2.proc);
	}
};

struct hashClassPROC_ID
{
	unsigned int operator()(const PROC_ID& p) const
	{
		return hashFuncPROC_ID(p);
	}
};

typedef ext::hash_map<PROC_ID, BaseShadow*, hashClassPROC_ID, eqproc> ProcShadowMap;

class ShadowWrangler : public Service
{

public:

	ShadowWrangler();

	// Retrieve a shadow this wrangler manages
	// Which shadow to get is determined from the ClassAd
	// Returns NULL if no matching shadow found.
	BaseShadow* getShadow(ClassAd *);

	// Start managing a given shadow.
	// ClassAd is used to make a hash for the job.
	void putShadow(ClassAd*, BaseShadow*);

	// Initialize a Shadow given a ClassAd
	void initShadow(ClassAd*);

	// Start a Shadow, given a ClassAd
	void startShadow(ClassAd*);

	// Shutdown all the shadows we are wrangling
	void shutDown(int);
	void gracefulShutDown();

	void config();

	bool recycleShadow(BaseShadow*, int);

	// Commands from the schedd
	void handleUpdateJob(Stream*);
	void handleCreateJob(Stream*);
	void handleRemoveJob(Stream*);

	void setSendUpdatesToSchedd(bool shallWe) { m_sendUpdatesToSchedd = shallWe;}
	bool const getSendUpdatesToSchedd() {return m_sendUpdatesToSchedd;}

private:

	// All the shadows this wrangler is responsible for.
	// Hashed on the ClusterId.ProcId
	ProcShadowMap m_shadows;

	bool m_sendUpdatesToSchedd;

	// Return the shadow if there's only one shadow
	// If more than one shadow, return NULL;
	inline BaseShadow* getOne();
};
