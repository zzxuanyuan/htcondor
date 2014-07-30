
#include "condor_common.h"

#include "compat_classad.h"
#include "condor_version.h"
#include "condor_attributes.h"
#include "file_transfer.h"
#include "directory.h"


#include "dc_cached.h"

DCCached::DCCached(const char * name, const char *pool)
	: Daemon( DT_CACHED, name, pool )
{}

int
DCCached::createCacheDir(std::string &cacheName, time_t &expiry, CondorError &err)
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

	std::string new_cacheName;
	time_t new_expiry;
	if (!ad.EvaluateAttrString("CacheName", new_cacheName) || !ad.EvaluateAttrInt("LeaseExpiration", new_expiry))
	{
		err.push("CACHED", 1, "Required attributes (CacheName and LeaseExpiration) not set in server response.");
		return 1;
	}
	cacheName = new_cacheName;
	expiry = new_expiry;
	return 0;
}


int
DCCached::uploadFiles(const std::string &cacheName, const std::list<std::string> files, CondorError &err)
{
	if (!_addr && !locate())
	{
		err.push("CACHED", 2, error() && error()[0] ? error() : "Failed to locate remote cached");
		return 2;
	}

	ReliSock *rsock = (ReliSock *)startCommand(
					CACHED_UPLOAD_FILES, Stream::reli_sock, 20 );


	if (!rsock)
	{
		err.push("CACHED", 1, "Failed to start command to remote cached");
		return 1;
	}

	filesize_t transfer_size = 0;
	for (std::list<std::string>::const_iterator it = files.begin(); it != files.end(); it++) {
		if (IsDirectory(it->c_str())) {
			Directory dir(it->c_str(), PRIV_USER);
			transfer_size += dir.GetDirectorySize();
		} else {
			StatInfo info(it->c_str());
			transfer_size += info.GetFileSize();
		}

	}
	
	dprintf(D_FULLDEBUG, "Transfer size = %lli\n", transfer_size);

	compat_classad::ClassAd ad;
	std::string version = CondorVersion();
	ad.InsertAttr(ATTR_DISK_USAGE, transfer_size);
	ad.InsertAttr("CondorVersion", version);
	ad.InsertAttr("CacheName", cacheName);

	if (!putClassAd(rsock, ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
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


	compat_classad::ClassAd transfer_ad;
	transfer_ad.InsertAttr("CondorVersion", version);

	// Expand the files list and add to the classad
	StringList inputFiles;
	for (std::list<std::string>::const_iterator it = files.begin(); it != files.end(); it++) {
		inputFiles.insert((*it).c_str());
	}
	char* filelist = inputFiles.print_to_string();
	dprintf(D_FULLDEBUG, "Transfer list = %s\n", filelist);
	transfer_ad.InsertAttr(ATTR_TRANSFER_INPUT_FILES, filelist);
	char current_dir[PATH_MAX];
	getcwd(current_dir, PATH_MAX);
	transfer_ad.InsertAttr(ATTR_JOB_IWD, current_dir);
	dprintf(D_FULLDEBUG, "IWD = %s\n", current_dir);
	free(filelist);

	// From here on out, this is the file transfer server socket.
	FileTransfer ft;
  rc = ft.SimpleInit(&transfer_ad, false, false, static_cast<ReliSock*>(rsock));
	if (!rc) {
		dprintf(D_ALWAYS, "Simple init failed\n");
		return 1;
	}
	ft.setPeerVersion(version.c_str());
	//UploadFilesHandler *handler = new UploadFilesHandler(*this, dirname);
	//ft.RegisterCallback(static_cast<FileTransferHandlerCpp>(&UploadFilesHandler::handle), handler);
	rc = ft.UploadFiles(true);

	if (!rc) {
		dprintf(D_ALWAYS, "Upload files failed.\n");
		return 1;
	}

	return 0;




}


int
DCCached::downloadFiles(const std::string &cacheName, const std::string dest, CondorError &err)
{
	if (!_addr && !locate())
	{
		err.push("CACHED", 2, error() && error()[0] ? error() : "Failed to locate remote cached");
		return 2;
	}

	ReliSock *rsock = (ReliSock *)startCommand(
					CACHED_DOWNLOAD_FILES, Stream::reli_sock, 20 );


	if (!rsock)
	{
		err.push("CACHED", 1, "Failed to start command to remote cached");
		return 1;
	}


	compat_classad::ClassAd ad;
	std::string version = CondorVersion();
	ad.InsertAttr("CondorVersion", version);
	ad.InsertAttr("CacheName", cacheName);

	if (!putClassAd(rsock, ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
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


	compat_classad::ClassAd transfer_ad;

	dprintf(D_FULLDEBUG, "Download Files Destination = %s\n", dest.c_str());
	transfer_ad.InsertAttr(ATTR_OUTPUT_DESTINATION, dest.c_str());
	char current_dir[PATH_MAX];
	getcwd(current_dir, PATH_MAX);
	transfer_ad.InsertAttr(ATTR_JOB_IWD, current_dir);
	dprintf(D_FULLDEBUG, "IWD = %s\n", current_dir);


	// From here on out, this is the file transfer server socket.
	FileTransfer ft;
	rc = ft.SimpleInit(&transfer_ad, false, true, static_cast<ReliSock*>(rsock));
	if (!rc) {
		dprintf(D_ALWAYS, "Simple init failed\n");
		return 1;
	}
	ft.setPeerVersion(version.c_str());
	rc = ft.DownloadFiles(true);

	if (!rc) {
		dprintf(D_ALWAYS, "Download files failed.\n");
		return 1;
	}

	return 0;




}


int 
DCCached::removeCacheDir(const std::string &cacheName, CondorError &err) {
	
	if (!_addr && !locate())
	{
		err.push("CACHED", 2, error() && error()[0] ? error() : "Failed to locate remote cached");
		return 2;
	}

	ReliSock *rsock = (ReliSock *)startCommand(
					CACHED_REMOVE_CACHE_DIR, Stream::reli_sock, 20 );


	if (!rsock)
	{
		err.push("CACHED", 1, "Failed to start command to remote cached");
		return 1;
	}


	compat_classad::ClassAd ad;
	std::string version = CondorVersion();
	ad.InsertAttr("CondorVersion", version);
	ad.InsertAttr("CacheName", cacheName);

	if (!putClassAd(rsock, ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
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

	return rc;
	
	
}



int DCCached::setReplicationPolicy(const std::string &cacheName, const std::string &policy, CondorError &err) {
	
	if (!_addr && !locate())
	{
		err.push("CACHED", 2, error() && error()[0] ? error() : "Failed to locate remote cached");
		return 2;
	}
	
	
	ReliSock *rsock = (ReliSock *)startCommand(
					CACHED_SET_REPLICATION_POLICY, Stream::reli_sock, 20 );
					
	if (!rsock)
	{
		err.push("CACHED", 1, "Failed to start command to remote cached");
		return 1;
	}

	compat_classad::ClassAd ad;
	std::string version = CondorVersion();
	ad.InsertAttr("CondorVersion", version);
	ad.InsertAttr("CacheName", cacheName);
	ad.InsertAttr("ReplicationPolicy", policy);
	
	if (!putClassAd(rsock, ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
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

	return rc;
	
	
}
