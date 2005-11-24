#include "condor_common.h"
#include "../condor_procapi/procapi.h"
#include "uniq_pid_undertaker.h"
#include "uniq_pid_tool_file_io.h"
#include "uniq_pid_util.h"

//***
//constant static variables
//***
const static int RESERVED_PIDS = 300;
const static int UNDERTAKER_FAILURE = -1;
const static int PROCESS_DEAD = 0;
const static int PROCESS_ALIVE = 1;
const static int PROCESS_UNCERTAIN = 2;


//***
// private function declarations
//***
int isAlive( process_signature_t* stored_proc_sig, 
			 unsigned int precision_range, 
			 process_confirmation_t* stored_proc_conf = 0);

int isAliveFromFile(const char* pidfile, unsigned int precision_range);
int blockUntilDead(const char* pidfile, unsigned int precision_range);
int handleUncertain(const char* pidfile);

/*
  Main function for the undertaker.  Determines whether the process
  associated with the given pid file is alive or dead.
  Returns 1 for alive and 0 for dead.  Can optionally block until
  the process dies.
*/
int 
undertaker_main( int argc, char* argv[], 
				 const char* pidfile, bool blockflag, 
				 unsigned int precision_range)
{
 
		// generate a confirmation before we determine the status
		// that way we don't confirm at an arbitrary time after we KNOW
		// the process is alive
	int status;
	process_confirmation_t* new_confirmation = new process_confirmation_t;
	if( generateSignatureConfirmation(new_confirmation, &status) == UNIQ_PID_FAILURE ){
		char* err_str = "WARNING: Could not generate a new confirmation in undertaker_main\n";
		uniq_pid_perror(status, err_str);
		delete new_confirmation;
		new_confirmation = NULL;
	}
  
		// see if the process is still alive
	int result =  isAliveFromFile(pidfile, precision_range);
  

		// ALIVE
	if( result == PROCESS_ALIVE ){
    
			// attach the confirmation if it exists
		if( new_confirmation != NULL ){
			if( appendConfirmation(pidfile, new_confirmation) == -1 ){
				fprintf(stderr, "WARNING: Could not append a new confirmation\n");
			}
			delete new_confirmation;
		}
    
			// block until dead if neccessary
		if( blockflag ){
			if( blockUntilDead(pidfile, precision_range) == -1 ){
				fprintf(stderr, "ERROR: Encountered an error while attempting to block until the process is dead\n");
				exit(UNDERTAKER_FAILURE);
			}
			exit(PROCESS_DEAD);
		}
    
			// or exit with "alive"
		else{
			exit(PROCESS_ALIVE);
		}
	}

		// DEAD
	else if( result == PROCESS_DEAD ){
			// exit with "dead" status
		exit(PROCESS_DEAD);
	}

		// UNCERTAIN
	else if( result == PROCESS_UNCERTAIN ){
			// even though the response is uncertain print out as much info as possible
		handleUncertain(pidfile);
			// exit with "uncertain" status
		exit(PROCESS_UNCERTAIN);
	}

		// ERROR
	else if( result == -1 ){
		fprintf(stderr,"ERROR: Failure occured while determining whether the process was alive from the pid file [%s]\n", pidfile);
		exit(UNDERTAKER_FAILURE);
	}

	else{
		fprintf(stderr,"ERROR: Unknown value [%i] returned from isAlive(...)\n", result);
		exit(UNDERTAKER_FAILURE);
	}

		// UNREACHABLE
	fprintf(stderr, "ERROR: Reached unreachable code, bailing out\n");
	exit(UNDERTAKER_FAILURE);
	return -1;
}

//////////////////////////////////////////////////////////////////////
// Helper Functions
//////////////////////////////////////////////////////////////////////

/*
  Determines whether the process represented by the given pid file
  is alive.
  Returns -1 on error
*/
int 
isAliveFromFile(const char* pidfile, unsigned int precision_range)
{
  
		// read in the pid file
	process_signature_t stored_proc_sig;
	process_confirmation_t stored_proc_conf;
	bool fileEmpty;
	bool signatureConfirmed;
	if( readPidFile(pidfile, &stored_proc_sig, &fileEmpty, 
					&stored_proc_conf, &signatureConfirmed) == -1 ){
		fprintf(stderr, "ERROR: Could not read pid file[%s]\n", pidfile);
		return -1;
	}

		// file was empty or unparseable
	if( fileEmpty ){
		fprintf(stderr, "ERROR: Pid file[%s] was empty or was not parseable\n", pidfile);
		return -1;
	}
  
		// see if the process represented by this signature is still alive
	int handback = -1;
	if( signatureConfirmed ){
		handback = isAlive(&stored_proc_sig, precision_range, &stored_proc_conf);
	} else{
		handback = isAlive(&stored_proc_sig, precision_range);
	}

	return( handback );
}

/*
  Determines whether the process represented by the given process
  signature is alive.
  Returns -1 on error
*/
int 
isAlive( process_signature_t* stored_proc_sig, 
			 unsigned int precision_range, 
			 process_confirmation_t* stored_proc_conf )
{

	int status;

		// ensure the signature has a valid pid
	if( stored_proc_sig->pid == UNIQ_PID_UNDEF ){
		return PROCESS_UNCERTAIN;
	}
  
		// query for the current signature for this pid
	process_signature_t stack_allocated_sig;
	process_signature_t* queried_proc_sig = &stack_allocated_sig;
	int result = generateProcessSignature( stored_proc_sig->pid, 
										   queried_proc_sig, 
										   &status);

		// error getting the signature
	if( result == UNIQ_PID_FAILURE && status != UNIQ_PID_NO_PID ){
		return UNDERTAKER_FAILURE;
	}
  
		// no matching pid, process is dead
	else if( result == UNIQ_PID_FAILURE && status == UNIQ_PID_NO_PID ){
		return PROCESS_DEAD;
	}

		// see if the processes are the same
	result = isSameProcess(stored_proc_sig, 
						   queried_proc_sig, 
						   precision_range, 
						   stored_proc_conf);

		// the processes are the same
	if( result == UNIQ_PID_SAME ){
		return PROCESS_ALIVE;
	} 
  
		// different
	else if( result == UNIQ_PID_DIFFERENT ){
		return PROCESS_DEAD;
	}
  
		// uncertain
	else if( result == UNIQ_PID_UNCERTAIN ){
		return PROCESS_UNCERTAIN;
	}
  
		// error occured
	return -1;
}
  
/*
  Blocks until the process associated with with the log is dead.
*/
int 
blockUntilDead(const char* pidfile, unsigned int precision_range)
{
  
		// continue checking until the process is dead
	unsigned tries = 0;
	int status = PROCESS_UNCERTAIN;
	while( status != PROCESS_DEAD ){

			//back off the appropriate amount
		exponentialBackoff(1, tries);
    
		status = isAliveFromFile(pidfile, precision_range);
		if( status == -1 ){
			fprintf(stderr, "ERROR: Could not determine process status in blockUntilDead(...)\n");
			return -1;
		}

		tries++;
	}
  
		//success
	return 0;
}

/*
  Handles the case where the undertaker is unable to determine
  whether a process is alive or dead.
  Attempts to print out information about the process so the
  user can decide.
*/
int 
handleUncertain(const char* pidfile)
{
		// read in the pid file
	process_signature_t stored_proc_sig;
	process_confirmation_t stored_proc_conf;
	bool fileEmpty;
	bool signatureConfirmed;
	if( readPidFile(pidfile, &stored_proc_sig, &fileEmpty, 
					&stored_proc_conf, &signatureConfirmed) == -1 ){
		return -1;
	}

		// file was empty or unparseable
	if( fileEmpty ){
		fprintf(stderr, 
				"Could not be certain of the processes status because pid file[%s] is empty or unparseable\n", 
				pidfile);
	}
  
		// no pid
	if( stored_proc_sig.pid == UNIQ_PID_UNDEF ){
		fprintf(stderr, 
				"Could not be certain of processes status because pid file[%s] did not contain the a pid\n", 
				pidfile);
	}

	else{
			// get information about the process from procapi
		piPTR pi = NULL;
		int status;
		int result = ProcAPI::getProcInfo( stored_proc_sig.pid, 
										   pi, status );
    
			// print it
		if( result != PROCAPI_FAILURE ){
			ProcAPI::printProcInfo(pi);
		} else{
			fprintf(stderr, 
					"Information about process currently unavailable\n");
		}
    
	}

		// success
	return 0;
}
