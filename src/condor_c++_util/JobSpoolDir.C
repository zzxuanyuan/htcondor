#include "JobSpoolDir.h"
#include "directory.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "condor_ckpt_name.h"
#include "my_username.h"
#include "condor_uid.h"

/*****************************************************************************/

/** General interface to stat (2)

Results are a snapshot from when StatObj was created.
*/
// TODO: Use existing stat interface?
class StatObj
{
public:
	StatObj(const MyString & path) : name (path) { init(); }

	StatObj(const char * path) : name(path) { init(); }

	void refresh() {
		init();
	}

	// Was I unable to stat the file?
	// Possible reasons: ErrorDoesNotExist(), ErrorNoAccess() or "other"
	bool HasError() const {
		return myerrno;
	}

	// Something in the path doesn't exist
	bool ErrorDoesNotExist() const { 
		return myerrno == ENOENT;
	}

	bool ErrorNoAccess() const { 
		return myerrno == EACCES;
	}

	// Is this a directory?  Returns false on an error.
	bool IsDir() const {
		if(myerrno) { return false; }
		return S_ISDIR(mystat.st_mode);
	}

	// Can the owner read this? Returns false on an error.
	bool OwnerCanRead() const {
		if(myerrno) { return false; }
		return mystat.st_mode & S_IRUSR;
	}

	// Can the owner write this? Returns false on an error.
	bool OwnerCanWrite() const {
		if(myerrno) { return false; }
		return mystat.st_mode & S_IWUSR;
	}

	// Is the directory searchable (will opendir work?)
	// Is execute bit on unix.
	// Returns false on an error.
	bool OwnerCanSearch() const {
		if(myerrno) { return false; }
		if( ! IsDir() ) { return false; }
		return mystat.st_mode & S_IXUSR;
	}

	// What is the file size in bytes? Returns 0 on an error.
	off_t Size() const {
		if(myerrno) { return 0; }
		return mystat.st_size;
	}

	// Return the mode.  It's recommended to use more
	// specific functions.  Returns 0 on an error.
	mode_t Mode() const {
		if(myerrno) { return 0; }
		return mystat.st_mode;
	}

	// Time of last modification, kept as second since the epoch.
	// Returns 0 on error.
	time_t ModificationTime() const {
		if(myerrno) { return 0; }
		return mystat.st_mtime;
	}


private:
	void init()
	{
		myerrno = 0;
		if( stat(name.GetCStr(), &mystat) != 0)
		{
			myerrno = errno;
		}
	}


	struct stat mystat;
	int myerrno;
	MyString name;
};

/*****************************************************************************/

// Chowning implementation


bool is_directory(const char * path) 
{
	StatObj obj(path);
	return obj.IsDir();
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
	if( is_directory(path) ) {
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

// Random helper functions


/// Lookup a string in a classad (specialized to return a MyString)
// TODO: Move this into condor_attrlist, where it belongs
bool LookupString(ClassAd * p, const char * key, MyString & result)
{
	ASSERT(p);
	char * value;
	if( ! p->LookupString(key, &value) ) {
		return false;
	}
	result = value;
	free(value);
	return true;
}

bool param(const char * key, MyString & val)
{
#if 0
		// Debug code
#warning "param is hacked on"
	val = "/scratch/adesmet/V6_7-branch/condor-c/src/condor_schedd.V6/testspool";
	return true;
#endif
	char * valtmp = param(key);
	if( ! valtmp ) {
		return false;
	}
	val = valtmp;
	free(valtmp);
	return true;
}


/// Returns strerror for errno "e" as a MyString
MyString StringError(int e)
{
	char error[1024];
	strerror_r(e, error, sizeof(error)-1);
	MyString ret;
	ret.sprintf("%s (errno=%d)", error, e);
	return ret;
}

/// Concatenates a directory with a basename in that directory.
MyString ConcatDir(MyString lhs, MyString rhs)
{
	// TODO: Check if lhs already ends with DIR_DELIM_STRING,
	// don't append if that's the case.
	lhs += DIR_DELIM_STRING;
	lhs += rhs;
	return lhs;
}


// For const correctness
char *gen_ckpt_name_2 ( const char *dir, int cluster, int proc, int subproc )
{
	return gen_ckpt_name( const_cast<char *>(dir), cluster, proc, subproc );
}







/**********************************************************************/

#define AssertIsInitialized() AssertIsInitializedImpl(__FILE__, __LINE__)

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
	// We can't use AssertIsInitialized because that
	// fails if the process directory has been destroyed.
	// We can pretty much assume it has been at this point.
	if( ! IsInitialized() ) {
		EXCEPT("Internal Error: Attempting to use uninitialized JobSpoolDir");
	}
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
	AssertIsInitialized();
	return ConcatDir(DirFullCluster(), FileBaseExecutable());
}



void JobSpoolDir::AssertIsInitializedImpl(const char * filename, int linenum) const
{
	if( ! IsInitialized() ) {
		MyString errmsg;
		errmsg.sprintf("Internal Error: Attempting to use uninitialized JobSpoolDir (%s(%d))", filename, linenum);
		const char * msg = errmsg.GetCStr();
		EXCEPT(const_cast<char *>(msg));
	}
	if( is_destroyed ) {
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

	if( ! param("SPOOL", mainspooldir) ) {
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

	if( ! LookupString(JobAd, ATTR_JOB_CMD, cmd) ) {
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
	ASSERT(0); // TODO #error "TODO"
	// Verify only thing present is condor_exec.exe
	// Delete condor_exec.exe
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
		StatObj statobj(exe);
		if( ! statobj.ErrorDoesNotExist() ) {
			return exe;
		}
	}

	// The executable isn't in our JobSpoolDir
	// Try the old ickpt file in SPOOL.

	{
		char * spooltmp = param("SPOOL");
		ASSERT(spooltmp); // No SPOOL?  We're in deep trouble.
		MyString spool = spooltmp;
		free(spooltmp);

		MyString exe = gen_ckpt_name_2(spool.GetCStr(), cluster, ICKPT, 0);
		StatObj statobj(exe);
		if( ! statobj.ErrorDoesNotExist() ) {
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
	StatObj statobj(path);
	if( statobj.ErrorDoesNotExist() ) {

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

	statobj.refresh();

	// It already exists, (or was just created) quick usability check
	if(statobj.ErrorNoAccess()) {
		joberrordprintf("Unable to access job spool directory %s.",
			path.GetCStr());
		return false;
	}
	if( ! statobj.IsDir()) {
		joberrordprintf("Job spool %s is not a directory.",
			path.GetCStr());
		return false;
	}
	if( ! statobj.OwnerCanRead()) {
		joberrordprintf("Job spool directory %s lacks owner read permission.",
			path.GetCStr());
		return false;
	}
	if( ! statobj.OwnerCanWrite()) {
		joberrordprintf("Job spool directory %s lacks owner write permission.",
			path.GetCStr());
		return false;
	}
	if( ! statobj.OwnerCanSearch()) {
		joberrordprintf("Job spool directory %s lacks owner search permission.",
			path.GetCStr());
		return false;
	}

	return true;
}



void JobSpoolDir::test() 
{
	printf("%s\n",DirFullCluster().GetCStr());
	printf("%s\n",DirFullProcess().GetCStr());
	printf("%s\n",DirFullSandbox().GetCStr());
	printf("%s\n",DirFullTransfer().GetCStr());
	printf("%s\n",FileFullExecutable().GetCStr());
	jobdprintf(D_ALWAYS, "dprintf 1 (%s)", "substring");
	joberrordprintf("dprintf 2 (%s)", "substring");
	printf("%s\n",SandboxPath().GetCStr());
	printf("%s\n",TransferPath().GetCStr());
	DestroyProcessDirectory();
	DestroyClusterDirectory();
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


#if 0
int main()
{
	JobSpoolDir s;
	s.Initialize(123,4,5,true);
	//s.SetCmd("/example/executable");
	s.test();
	return 0;
}
#endif
