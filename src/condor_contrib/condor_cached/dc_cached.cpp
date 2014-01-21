
#include "condor_common.h"

#include "compat_classad.h"
#include "condor_version.h"
#include "condor_attributes.h"

#include "dc_cached.h"

DCCached::DCCached(const char * name, const char *pool)
	: Daemon( DT_CACHED, name, pool )
{}

int
DCCached::createCacheDir(const std::string &cacheName, time_t expiry, CondorError &err)
{
	if (!_addr && !locate())
	{
		err.push("CACHED", 2, error() && error()[0] ? error() : "Failed to locate remote cached");
		return 2;
	}

        ReliSock *rsock = (ReliSock *)startCommand(
                CACHED_CREATE_CACHE_DIR, Stream::reli_sock, 20 );
	if (!rsock)
	{
		err.push("CACHED", 1, "Failed to start command to remote cached");
		return 1;
	}

	compat_classad::ClassAd ad;
	std::string version = CondorVersion();
	ad.InsertAttr("CondorVersion", version);
	ad.InsertAttr("LeaseExpiration", expiry);
	ad.InsertAttr("CacheName", cacheName);

	if (!putClassAd(rsock, ad) || !rsock->end_of_message())
	{
		delete rsock;
		err.push("CACHED", 1, "Failed to send request to remote condor_cached");
		return 1;
	}

	ad.Clear();
	rsock->decode();
	if (!getClassAd(rsock, ad) || !rsock->end_of_message())
	{
		delete rsock;
		err.push("CACHED", 1, "Failed to get response from remote condor_cached");
		return 1;
	}

	rsock->close();
	delete rsock;

	int rc;
	if (!ad.EvaluateAttrInt(ATTR_ERROR_CODE, rc))
	{
		err.push("CACHED", 2, "Remote condor_cached did not return error code");
	}

	if (rc)
	{
		std::string error_string;
		if (!ad.EvaluateAttrString(ATTR_ERROR_STRING, error_string))
		{
			err.push("CACHED", rc, "Unknown error from remote condor_cached");
		}
		else
		{
			err.push("CACHED", rc, error_string.c_str());
		}
		return rc;
	}
	return 0;
}

