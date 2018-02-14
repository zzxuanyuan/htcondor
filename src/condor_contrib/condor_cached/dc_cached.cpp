
#include "condor_common.h"

#include "compat_classad.h"
#include "condor_version.h"
#include "condor_attributes.h"
#include "file_transfer.h"
#include "directory.h"
#include <iostream>
#include <fstream>


#include "dc_cached.h"

DCCached::DCCached(const char * name, const char *pool)
	: Daemon( DT_CACHED, name, pool )
{}
	
DCCached::DCCached(const ClassAd* ad, const char* pool)
	: Daemon(ad, DT_GENERIC, pool )
{}

int
DCCached::createCacheDir(std::string &cacheName, time_t &expiry, CondorError &err)
{
	dprintf(D_FULLDEBUG, "FULLDEBUG: In DCCached::createCacheDir!");//##
	dprintf(D_ALWAYS, "ALWAYS: In DCCached::createCacheDir!");//##
	dprintf(D_FAILURE, "FAILURE: In DCCached::createCacheDir!");//##
	printf("1 In DCCached::createCacheDir!\n");//##

	if (!_addr && !locate())
	{
		err.push("CACHED", 2, error() && error()[0] ? error() : "Failed to locate remote cached");
		return 2;
	}
	printf("2 In DCCached::createCacheDir!\n");//##
        ReliSock *rsock = (ReliSock *)startCommand(
                CACHED_CREATE_CACHE_DIR, Stream::reli_sock, 20 );
	if (!rsock)
	{
		err.push("CACHED", 1, "Failed to start command to remote cached");
		return 1;
	}
	printf("3 In DCCached::createCacheDir!\n");//##

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
	printf("4 In DCCached::createCacheDir!\n");//##

	ad.Clear();
	
	rsock->decode();
	if (!getClassAd(rsock, ad) || !rsock->end_of_message())
	{
		delete rsock;
		err.push("CACHED", 1, "Failed to get response from remote condor_cached");
		return 1;
	}
	printf("5 In DCCached::createCacheDir!\n");//##

	rsock->close();
	delete rsock;

	int rc;
	if (!ad.EvaluateAttrInt(ATTR_ERROR_CODE, rc))
	{
		err.push("CACHED", 2, "Remote condor_cached did not return error code");
	}
	printf("6 In DCCached::createCacheDir!\n");//##

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
	printf("7 In DCCached::createCacheDir!\n");//##

	std::string new_cacheName;
	time_t new_expiry;
	if (!ad.EvaluateAttrString("CacheName", new_cacheName) || !ad.EvaluateAttrInt("LeaseExpiration", new_expiry))
	{
		err.push("CACHED", 1, "Required attributes (CacheName and LeaseExpiration) not set in server response.");
		return 1;
	}
	printf("8 In DCCached::createCacheDir!\n");//##
	cacheName = new_cacheName;
	expiry = new_expiry;
	return 0;
}


int
DCCached::uploadFiles(const std::string &cacheName, const std::list<std::string> files, CondorError &err)
{
	dprintf(D_ALWAYS, "1 In DCCached::uploadFiles!\n");//##
	if (!_addr && !locate())
	{
		err.push("CACHED", 2, error() && error()[0] ? error() : "Failed to locate remote cached");
		return 2;
	}
	dprintf(D_ALWAYS, "2 In DCCached::uploadFiles!\n");//##

	ReliSock *rsock = (ReliSock *)startCommand(
					CACHED_UPLOAD_FILES, Stream::reli_sock, 20 );


	if (!rsock)
	{
		err.push("CACHED", 1, "Failed to start command to remote cached");
		delete rsock;
		return 1;
	}
	dprintf(D_ALWAYS, "3 In DCCached::uploadFiles!\n");//##

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
	ad.InsertAttr(ATTR_CACHE_NAME, cacheName);

	dprintf(D_ALWAYS, "4 In DCCached::uploadFiles!\n");//##
	if (!putClassAd(rsock, ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		delete rsock;
		return 1;
	}
	dprintf(D_ALWAYS, "5 In DCCached::uploadFiles!\n");//##

	ad.Clear();
	rsock->decode();
	if (!getClassAd(rsock, ad) || !rsock->end_of_message())
	{
		delete rsock;
		err.push("CACHED", 1, "Failed to get response from remote condor_cached");
		return 1;
	}

	dprintf(D_ALWAYS, "6 In DCCached::uploadFiles!\n");//##
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
		delete rsock;
		return rc;
	}

	dprintf(D_ALWAYS, "7 In DCCached::uploadFiles!\n");//##

	compat_classad::ClassAd transfer_ad;
	transfer_ad.InsertAttr("CondorVersion", version);

	// Expand the files list and add to the classad
	StringList inputFiles;
	for (std::list<std::string>::const_iterator it = files.begin(); it != files.end(); it++) {
		inputFiles.insert((*it).c_str());
	}
	char* filelist = inputFiles.print_to_string();
	dprintf(D_FULLDEBUG, "Transfer list = %s\n", filelist);
	dprintf(D_ALWAYS, "Transfer list = %s\n", filelist);//##
	transfer_ad.InsertAttr(ATTR_TRANSFER_INPUT_FILES, filelist);
	char current_dir[PATH_MAX];
	getcwd(current_dir, PATH_MAX);
	transfer_ad.InsertAttr(ATTR_JOB_IWD, current_dir);
	dprintf(D_FULLDEBUG, "IWD = %s\n", current_dir);
	free(filelist);

	dprintf(D_ALWAYS, "8 In DCCached::uploadFiles!\n");//##
	// From here on out, this is the file transfer server socket.
	FileTransfer ft;
  	rc = ft.SimpleInit(&transfer_ad, false, false, static_cast<ReliSock*>(rsock));
	dprintf(D_ALWAYS, "9 rc = %d\n", rc);//##
	if (!rc) {
		dprintf(D_ALWAYS, "Simple init failed\n");
		delete rsock;
		return 1;
	}
	dprintf(D_ALWAYS, "9 In DCCached::uploadFiles!\n");//##
	ft.setPeerVersion(version.c_str());
	//UploadFilesHandler *handler = new UploadFilesHandler(*this, dirname);
	//ft.RegisterCallback(static_cast<FileTransferHandlerCpp>(&UploadFilesHandler::handle), handler);
	rc = ft.UploadFiles(true);

	if (!rc) {
		delete rsock;
		dprintf(D_ALWAYS, "Upload files failed.\n");
		return 1;
	}
	
	dprintf(D_ALWAYS, "10 In DCCached::uploadFiles!\n");//##
	delete rsock;
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
	
	
	std::auto_ptr<ReliSock> rsock((ReliSock *)startCommand(
					CACHED_DOWNLOAD_FILES, Stream::reli_sock, 20 ));


	if (!rsock.get())
	{
		err.push("CACHED", 1, "Failed to start command to remote cached");
		return 1;
	}
	
	compat_classad::ClassAd ad;
	std::string version = CondorVersion();
	ad.InsertAttr("CondorVersion", version);
	ad.InsertAttr(ATTR_CACHE_NAME, cacheName);
	
	
	// If the cached is local (ie, same pilot), then suggest the HARDLINK transfer
	// method.
	if (isLocal()) {
		ad.InsertAttr(ATTR_CACHE_REPLICATION_METHODS, "HARDLINK, DIRECT");
	}


	if (!putClassAd(rsock.get(), ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		return 1;
	}

	ad.Clear();
	rsock->decode();
	if (!getClassAd(rsock.get(), ad) || !rsock->end_of_message())
	{
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
	
	std::string selected_transfer_method;
	if (ad.EvalString(ATTR_CACHE_REPLICATION_METHODS, NULL, selected_transfer_method)) {
		
		if (selected_transfer_method == "HARDLINK") {
			
			return DoHardlinkTransfer(cacheName, dest, rsock.get(), err);
		}
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
	rc = ft.SimpleInit(&transfer_ad, false, true, static_cast<ReliSock*>(rsock.get()));
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
DCCached::DoHardlinkTransfer(const std::string cacheName, const std::string dest, ReliSock* rsock, CondorError& err) 
{
	
	/** 
		* The protocol is as follows:
		* 1. Client sends a directory for the server to save a hardlink
		* 2. Server creates hardlink file with mkstemp in directory from 1, and sends file name to client.
		* 3. Client acknowledges creation, renames hardlink to dest from client.
		*/
		
	compat_classad::ClassAd ad;
	
	rsock->encode();
	
	ad.InsertAttr(ATTR_JOB_IWD, dest);
	if (!putClassAd(rsock, ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		return 1;
	}
	
	ad.Clear();
	rsock->decode();
	if (!getClassAd(rsock, ad) || !rsock->end_of_message())
	{
		err.push("CACHED", 1, "Failed to get response from remote condor_cached");
		return 1;
	}
	
	std::string dest_file;
	ad.EvalString(ATTR_FILE_NAME, NULL, dest_file);
	std::string new_dest = dest + cacheName;
	if (rename(dest_file.c_str(), new_dest.c_str())) 
	{
		err.pushf("CACHED", 1, "Failed to rename file %s to %s: %s", dest_file.c_str(), new_dest.c_str(), strerror(errno));
		return 1;
		
	}
	
	
	// Send back an ACK that we have moved the file
	rsock->encode();
	ad.InsertAttr(ATTR_ERROR_CODE, 0);
	if (!putClassAd(rsock, ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
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
	ad.InsertAttr(ATTR_CACHE_NAME, cacheName);

	if (!putClassAd(rsock, ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		delete rsock;
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
	}
	
	delete rsock;
	return rc;
	
	
}



int DCCached::setReplicationPolicy(const std::string &cacheName, const std::string &policy, const std::string &methods, CondorError &err) {
	
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
	ad.InsertAttr(ATTR_CACHE_NAME, cacheName);
	
	if (!policy.empty()) {
		ad.InsertAttr(ATTR_CACHE_REPLICATION_POLICY, policy);
	}
	
	if (!methods.empty()) {
		ad.InsertAttr(ATTR_CACHE_REPLICATION_METHODS, methods);
	}
	
	
	
	if (!putClassAd(rsock, ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		delete rsock;
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
	}

	delete rsock;
	return rc;
	
	
}

int DCCached::listCacheDirs(const std::string &cacheName, const std::string& requirements, std::list<compat_classad::ClassAd>& result_list, CondorError& err) {
	
	if (!_addr && !locate())
	{
		err.push("CACHED", 2, error() && error()[0] ? error() : "Failed to locate remote cached");
		return 2;
	}
	
	ReliSock *rsock = (ReliSock *)startCommand(
	CACHED_LIST_CACHE_DIRS, Stream::reli_sock, 20 );
	
	if (!rsock)
	{
		err.push("CACHED", 1, "Failed to start command to remote cached");
		return 1;
	}
	

	
	compat_classad::ClassAd request_ad;
	std::string version = CondorVersion();
	request_ad.InsertAttr("CondorVersion", version);
	
	if (!cacheName.empty()) {
		request_ad.InsertAttr(ATTR_CACHE_NAME, cacheName);
	} 
	if (!requirements.empty()) {
		request_ad.InsertAttr(ATTR_REQUIREMENTS, requirements);
	}
	if (cacheName.empty() && requirements.empty()) {
		request_ad.InsertAttr(ATTR_REQUIREMENTS, "TRUE");
	}
	
	
	if (!putClassAd(rsock, request_ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		delete rsock;
		return 1;
	}
	
	request_ad.Clear();
	
	// Now get all the replies.
	rsock->decode();
	
	while(true) {
		if (!getClassAd(rsock, request_ad) || !rsock->end_of_message())
		{
			err.push("CACHED", 2, "Request to remote cached closed before completing protocol.");
			delete rsock;
			return 2;
		}
		
		int is_end = 0;
		
		if(request_ad.LookupBool("FinalAd", is_end)) {
			if (is_end)
				break;
		}
		
		result_list.push_back(request_ad);
		
	}
	
	delete rsock;
	return 0;
	
	
}

int DCCached::listCacheDs(const std::string& requirements, std::list<compat_classad::ClassAd>& result_list, CondorError& err) {

	if (!_addr && !locate())
	{
		err.push("CACHED", 2, error() && error()[0] ? error() : "Failed to locate remote cached");
		return 2;
	}
	
	ReliSock *rsock = (ReliSock *)startCommand(
	CACHED_LIST_CACHEDS, Stream::reli_sock, 20 );
	
	if (!rsock)
	{
		err.push("CACHED", 1, "Failed to start command to remote cached");
		return 1;
	}
	
	compat_classad::ClassAd request_ad;
	std::string version = CondorVersion();
	request_ad.InsertAttr("CondorVersion", version);
	
	if (!requirements.empty()) {
		request_ad.InsertAttr(ATTR_REQUIREMENTS, requirements);
	} else {
		request_ad.InsertAttr(ATTR_REQUIREMENTS, "TRUE");
	}
	
	
	if (!putClassAd(rsock, request_ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		delete rsock;
		return 1;
	}
	
	request_ad.Clear();
	
	// Now get all the replies.
	rsock->decode();
	
	while(true) {
		if (!getClassAd(rsock, request_ad) || !rsock->end_of_message())
		{
			err.push("CACHED", 2, "Request to remote cached closed before completing protocol.");
			delete rsock;
			return 2;
		}
		
		int is_end = 0;
		
		if(request_ad.LookupBool("FinalAd", is_end)) {
			if (is_end)
				break;
		}
		
		result_list.push_back(request_ad);
		fPrintAd (stdout, request_ad);
	}
	
	delete rsock;
	return 0;

}

/**
	*	Mostly non-blocking version of request local cache.  The protocol states
	* that the cached will return as soon as possible a classad saying something...
	*
	*/

int DCCached::requestLocalCache(const std::string &cached_server, const std::string &cached_name, compat_classad::ClassAd& response, CondorError& err) 
{
	
	if (!_addr && !locate())
	{
		err.push("CACHED", 2, error() && error()[0] ? error() : "Failed to locate remote cached");
		return 2;
	}
	
	ReliSock *rsock = (ReliSock *)startCommand(
	CACHED_REQUEST_LOCAL_REPLICATION, Stream::reli_sock, 20 );
	
	if (!rsock)
	{
		err.push("CACHED", 1, "Failed to start command to remote cached");
		return 1;
	}
	
	compat_classad::ClassAd request_ad;
	std::string version = CondorVersion();
	request_ad.InsertAttr("CondorVersion", version);
	request_ad.InsertAttr(ATTR_CACHE_ORIGINATOR_HOST, cached_server);
	request_ad.InsertAttr(ATTR_CACHE_NAME, cached_name);
	
	if (!putClassAd(rsock, request_ad) || !rsock->end_of_message())
	{
		// Can't send another response!  Must just hang-up.
		delete rsock;
		return 1;
	}
	
	rsock->decode();
	
	// We should get a response now
	if (!getClassAd(rsock, response) || !rsock->end_of_message())
	{
		delete rsock;
		err.push("CACHED", 1, "Failed to get response from remote condor_cached");
		return 1;
	}
	
	int rc = 0;
	if (!response.EvaluateAttrInt(ATTR_ERROR_CODE, rc))
	{
		err.push("CACHED", 2, "Remote condor_cached did not return error code");
	}
	
	if (rc)
	{
		std::string error_string;
		if (!response.EvaluateAttrString(ATTR_ERROR_STRING, error_string))
		{
			err.push("CACHED", rc, "Unknown error from remote condor_cached");
		}
		else
		{
			err.push("CACHED", rc, error_string.c_str());
		}
	}
	
	delete rsock;
	return rc;
	
	
}

int
DCCached::encodeDir(const std::string &server, const std::string &directory, const int data, const int parity, std::string &codeTech, const int w, const int packetsize, const int buffersize, CondorError &err)
{
	dprintf(D_ALWAYS, "In DCCached::encodeDir, getting into this function!!!\n");//##
	if (!_addr && !locate())
	{
		err.push("CACHED", 2, error() && error()[0] ? error() : "Failed to locate remote cached");
		return 2;
	}
	dprintf(D_ALWAYS, "In DCCached::encodeDir 1\n");//##
	ReliSock *rsock = (ReliSock *)startCommand(
			CACHED_ENCODE_DIR, Stream::reli_sock, 20 );
	if (!rsock)
	{
		err.push("CACHED", 1, "Failed to start command to remote cached");
		return 1;
	}
	dprintf(D_ALWAYS, "In DCCached::encodeDir 2\n");//##

	compat_classad::ClassAd ad;
	std::string version = CondorVersion();
	ad.InsertAttr("CondorVersion", version);
	ad.InsertAttr("EncodeServer", server);
	ad.InsertAttr("EncodeDir", directory);
	ad.InsertAttr("EncodeDataNum", data);
	ad.InsertAttr("EncodeParityNum", parity);
	ad.InsertAttr("EncodeCodeTech", codeTech);
	ad.InsertAttr("EncodeFieldSize", w);
	ad.InsertAttr("EncodePacketSize", packetsize);
	ad.InsertAttr("EncodeBufferSize", buffersize);

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

	std::string return_string;
	if (!ad.EvaluateAttrString("EncodeDirState", return_string))
	{
		err.push("CACHED", 2, "Remote condor_cached did not return error code");
	}

	dprintf(D_ALWAYS, "EncodeDirState = %s\n", return_string.c_str());//##
	if (return_string == "SUCCEEDED")
		return 0;
	else
		return 1;
}

int
DCCached::encodeFile(const std::string &server, const std::string &directory, const std::string &file, const int data, const int parity, std::string &codeTech, const int w, const int packetsize, const int buffersize, CondorError &err)
{
	if (!_addr && !locate())
	{
		err.push("CACHED", 2, error() && error()[0] ? error() : "Failed to locate remote cached");
		return 2;
	}

	ReliSock *rsock = (ReliSock *)startCommand(
		CACHED_ENCODE_FILE, Stream::reli_sock, 20 );
	if (!rsock)
	{
		err.push("CACHED", 1, "Failed to start command to remote cached");
		return 1;
	}

	compat_classad::ClassAd ad;
	std::string version = CondorVersion();
	ad.InsertAttr("CondorVersion", version);
	ad.InsertAttr("EncodeServer", server);
	ad.InsertAttr("EncodeDir", directory);
	ad.InsertAttr("EncodeFile", file);
	ad.InsertAttr("EncodeDataNum", data);
	ad.InsertAttr("EncodeParityNum", parity);
	ad.InsertAttr("EncodeCodeTech", codeTech);
	ad.InsertAttr("EncodeFieldSize", w);
	ad.InsertAttr("EncodePacketSize", packetsize);
	ad.InsertAttr("EncodeBufferSize", buffersize);

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

        std::string return_string;
        if (!ad.EvaluateAttrString("EncodeFileState", return_string))
        {
                err.push("CACHED", 2, "Remote condor_cached did not return error code");
        }

        dprintf(D_ALWAYS, "EncodeFileState = %s\n", return_string.c_str());//##
        if (return_string == "SUCCEEDED")
                return 0;
        else
                return 1;
}

int
DCCached::decodeFile(const std::string &server, const std::string &directory, const std::string &file, CondorError &err)
{
	dprintf(D_ALWAYS, "In DCCached::encodeDir, getting into this function!!!\n");//##
	if (!_addr && !locate())
	{
		err.push("CACHED", 2, error() && error()[0] ? error() : "Failed to locate remote cached");
		return 2;
	}
	dprintf(D_ALWAYS, "In DCCached::encodeDir 1\n");//##
	ReliSock *rsock = (ReliSock *)startCommand(
		CACHED_DECODE_FILE, Stream::reli_sock, 20 );
	if (!rsock)
	{
		err.push("CACHED", 1, "Failed to start command to remote cached");
		return 1;
	}
	dprintf(D_ALWAYS, "In DCCached::encodeDir 2\n");//##

	compat_classad::ClassAd ad;
	std::string version = CondorVersion();
	ad.InsertAttr("CondorVersion", version);
	ad.InsertAttr("DecodeServer", server);
	ad.InsertAttr("DecodeDir", directory);
	ad.InsertAttr("DecodeFile", file);

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

        std::string return_string;
        if (!ad.EvaluateAttrString("DecodeState", return_string))
        {
                err.push("CACHED", 2, "Remote condor_cached did not return error code");
        }

        dprintf(D_ALWAYS, "DecodeState = %s\n",return_string.c_str());//##
        if (return_string == "SUCCEEDED")
                return 0;
        else
                return 1;
}

/**
 *	Mostly non-blocking version of distributing encoded files.  The protocol states
 * that the cached will return as soon as possible a classad saying something...
 *
 */

int DCCached::distributeEncodedFiles(const std::string &cached_server, const std::string &cached_name, std::vector<std::string>& transfer_files, compat_classad::ClassAd& response, CondorError& err) 
{

	if (!_addr && !locate())
	{
		err.push("CACHED", 2, error() && error()[0] ? error() : "Failed to locate remote cached");
		return 2;
	}

	ReliSock *rsock = (ReliSock *)startCommand(CACHED_DISTRIBUTE_ENCODED_FILES, Stream::reli_sock, 20 );


	if (!rsock)
	{
		err.push("CACHED", 1, "Failed to start command to remote cached");
		return 1;
	}

	compat_classad::ClassAd request_ad;
	std::string version = CondorVersion();
	request_ad.InsertAttr("CondorVersion", version);
	request_ad.InsertAttr("CachedServerName", cached_server);
	request_ad.InsertAttr("CacheName", cached_name);

	if (!putClassAd(rsock, request_ad) || !rsock->end_of_message())
	{
		dprintf(D_ALWAYS, "Can't send another response!!!!\n");//##
		// Can't send another response!  Must just hang-up.
		delete rsock;
		return 1;
	}
	dprintf(D_ALWAYS, "distributeEncodedFiles and before decode\n");//##
	rsock->decode();

	// We should get a response now
	if (!getClassAd(rsock, response) || !rsock->end_of_message())
	{
		dprintf(D_ALWAYS, "Failed to get remote response!!!!\n");//##
		delete rsock;
		err.push("CACHED", 1, "Failed to get response from remote condor_cached");
		return 1;
	}
	dprintf(D_ALWAYS, "distributeEncodedFiles and get response\n");//##

	int rc = 0;
	if (!response.EvaluateAttrInt(ATTR_ERROR_CODE, rc))
	{
		dprintf(D_ALWAYS, "Remote condor_cached did not return error code!!!!\n");//##
		err.push("CACHED", 2, "Remote condor_cached did not return error code");
	}
	dprintf(D_ALWAYS, "got rc=%d!!!\n",rc);//##

	if (rc)
	{
		std::string error_string;
		if (!response.EvaluateAttrString(ATTR_ERROR_STRING, error_string))
		{
			err.push("CACHED", rc, "Unknown error from remote condor_cached");
		}
		else
		{
			err.push("CACHED", rc, error_string.c_str());
		}
	}

	compat_classad::ClassAd transfer_ad;

	std::string trans_file = transfer_files[0];
	transfer_ad.InsertAttr(ATTR_TRANSFER_INPUT_FILES, trans_file.c_str());
	transfer_ad.InsertAttr(ATTR_JOB_IWD, trans_file.c_str());

	FileTransfer* ft = new FileTransfer();
	ft->SimpleInit(&transfer_ad, false, false, static_cast<ReliSock*>(rsock));
	ft->setPeerVersion(version.c_str());
	ft->UploadFiles(true);

	return rc;	
}
