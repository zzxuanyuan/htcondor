#include "JobSpoolDir.h"
#include "directory.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "condor_ckpt_name.h"
#include "my_username.h"
#include "condor_uid.h"


/*****************************************************************************/

// Chowning implementation

bool file_exists(const char * path)
{
	StatInfo obj(path);
	return obj.Error() != SINoFile;
}

bool file_exists(const MyString & path)
{
	return file_exists(path.GetCStr());
}

bool recursive_chown_impl(const char * path, 
	uid_t src_uid, uid_t dst_uid, gid_t dst_gid);

/*
TODO: What happens if this is interrupted?
	- Some files with different ownerships.
		- Redoing should fix
*/
/// See recursive_chown
bool recursive_chown_impl_fast(const char * path,
	uid_t src_uid,  uid_t dst_uid, gid_t dst_gid)
{
	// TODO: for the sake of paranoia, verify that the file's current
	// uid is the same as src_uid
	if( chown(path, dst_uid, dst_gid) != 0 ) {
		return false;
	}
	if( IsDirectory(path) ) {
		// TODO: use Directory object to spin directory.
		DIR * d = opendir(path);
		if( ! d ) {
			dprintf(D_ALWAYS, "recursive_chown_impl_fast unable to opendir(%s) %d.%d\n",path,(int)dst_uid,(int)dst_gid);
			return false;
		}
		struct dirent * ent;
		while( (ent = readdir(d)) ) {
			if(strcmp(ent->d_name, ".") == 0) { continue; }
			if(strcmp(ent->d_name, "..") == 0) { continue; }
			MyString subpath(path);
			subpath += DIR_DELIM_STRING;
			subpath += ent->d_name;
			if( ! recursive_chown_impl(subpath.GetCStr(), src_uid,
					dst_uid, dst_gid) ) {
				return false;
			}
		}

		closedir(d);
	}
	return true;
}


/// See recursive_chown
bool recursive_chown_impl(const char * path, 
	uid_t src_uid, uid_t dst_uid, gid_t dst_gid)
{
	ASSERT(get_priv() == PRIV_ROOT);
	
	if( recursive_chown_impl_fast(path, src_uid, dst_uid, dst_gid) ) {
		return true;
	}
	/* // TODO
	// No luck with the fast (chown()-based) implementation.  Try
	// the slow (copy-based) implementation
	if( recursive_chown_impl_slow(const char * path, uid_t uid, gid_t gid) ) {
		return true;
	}
	*/

	// Still no luck.
	dprintf(D_ALWAYS, "Error: Unable to chown '%s' to %d.%d\n",
		path, (int)dst_uid, (int)dst_gid);
	return false;
}

/** chown a file and all children (if it's a directory)

path - file/directory to chown
src_uid - UID of the file now.  (Used for safety, encountering
    a file owned by another uid is considered an error)
dst_uid - UID to switch files to
dst_gif - GID to switch files to

Returns true on success, false on failure.

*/
bool recursive_chown(const char * path,
	uid_t src_uid, uid_t dst_uid, gid_t dst_gid)
{
	if( ! can_switch_ids()) {
		return false;
	}
	// Lower levels of the implementation assume we're
	// root, which is reasonable since we have no hope of doing
	// a chown if we're not root.
	priv_state previous = set_priv(PRIV_ROOT);
	bool ret = recursive_chown_impl(path,src_uid,dst_uid,dst_gid);
	set_priv(previous);
	return  ret;
}

/** As recursive_chown, but if we're not root silently skip and succeed
*/
bool recursive_chown_nonroot(const char * path,
	uid_t src_uid, uid_t dst_uid, gid_t dst_gid)
{
	if( ! can_switch_ids() ) {
		dprintf(D_FULLDEBUG, "Unable to chown %s; process lacks the ability to change UIDs.  Skipping chown attempt.\n", path);
		return true;
	}
	return recursive_chown(path, src_uid, dst_uid, dst_gid);
}


/*****************************************************************************/


/// Returns strerror for errno "e" as a MyString
MyString StringError(int e)
{
	char * error = strerror(e);
	MyString ret;
	ret.sprintf("%s (errno=%d)", error, e);
	return ret;
}

/// Concatenates a directory with a basename in that directory.
MyString ConcatDir(const MyString & lhs, const MyString & rhs)
{
	char * tmp = dircat(lhs.GetCStr(), rhs.GetCStr());
	MyString ret(tmp);
	delete tmp; // dircat allocated new space.
	return ret;
}


// For const correctness
char *gen_ckpt_name_2 ( const char *dir, int cluster, int proc, int subproc )
{
	return gen_ckpt_name( const_cast<char *>(dir), cluster, proc, subproc );
}







/**********************************************************************/

// Validate that the object is initialized and the associated
// directory not (yet) destroyed.
#define AssertIsInitialized() AssertIsInitializedImpl(__FILE__, __LINE__, false)
// Validate that the object is initialized
#define AssertIsInitializedDestroyedOK() AssertIsInitializedImpl(__FILE__, __LINE__, true)

JobSpoolDir::JobSpoolDir()  :
	cluster(0),
	proc(0),
	subproc(0),
	prevuid(0),
	is_initialized(false),
	is_destroyed(false)
{
}

bool JobSpoolDir::IsInitialized() const
{
	return is_initialized;
}

MyString JobSpoolDir::DirBaseCluster() const
{
	AssertIsInitializedDestroyedOK();
	MyString s;
	s.sprintf("cluster%d", cluster);
	return s;
}

MyString JobSpoolDir::DirFullCluster() const
{
	//  Don't AssertIsInitialized, let DirBaseCluster handle it.
	return ConcatDir(mainspooldir, DirBaseCluster());
}

MyString JobSpoolDir::DirBaseProcess() const
{
	AssertIsInitialized();
	MyString s;
	s.sprintf("proc%d.%d", proc, subproc);
	return s;
}

MyString JobSpoolDir::DirFullProcess() const
{
	AssertIsInitialized();
	return ConcatDir(DirFullCluster(), DirBaseProcess());
}

MyString JobSpoolDir::DirFullSandbox() const
{
	AssertIsInitialized();
	return ConcatDir(DirFullProcess(), "sandbox");
}

MyString JobSpoolDir::DirFullTransfer() const
{
	AssertIsInitialized();
	return ConcatDir(DirFullProcess(), "transfer");
}

MyString JobSpoolDir::FileBaseExecutable() const
{
	return CONDOR_EXEC;
}

MyString JobSpoolDir::FileFullExecutable() const
{
	AssertIsInitializedDestroyedOK();
	return ConcatDir(DirFullCluster(), FileBaseExecutable());
}



void JobSpoolDir::AssertIsInitializedImpl(const char * filename, int linenum, bool destroyed_ok) const
{
	if( ! IsInitialized() ) {
		MyString errmsg;
		errmsg.sprintf("Internal Error: Attempting to use uninitialized JobSpoolDir (%s(%d))", filename, linenum);
		const char * msg = errmsg.GetCStr();
		EXCEPT(const_cast<char *>(msg));
	}
	if( destroyed_ok == false && is_destroyed ) {
		MyString errmsg;
		errmsg.sprintf("Internal Error: Attempting to use destroyed JobSpoolDir (%s(%d))", filename, linenum);
		const char * msg = errmsg.GetCStr();
		EXCEPT(const_cast<char *>(msg));
	}
}

bool JobSpoolDir::Initialize(int incluster, int inproc, int insubproc, bool allow_create)
{
	ASSERT( ! IsInitialized() ); // Don't multiply initialize

	cluster = incluster;
	proc = inproc;
	subproc = insubproc;

	mainspooldir = param_mystring("SPOOL");
	if(mainspooldir.Length() < 1) {
		joberrordprintf("Unable to locate SPOOL setting.");
		return false;
	}

	// Must be set for other functions to work.
	is_initialized = true;

	if( ! EnsureUsableDir(DirFullCluster(), allow_create) ) {
		is_initialized = false;
		return false;
	}
	if( ! EnsureUsableDir(DirFullProcess(), allow_create) ) {
		is_initialized = false;
		return false;
	}

	return true;
}


bool JobSpoolDir::Initialize(ClassAd * JobAd, bool allow_create)
{
	ASSERT(JobAd);

	const char * ERROR_MISSING_ATTR = "Error: Internal consistency problem: job lacks a %s";

	int incluster, inproc, insubproc;

	if( ! JobAd->LookupInteger(ATTR_CLUSTER_ID, incluster) ) {
		jobdprintf(D_ALWAYS, ERROR_MISSING_ATTR, ATTR_CLUSTER_ID);
		return false;
	}

	if( ! JobAd->LookupInteger(ATTR_PROC_ID, inproc) ) {
		jobdprintf(D_ALWAYS, ERROR_MISSING_ATTR, ATTR_PROC_ID);
		return false;
	}

	// TODO: subproc?
	insubproc = 0;

	if( ! JobAd->LookupString(ATTR_JOB_CMD, cmd) ) {
		jobdprintf(D_ALWAYS, ERROR_MISSING_ATTR, ATTR_JOB_CMD);
		return false;
	}

	return Initialize(incluster,inproc,insubproc,allow_create);
}

void JobSpoolDir::InitializeCmd(const char * newcmd)
{
	cmd = newcmd;
}


void JobSpoolDir::DestroyProcessDirectory()
{
	AssertIsInitialized();
	MyString jobspool = DirFullProcess();
	// Directory is really for enumeration, but appears to be 
	// the best directory scrubber we have.
	Directory dir(jobspool.GetCStr());
	if( ! dir.Remove_Entire_Directory() )
	{
		joberrordprintf("Failed to remove contents of %s.  Directory will be left behind.",
			jobspool.GetCStr());
		return;
	}
	if( rmdir(jobspool.GetCStr()) != 0 ) {
		joberrordprintf("Failed to remove  %s.  Error: %s. Directory will be left behind.",
			jobspool.GetCStr(), StringError(errno).GetCStr());
	}

	// TODO: If DirFullCluster is now empty, should we DestoryClusterDirectory()?

	is_destroyed = true;
}


MyString JobSpoolDir::SandboxPath(bool allow_create /* = true */)
{
	MyString dir = DirFullSandbox();
	if( ! EnsureUsableDir(dir, allow_create) ) {
		return false;
	}
	return dir;
}

void JobSpoolDir::ChownProcessDir(uid_t srcuid, uid_t uid, gid_t gid)
{
	AssertIsInitialized();
	MyString path = SandboxPath(false);
	if( path.Length() < 1) {
		joberrordprintf("Unable to locate andbox path for this job while attempting to change ownership of it.");
		return;
	}
	recursive_chown_nonroot(path.GetCStr(), srcuid, uid, gid);

	path = ExecutablePathForReading();
	if(path == ExecutablePathForWriting()) {
		// The executable is in the spool and may need chowning
		// TODO: Skip if already owned by uid.gid?
		recursive_chown_nonroot(path.GetCStr(), srcuid, uid, gid);
	}
}

void JobSpoolDir::ChownProcessDirToUser(uid_t uid, gid_t gid)
{
	AssertIsInitialized();
	if(prevuid != 0) {
		if(prevuid == uid) {
			EXCEPT("Unexpected attempt to chown process directory more than once in a harmless but redundant way.");
		}
		EXCEPT("Unexpected attempt to chown process directory more than once to different users.");
	}
	prevuid = uid;
	ChownProcessDir(get_condor_uid(), uid, gid);
}

void JobSpoolDir::ChownProcessDirToCondor()
{
	AssertIsInitialized();
	ChownProcessDir(prevuid, get_condor_uid(), get_condor_gid());
	prevuid = 0;
}

MyString JobSpoolDir::TransferPath(bool allow_create /* = true */)
{
	MyString dir = DirFullTransfer();
	if( ! EnsureUsableDir(dir, allow_create) ) {
		return false;
	}
	return dir;
}

void JobSpoolDir::EraseTransferContents()
{
	MyString path = TransferPath(false);
	if(path.Length() == 0) {
		joberrordprintf("Attempting to erase contents of non-existant transfer directory.");
	}
	// Directory is really for enumeration, but appears to be 
	// the best directory scrubber we have.
	Directory dir(path.GetCStr());
	if( ! dir.Remove_Entire_Directory() )
	{
		joberrordprintf("Failed to remove contents of transfer directory %s.",
			path.GetCStr());
		return;
	}
}

void JobSpoolDir::DestroyClusterDirectory()
{
	MyString clusterdir = DirFullCluster();

	MyString exefile = FileFullExecutable();
	if(unlink(exefile.GetCStr())) {
		if( file_exists(exefile) ) {
			joberrordprintf("Failed to remove %s.  Error: %s.  May be unable to destroy cluster directory %s.\n", exefile.GetCStr(), StringError(errno).GetCStr(), clusterdir.GetCStr());
		}
	}

	// Note: assuming that rmdir fails if there are files in the directory.
	if( rmdir(clusterdir.GetCStr()) != 0 ) {
		joberrordprintf("Failed to remove  %s.  Error: %s. Directory will be left behind.",
			clusterdir.GetCStr(), StringError(errno).GetCStr());
	}
}




MyString JobSpoolDir::ExecutablePathForReading() const
{
	AssertIsInitialized();

	// Check in the JobSpoolDir
	{
		MyString exe = ExecutablePathForWriting();
		if( file_exists(exe) ) {
			return exe;
		}
	}

	// The executable isn't in our JobSpoolDir
	// Try the old ickpt file in SPOOL.

	{
		MyString spool = param_mystring("SPOOL");
		ASSERT(spool.Length() > 0); // No SPOOL?  We're in deep trouble.

		MyString exe = gen_ckpt_name_2(spool.GetCStr(), cluster, ICKPT, 0);
		if( file_exists(exe) ) {
			return exe;
		}
	}

	// Still no luck
	// Try whatever's in the job ad
	{
		if(cmd.Length() > 0) {
			return cmd;
		}
	}

	// There isn't a Cmd in the job ad.  Something has gone very wrong.
	// There is nothing left to do. Return a wild guess and report an error.
	joberrordprintf("Job lacks attribute '%s'.\n", ATTR_JOB_CMD);
	joberrordprintf("Unable to locate executable for job.\n");
	return ExecutablePathForWriting();
}

MyString JobSpoolDir::ExecutablePathForWriting() const
{
	AssertIsInitialized();
	return FileFullExecutable();
}

bool JobSpoolDir::EnsureUsableDir(const MyString & path, bool allow_create)
{
	if( ! file_exists(path) ) {

		if( ! allow_create ) {
			joberrordprintf("Job spool directory %s does not exist.",
				path.GetCStr());
			return false;
		}

		// 0700 Seems reasonably paranoid to me.  Perhaps overly so?
		if( mkdir(path.GetCStr(), 0700) != 0 ) {
			joberrordprintf("Unable to create job spool directory %s.  Error: %s.",
				path.GetCStr(), StringError(errno).GetCStr());
			return false;
		}

	}

	StatInfo statobj(path.GetCStr());

	// It already exists, (or was just created) quick usability check
	if(statobj.Error()) {
		joberrordprintf("Unable to access job spool directory %s.",
			path.GetCStr());
		return false;
	}
	if( ! statobj.IsDirectory()) {
		joberrordprintf("Job spool %s is not a directory.",
			path.GetCStr());
		return false;
	}
	if( ! statobj.IsOwnerReadable()) {
		joberrordprintf("Job spool directory %s lacks owner read permission.",
			path.GetCStr());
		return false;
	}
	if( ! statobj.IsOwnerWritable()) {
		joberrordprintf("Job spool directory %s lacks owner write permission.",
			path.GetCStr());
		return false;
	}
	if( ! statobj.IsOwnerSearchable()) {
		joberrordprintf("Job spool directory %s lacks owner search permission.",
			path.GetCStr());
		return false;
	}

	return true;
}




void JobSpoolDir::jobdprintf(int flags, const char * fmt, ...) const
{
	MyString results;
    va_list args;
    va_start( args, fmt );
	if( ! results.vsprintf(fmt, args) )
	{
		// Unable to use MyString's version.  Odd.  Well,
		// Try a last ditch...
		dprintf(flags, "Message for (%d.%d.%d) JobSpoolDir\n",
			cluster, proc, subproc);
		_condor_dprintf_va(flags, const_cast<char *>(fmt), args);
    	va_end( args );
		return;
	}
    va_end( args );

#if 0
	// Debugg code
#warning "jobdprintf is hacked on"
	printf("(%d.%d.%d) JobSpoolDir %s\n",
		cluster, proc, subproc, results.GetCStr());
#endif
	dprintf(flags, "(%d.%d.%d) JobSpoolDir %s\n", cluster, proc, subproc, results.GetCStr());
}


void JobSpoolDir::joberrordprintf(const char * fmt, ...) const
{
	MyString results;
    va_list args;
    va_start( args, fmt );
	if( ! results.vsprintf(fmt, args) )
	{
		// Unable to use MyString's version.  Odd.  Well,
		// Try a last ditch...
		dprintf(D_ALWAYS, "ERROR Message for (%d.%d.%d) JobSpoolDir\n",
			cluster, proc, subproc);
		_condor_dprintf_va(D_ALWAYS, const_cast<char *>(fmt), args);
    	va_end( args );
		return;
	}
    va_end( args );

	jobdprintf(D_ALWAYS, "Error: %s", results.GetCStr());
}
