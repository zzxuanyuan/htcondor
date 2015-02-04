#ifndef _CONDOR_DOCKER_API_H
#define _CONDOR_DOCKER_API_H

#include <string>
#include "condor_arglist.h"


class DockerAPI {
	public:

		/**
		 * Runs command in the Docker identified by imageID.  The container
		 * will be named name.  The command will run in the given
		 * environment with the given arguments.  The given directory will
		 * be mounted as itself in the container, and will be the initial
		 * working directory.
		 *
		 * If run() succeeds, the pid will be that of a process which will
		 * terminate when the instance does.  The error will be unchanged.
		 *
		 * If run() fails, it will return a negative number.
		 *
		 * @param name 			Must not be empty.
		 * @param imageID		For now, must be the GUID.
		 * @param command		A full path, or a binary in the container's PATH.
		 * @param arguments		The arguments to the command.
		 * @param environment	The environment in which to run.
		 * @param directory		A full path.
		 * @param pid			On success, will be set to the PID of a process which will terminate when the container does.  Otherwise, unchanged.
		 * @param childFDs		The redirected std[in|out|err] FDs.
		 * @param cpuAffinity	Pointer to a list of integers; the first is the size of the list (including itself).  Each element in the list is a CPU ID.
		 * @param memoryInMB	The memory limit, in MB, for this container.  Undefined if 0.
		 * @param error			....
		 * @return 				0 on success, negative otherwise.
		 */
		static int run(	const std::string & name,
						const std::string & imageID,
						const std::string & command,
						const ArgList & arguments,
						const Env & environment,
						const std::string & directory,
						int & pid,
						int * childFDs,
						int * cpuAffinity,
						int memoryInMB,
						CondorError & error );

		/**
		 * Releases the disk space (but not the image) associated with
		 * the given container.
		 *
		 * @param container		The Docker GUID, or the name passed to run().
		 * @param error			....
		 * @return				0 on success, negative otherwise.
		 */
		static int rm( const std::string & container, CondorError & err );

		/**
		 * Sends the given signal to the specified container's primary process.
		 *
		 * @param container		The Docker GUID, or the name passed to run().
		 * @param signal		The signal to send.
		 * @param error			....
		 * @return				0 on success, negative otherwise.
		 */
		static int kill( const std::string & container, int signal, CondorError & err );

		// Only available in Docker 1.1 or later.
		static int pause( const std::string & container, CondorError & err );

		// Only available in Docker 1.1 or later.
		static int unpause( const std::string & container, CondorError & err );

		/**
		 * Obtains the docker-inspect values State.Running and State.ExitCode.
		 *
		 * @param container		The Docker GUID, or the name passed to run().
		 * @param isRunning		On success, will be set to State.Running.  Otherwise, unchanged.
		 * @param exitCode		On success, will be set to State.ExitCode.  Otherwise, unchanged.
		 * @param error			....
		 * @return				0 on success, negative otherwise.
		 */
		static int getStatus( const std::string & container, bool isRunning, int & result, CondorError & err );

		/**
		 * Attempts to detect the presence of a working Docker installation.
		 * Also returns the configured DOCKER's version string.
		 *
		 * @param version		On success, will be set to the version string.  Otherwise, unchanged.
		 * @param error			....
		 * @return				0 on success, negative otherwise.
		 */
		static int detect( std::string & version, CondorError & err );

		/**
		 * Returns a ClassAd corresponding to a subset of the output of
		 * 'docker inspect'.
		 *
		 * @param container		The Docker GUID, or the name passed to run().
		 * @param inspectionAd	Populated on success, unchanged otherwise.
		 * @param error			....
		 * @return				0 on success, negative otherwise.
		 */
		static int inspect( const std::string & container, ClassAd * inspectionAd, CondorError & err );

	protected:
		static int version( std::string & version, CondorError & err );
};

#endif /* _CONDOR_DOCKER_API_H */
