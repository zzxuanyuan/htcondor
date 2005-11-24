#include "condor_common.h"
#include "../condor_procapi/procapi.h"
#include "uniq_pid_midwife.h"
#include "uniq_pid_tool_file_io.h"
#include "uniq_pid_util.h"

//***
//constant static variables
//***
const static int DEFAULT_BUFFER_SIZE = 100;

//***
// Helper function declarations
//***
pid_t midwife_executable(int argc, char* argv[], 
						 const char* pidfile, 
						 unsigned int precision_range);


/*
  Main function for the midwife program.
  Midwifes a program into existence, creating a pid file artifact
  of its creation.
*/
int 
midwife_main(int argc, char* argv[], 
			 const char* pidfile, 
			 bool blockflag, 	
			 unsigned int precision_range)
{
		// usage check
	if( argc < 1 ){
		fprintf(stderr, MIDWIFE_USAGE);
		exit(EXIT_FAILURE);
	}

		// midwife the executable
	pid_t pid = midwife_executable(argc, argv, pidfile, precision_range);
	if( pid == -1 ){
		exit(EXIT_FAILURE);
	}

		// block until the child completes, if neccessary
	if( blockflag && waitpid(pid, NULL, 0) == -1 ){
		perror("ERROR: Could not waitpid(...) in midwife_main");
		exit(EXIT_FAILURE);
	} 

		// success
	exit(EXIT_SUCCESS);
	return 0;
}

/*
  Midwifes the given executable into existance, creating a pid
  file for use by an undertaker.
*/
pid_t 
midwife_executable(int argc, char* argv[], 
				   const char* pidfile, unsigned int precision_range)
{
  
		// Ensure we can create and write the pid file
	FILE* fp = createPidFile(pidfile);
	if( fp == NULL ){
		fprintf(stderr, 
				"ERROR: Could not create the pidfile [%s] in midwife_executable\n",
				pidfile);
		return -1;
	}

		// fork 
		// Must indicate that the parent wants notification of 
		// child's exit.  Therefore there is no chance of a race
		// where child dies and its pid is reused before the 
		// parent calls generateSignature(...).  As long as the parent
		// is alive and hasn't reaped the child, the pid cannot
		// be reused.
	pid_t pid = fork();
  

		// CHILD:
	if( pid == 0 ){
    
			// exec the program with its args
		if( execvp(argv[0], argv) == -1 ){
			char buffer[DEFAULT_BUFFER_SIZE];
			sprintf(buffer, "FAILED: Midwife child could not exec[%s]", argv[0]);
			perror(buffer);
		}

		exit(EXIT_FAILURE);
	}

		// ERROR: 
	else if( pid == -1 ){
		perror("FAILED: Midwife could not fork");
		return -1;
	}

		// PARENT:
	else{
			// generate the child's signature
		int status = 0;
		process_signature_t proc_sig;
		if( generateProcessSignature(pid, &proc_sig, &status) == UNIQ_PID_FAILURE ){
			uniq_pid_perror(status,
							"ERROR: Failed to generate process signature in midwife_executable\n");
			return -1;
		}

			// write the child's signature to the pid file
		if( writeProcessSignature(fp, &proc_sig) == -1 ){
			fprintf(stderr,
					"ERROR: Failed to write process signature to %s in midwife_executable\n", pidfile);
			return -1;
		}

			// ensure the signature remains unique
		int sleepfor = computeWaitTime(&proc_sig, precision_range);
		while( (sleepfor = sleep(sleepfor)) != 0 );

			// generate a confirmation
		process_confirmation_t proc_conf;
		if( generateSignatureConfirmation(&proc_conf, &status) == UNIQ_PID_FAILURE ){
			uniq_pid_perror(status,
							"ERROR: Failed to generate a confirmation signature in midwife_executable\n");
			return -1;
		}

			// write the confirmation to the pid file
		if( writeConfirmation(fp, &proc_conf) == -1 ){
			fprintf(stderr,
					"ERROR: Failed to write process confirmation to %s in midwife_executable\n", pidfile);
			return -1;
		}

			// close the pid file
		fclose(fp);

			// success
		return( pid );

	}//endelse
  

		// if anything reaches here its a failure
	return -1;
}


