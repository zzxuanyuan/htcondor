/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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

// oh, the hacks! this is needed since otherwise our macro-izing of
// open interferes with the <sstream> include below
//
#define _CONDOR_ALLOW_OPEN

#include "condor_common.h"

#include "ickpt_share.h"

#include <iomanip>
#include <sstream>

#include "condor_attributes.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "MyString.h"

extern char* Spool;

#if defined(WIN32)
// a wrapper for CreateHardLink() on Windows that looks like link()
// for the code in this module
//
static int
ickpt_share_link(const char* oldpath, const char* newpath)
{
	if (CreateHardLink(newpath, oldpath, NULL)) {
		return 0;
	}
	else {
		DWORD error = GetLastError();
		if (error == ERROR_FILE_NOT_FOUND) {
			errno = ENOENT;
		}
		else {
			// for anything but the ENOENT-equivalent, we'll
			// log the error and set errno to something generic-
			// sounding ("operation not permitted")
			dprintf(D_ALWAYS,
			        "CreateHardLink error: %u\n",
			        (unsigned)error);
			errno = EPERM;
		}
		return -1;
	}
}

// stat on Windows  seems to always return 1 for link count,
// so we need to provide our own (that only sets link
// count, since that's all the code below cares about)
//
static int
ickpt_share_stat(const char* path, struct _fixed_windows_stat* st)
{
	HANDLE handle = CreateFile(path,
	                           GENERIC_READ,
	                           FILE_SHARE_READ |
	                               FILE_SHARE_WRITE |
	                               FILE_SHARE_DELETE,
	                           NULL,
	                           OPEN_EXISTING,
	                           0,
	                           NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		dprintf(D_ALWAYS,
		        "ickpt_share: CreateFile error: %u\n",
		        (unsigned)GetLastError());
		errno = EPERM;
		return -1;
	}
	BY_HANDLE_FILE_INFORMATION bhfi;
	BOOL ret = GetFileInformationByHandle(handle, &bhfi);
	DWORD err = GetLastError();
	CloseHandle(handle);
	if (!ret) {
		dprintf(D_ALWAYS,
		        "ickpt_share: GetFileInformationByHandle error: %u\n",
		        (unsigned)err);
		errno = EPERM;
		return -1;
	}
	st->st_nlink = (short)bhfi.nNumberOfLinks;
	return 0;
}
#else
// UNIX: just use regular old link(2) and stat(2)
//
#define ickpt_share_link link
#define ickpt_share_stat stat
#endif

// escape a string to that it is acceptable for use as a filename. any
// character not in the regex [a-zA-Z._] is replaced with %AA, where AA
// is the character's two-hex-digit equivalent
//
static std::string
escape_for_filename(const std::string& s)
{
	std::ostringstream out;
	out << std::hex << std::setfill('0');
	for (std::string::const_iterator i = s.begin(); i != s.end(); i++) {
		if (isalnum(*i) || (*i == '_') || (*i == '.')) {
			out << *i;
		}
		else {
			out << '%' << std::setw(2) << static_cast<int>(*i);
		}
	}
	return out.str();
}

// given an owner and an executable's hash string, construct the filename
// used to hold the shared ickpt file
//
static std::string
make_hash_filename(const std::string& owner, const std::string& hash)
{
	return std::string(Spool) +
	       "/exe-" +
	       escape_for_filename(owner) +
	       "-" +
	       hash;
}

std::string
ickpt_share_get_hash(ClassAd& ad)
{
	// for now, we only pay attention to the executable's MD5
	//
	MyString md5;
	if (!ad.LookupString(ATTR_JOB_CMD_MD5, md5)) {
		return "";
	}
	return escape_for_filename(ATTR_JOB_CMD_MD5) +
	       "-" +
	       escape_for_filename(md5.Value());
}

bool
ickpt_share_try_sharing(const std::string& owner,
                        const std::string& hash,
                        const std::string& ickpt_file)
{
	std::string hash_file = make_hash_filename(owner, hash);
	if (ickpt_share_link(hash_file.c_str(), ickpt_file.c_str()) == -1) {
		if (errno == ENOENT) {
			dprintf(D_FULLDEBUG,
			        "ickpt_share: %s not available for link from %s\n",
			        hash_file.c_str(),
			        ickpt_file.c_str());
		}
		else {
			dprintf(D_ALWAYS,
			        "ickpt_share: unexpected error linking %s to %s: %s\n",
			        ickpt_file.c_str(),
			        hash_file.c_str(),
			        strerror(errno));
		}
		return false;
	}
	dprintf(D_FULLDEBUG,
	        "ickpt_share: linked %s to %s\n",
	        ickpt_file.c_str(),
	        hash_file.c_str());
	return true;
}

void
ickpt_share_init_sharing(const std::string& owner,
                         const std::string& hash,
                         const std::string& ickpt_file)
{
	std::string hash_file = make_hash_filename(owner, hash);
	if (ickpt_share_link(ickpt_file.c_str(), hash_file.c_str()) == -1) {
		dprintf(D_ALWAYS,
		        "ickpt_share: unexpected error linking %s to %s: %s\n",
		        hash_file.c_str(),
		        ickpt_file.c_str(),
		        strerror(errno));
		return;
	}
	dprintf(D_FULLDEBUG,
	        "ickpt_share: linked %s to %s\n",
	        hash_file.c_str(),
	        ickpt_file.c_str());
}

void
ickpt_share_try_removal(const std::string& owner, const std::string& hash)
{
	std::string hash_file = make_hash_filename(owner, hash);
	struct stat st;
	if (ickpt_share_stat(hash_file.c_str(), &st) == -1) {
		dprintf(D_ALWAYS,
		        "ickpt_share: unexpected stat error on %s: %s\n",
		        hash_file.c_str(),
		        strerror(errno));
		return;
	}
	if (st.st_nlink != 1) {
		dprintf(D_FULLDEBUG,
		        "ickpt_share: link count for %s at %d\n",
		        hash_file.c_str(),
		        (int)st.st_nlink);
		return;
	}
	if (unlink(hash_file.c_str()) == -1) {
		dprintf(D_ALWAYS,
		        "ickpt_share: unexpected unlink error on %s: %s\n",
		        hash_file.c_str(),
		        strerror(errno));
		return;
	}
	dprintf(D_FULLDEBUG, "ickpt_share: removed %s\n", hash_file.c_str());
}
