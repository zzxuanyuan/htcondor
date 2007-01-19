/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "condor_config.h"
#include "read_multiple_logs.h"
#include "basename.h"
#include "tmp_dir.h"
#include "dagman_multi_dag.h"

// Just so we can link in the ReadMultipleUserLogs class.
MULTI_LOG_HASH_INSTANCE;

//-------------------------------------------------------------------------
static void
AppendError(MyString &errMsg, const MyString &newError)
{
	if ( errMsg != "" ) errMsg += "; ";
	errMsg += newError;
}

//-------------------------------------------------------------------------
bool
GetLogFiles(/* const */ StringList &dagFiles, bool useDagDir, 
			StringList &condorLogFiles, StringList &storkLogFiles,
			MyString &errMsg)
{
	bool		result = true;

	TmpDir		dagDir;

	dagFiles.rewind();
	char *dagFile;
	while ( (dagFile = dagFiles.next()) != NULL ) {

//TEMPTEMP -- should I duplicate a lot of this code to get the config file, or should I come up with some way of avoiding that?
		const char *	file;
		if ( useDagDir ) {
			MyString	tmpErrMsg;
			if ( !dagDir.Cd2TmpDirFile( dagFile, tmpErrMsg ) ) {
				AppendError( errMsg,
						MyString("Unable to change to DAG directory ") +
						tmpErrMsg );
				return false;
			}
			file = condor_basename( dagFile );
		} else {
			file = dagFile;
		}

			// Note: this returns absolute paths to the log files.
		MyString msg = MultiLogFiles::getJobLogsFromSubmitFiles(
				file, "job", "dir", condorLogFiles);
		if ( msg != "" ) {
			AppendError( errMsg,
					MyString("Failed to locate Condor job log files: ") +
					msg );
			result = false;
		}

			// Note: this returns absolute paths to the log files.
		msg = MultiLogFiles::getJobLogsFromSubmitFiles(
				file, "data", "dir", storkLogFiles);
		if ( msg != "" ) {
			AppendError( errMsg,
					MyString("Failed to locate Stork job log files: ") +
					msg );
			result = false;
		}

		MyString	tmpErrMsg;
		if ( !dagDir.Cd2MainDir( tmpErrMsg ) ) {
			AppendError( errMsg,
					MyString("Unable to change to original directory ") +
					tmpErrMsg );
			result = false;
		}

	}

	return result;
}

//-------------------------------------------------------------------------
bool
LogFileNfsError(/* const */ StringList &condorLogFiles,
			/* const */ StringList &storkLogFiles)
{
	condorLogFiles.rewind();
	storkLogFiles.rewind();

	bool nfsIsError = param_boolean( "DAGMAN_LOG_ON_NFS_IS_ERROR", true );

	if (MultiLogFiles::logFilesOnNFS(condorLogFiles, nfsIsError)) {
		fprintf( stderr, "Aborting -- "
				"Condor log files should not be on NFS.\n");
		return true;
	}

	if (MultiLogFiles::logFilesOnNFS(storkLogFiles, nfsIsError)) {
		fprintf( stderr, "Aborting -- "
				"Stork log files should not be on NFS.\n");
		return true;
	}

	return false;
}

//-------------------------------------------------------------------------
bool
GetConfigFile(/* const */ StringList &dagFiles, bool useDagDir, 
			MyString &configFile, MyString &errMsg)
{
	bool		result = true;
//TEMPTEMP -- this should test for conflict between different DAG files and also conflict between the DAG file and the command-line spec
//TEMPTEMP -- hmm -- need to convert everything to absolute paths?  canonical paths?  what about symlinks causing conflict?
//TEMPTEMP -- be sure to test config files with multiple DAGs with and without usedagdir

	TmpDir		dagDir;

	dagFiles.rewind();
	char *dagFile;
	while ( (dagFile = dagFiles.next()) != NULL ) {
		const char *	file;
		if ( useDagDir ) {
			MyString	tmpErrMsg;
			if ( !dagDir.Cd2TmpDirFile( dagFile, tmpErrMsg ) ) {
				AppendError( errMsg,
						MyString("Unable to change to DAG directory ") +
						tmpErrMsg );
				return false;
			}
			file = condor_basename( dagFile );
		} else {
			file = dagFile;
		}

printf("DIAG file: <%s>\n", file);//TEMPTEMP


		//TEMPTEMP -- get config file here
		StringList		configFiles;
		MyString msg = MultiLogFiles::getValuesFromFile( file, "config",
					configFiles);
		if ( msg != "" ) {
			AppendError( errMsg,
					MyString("Failed to locate Condor job log files: ") +
					msg );
			result = false;
		}
//TEMPTEMP -- make all config file paths absolute?

		configFiles.rewind();
		char *		cFile;
		while ( (cFile = configFiles.next()) ) {
printf( "DIAG cFile: <%s>\n", cFile );//TEMPTEMP
				//TEMPTEMP -- change name below?
			MyString	config = cFile;
			if ( MakePathAbsolute( config ) ) {
				if ( configFile == "" ) {
					configFile = config;
				} else if ( configFile != config ) {
					fprintf( stderr, "Conflicting DAGMan config files "
								"specified: %s and %s\n", configFile.Value(),
								config.Value() );
					result = false;
					//TEMPTEMP -- bail out of loop?
				}
			} else {
				result = false;
				//TEMPTEMP -- bail out of loop?
			}
		}

		MyString	tmpErrMsg;
		if ( !dagDir.Cd2MainDir( tmpErrMsg ) ) {
			AppendError( errMsg,
					MyString("Unable to change to original directory ") +
					tmpErrMsg );
			result = false;
		}
	}

	return result;
}

//-------------------------------------------------------------------------
bool
MakePathAbsolute(MyString &filePath)
{
	bool		result = true;

	if ( !fullpath( filePath.Value() ) ) {
		MyString    currentDir;
		char    tmpCwd[PATH_MAX];
		if ( getcwd(tmpCwd, PATH_MAX) ) {
			currentDir = tmpCwd;
		} else {
			fprintf( stderr,
						"ERROR: getcwd() failed with errno %d (%s) at %s:%d\n",
						errno, strerror(errno), __FILE__, __LINE__);
			result = false;
		}

		filePath = currentDir + DIR_DELIM_STRING + filePath;
	}

	return result;
}
