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

#ifndef JOBSPOOLDIR_H
#define JOBSPOOLDIR_H

#include "condor_common.h"
#include "condor_classad.h"
#include "MyString.h"
#include "string_list.h"
//#include "globus_utils.h"
//#include "classad_hashtable.h"
//
//#include "proxymanager.h"
//#include "basejob.h"
//#include "globusresource.h"
//#include "gahp-client.h"


/**
Manages a per-job spool directory under SPOOL

Creates, manages, and destroys a subdirectory under SPOOL unique
to the job (Cluster/Process/SubProcess).

The heirarchy:
/path/to/spool/           - From param('SPOOL') (eg. SPOOL=/path/to/spool)
    cluster1/             - Per cluster directory (eg. cluster 1)
        condor_exec.exe   - Executable for all jobs in cluster (AKA ickpt)
        proc0.0/          - Per process/subproc directory (eg.  job 1.0.0)
            sandbox/      - Initial working directory for sandboxed jobs
                               (via "condor_submit -s" or Condor-C).
                               Also holds results from evictions when
                               when_to_transfer_output=ON_EXIT_OR_EVICT
            transfer/     - Staging point for file transfer transactions
			                   Files will eventually be up in the job's Iwd
*/
class JobSpoolDir
{
public:
	/** Constructor.

	Object is invalid until an Initialize() function is called.
	*/
	JobSpoolDir();

	/** Configures the JobSpoolDir for this particular job.

	If allow_create is true, the cluster and proc directories
	will be created if they doesn't exist.  If allow_create is false,
	than the directories must exist or this fails.

	Returns true on success, false on failure.  On failure an
	error message with details is written to the log.

	If Initialize fails the object enters an undefined state, the
	only safe thing to do with it is to destroy it.
	*/
	bool Initialize(int icluster, int iproc, int isubproc, bool allow_create);
	bool Initialize(ClassAd * JobAd, bool allow_create);


	/** Specific the Cmd for the job.

	The Cmd can be read from the Job's ClassAd on initialization, but you
	may need to set it anyway:
	1. It may change via qedit (ick)
	2. You can initialize without a JobAd in some situations.

	This lets you correct the Cmd information.
	*/
	void InitializeCmd(const char * cmd);

	/** Destroys the directories for this process.

	This will include the sandbox and transfer subdirectories

	On an error, writes an error message to the log.
	*/
	void DestroyProcessDirectory();


	/** Returns the full path to the sandbox for this process

	If allow_create is true, the directory will be created if it
	doesn't exist.  If allow_create is false, than the directory
	must exist or this fails.

	On an error (either failure to create the directory or
	allow_create==false and the directory doesn't exist), writes
	an error to the log and returns an empty string.
	*/
	MyString SandboxPath(bool allow_create = true);


	/** Changes directory ownership of the process directory to the user.

	Tries a number of tricks to do so if a simple "chown" isn't
	good enough.
	*/
	void ChownProcessDirToUser(uid_t uid, gid_t gid);

	/** Changes directory ownership of the process directory to the "condor"
	 user (or whoever the daemon's euid is.)

	Tries a number of tricks to do so if a simple "chown" isn't
	good enough.
	*/
	void ChownProcessDirToCondor();


	/** Returns the full path to the temporary transfer directory for this process

	This directory is intended for staging files in.
	Once all files have been staged, the contents should be
	immediately moved to the job's IWD (possibly the Sandbox)

	If allow_create is true, the directory will be created if it
	doesn't exist.  If allow_create is false, than the directory
	must exist or this fails.

	On an error (either failure to create the directory or
	allow_create==false and the directory doesn't exist), writes an
	error to the log and returns an empty string.
	*/
	MyString TransferPath(bool allow_create = true);


	/** Deletes all contents of the transfer directory

	Returns true on success. On an error, writes an error message
	to the log and returns false.
	*/
	void EraseTransferContents();





	/** Destroys the spool directory for this *cluster*

	This assumes that the cluster directory is empty, if it
	isn't this will fail.

	This call is allowed after DestroyProcessDirectory()
	*/
	void DestroyClusterDirectory();



	/** Return the path to the executable, suitable for reading

	Not suitable if you want to write the executable (that is, the
	ickpt file).

	This will actually check a number of places for the
	executable.

	This object must know about the Job's Cmd; either by its
	present during Initialize, or via SetCmd().  This will fail
	if the executable doesn't exist at all or if the Job's Cmd is
	not known.

	On failure writes a log message and returns an empty string.
	Returns an empty string on failure
	*/
	MyString ExecutablePathForReading() const;

	/** Return the path to the executable, suitable for writing (ickpt)

	This should only be used when copying the ickpt file into the
	spool.
	*/
	MyString ExecutablePathForWriting() const;

private:
	int cluster;
	int proc;
	int subproc;

	uid_t prevuid;

	bool is_initialized;
	bool is_destroyed;

	MyString mainspooldir;
	MyString cmd;

	/// Is this object initialized and valid?
	bool IsInitialized() const;

	/* This set of functions returns various noteworthy directories
		DirBase* returns just the single directory name ("p0.0")
		while DirFull* returns the full path ("/path/to/SPOOL/c1/p0.0")
	*/
		/// What is the spool directory for this cluster?
	MyString DirBaseCluster() const;
	MyString DirFullCluster() const;
		/// What is the spool directory for this process (clust.proc.subproc)?
	MyString DirBaseProcess() const;
	MyString DirFullProcess() const;
		/// What is the sandbox directory for this job?
	MyString DirFullSandbox() const;
		/// What is the transfer directory for this job?
	MyString DirFullTransfer() const;
		/// Find the executable (AKA "ickpt")
	MyString FileBaseExecutable() const;
	MyString FileFullExecutable() const;

	/** Tests IsInitialized().  If not logs an error message and EXCEPTs

	If destroyed_ok is true than it's okay if
	the process directory has been destroyed.  Otherwise we also require
	that the directory not have been destroyed.

	You'll probably want to use AssertIsInitialized or
	AssertIsInitializedDestroyedOK, macros that automatically handles filename
	and linenum.
	*/
	void AssertIsInitializedImpl(const char * filename, int linenum, bool destroyed_ok) const;




	/** Check if a directory exists and is usable.

	Returns true if it is, false if it isn't.  If allow_create is
	true, it will try to create the directory if it doesn't exist.
	Will _not_ create multiple levels of directory, so the parent 
	directory better exist.
	*/
	bool EnsureUsableDir(const MyString & path, bool allow_create);




	/** As dprintf, but automatically prefixes useful info

	Specifically, it prefixes "(cluster.proc.subproc) JobSpoolDir" and appends
	a newline.
	*/
	void jobdprintf(int flags, const char * fmt, ...) const;

	/** As jobdprintf, but assumes D_ALWAYS and prefixes "Error:"
	*/
	void joberrordprintf(const char * fmt, ...) const;


	void ChownProcessDir(uid_t srcuid, uid_t uid, gid_t gid);


	/// INITIALIZE/ASSIGNMENT/COPY NOT CURRENTLY IMPLEMENTED
	JobSpoolDir(const JobSpoolDir & src);
	/// INITIALIZE/ASSIGNMENT/COPY NOT CURRENTLY IMPLEMENTED
	const JobSpoolDir & operator=(const JobSpoolDir & src);


};


/*****************************************************************************
  Future consideration.
*/

/** Return cluster.process.subproc for a spool directory

Given a job spool directory, determine the identity of the job.
On success returns return and places the identity in cluster,
process, subprocess.  On failure returns false (this doesn't
appear to be a spool directory).

Expected user: condor_preen (so it doesn't need to know how the
names are built).

bool ParseJobSpoolDirName(const char * filename, int & cluster,
	int & process, int & subprocess);
*/


#endif /* ifndef JOBSPOOLDIR_H */

