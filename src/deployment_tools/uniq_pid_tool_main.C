#include "condor_common.h"
#include "uniq_pid_undertaker.h"
#include "uniq_pid_midwife.h"

//***
//constant static variables
//***
// command variables
const static char* PID_FILE_OPT = "--file";
const static char* BLOCK_OPT = "--block";
const static char* NO_BLOCK_OPT = "--noblock";
const static char* PRECISION_OPT = "--precision";

// defaults
const static char* DEFAULT_PID_FILE = "pid.file";
const static int PRECISION_RANGE = 10;


/*
  Tool Main function
  Determines which tool the user executed and calls the appropriate
  function.
*/
int 
main(int argc, char* argv[])
{

		// extract the uniq_pid_main command and options 
		// from the target command and options
	char* command = argv[0];
	argc--;
	argv++;
  
		// remove the path, if its included in the command
	if( strchr(command, DIR_DELIM_CHAR) != NULL ){
		command = strrchr( command, DIR_DELIM_CHAR );
		command++;
	}
  
		// set the defaults
	const char* pidfile = DEFAULT_PID_FILE;
	unsigned int precision_range = PRECISION_RANGE;
	bool mblockflag = true;
	bool ublockflag = false;

		// continue to read the first opt until it 
		// doesn't match a uniq pid flag
	bool moreOpts = true;
	while( argc > 0 && moreOpts ){
    
			// Check for the pid file option
		if( strcmp(PID_FILE_OPT, argv[0]) == 0){
				// ensure there is an argument for the option
			if( argc < 2 ){
				fprintf(stderr, "ERROR: %s requires a file argument\n", PID_FILE_OPT);
				exit(-1);
			}//endif
    
				// extract the argument
			pidfile = argv[1];
			argc -= 2;
			argv += 2;
      
		}//endif

			// Check for the block argument
		else if( strcmp(BLOCK_OPT, argv[0]) == 0 ){
			mblockflag = true;
			ublockflag = true;
			argc--;
			argv++;
		}

			// Check for the no block argument
		else if( strcmp(NO_BLOCK_OPT, argv[0]) == 0 ){
			mblockflag = false;
			ublockflag = false;
			argc--;
			argv++;
      
		}

			// Check for the precision argument
		else if( strcmp(PRECISION_OPT, argv[0]) == 0 ){
      
				// ensure the next value is a number
			int i = 0;
			while( argv[1][i] != NULL ){
				if( !isdigit(argv[1][i]) ){
					fprintf(stderr, "ERROR: %s requires a time in seconds\n", PRECISION_OPT);
					exit(-1);
				}
	    
				i++;
			}
      
				// get the precision
			precision_range = strtoul(argv[1], NULL, 0);

			argc -= 2;
			argv += 2;
		}

			// No more options
		else{
			moreOpts = false;
		}
    
	}//endwhile

		// determine the command executed
		// and call the appropriate function
	if( strcmp(command, MIDWIFE_CMD) == 0 ){
		midwife_main(argc, argv, pidfile, mblockflag, precision_range);
	} else if( strcmp(command, UNDERTAKER_CMD) == 0 ){
		undertaker_main(argc, argv, pidfile, ublockflag, precision_range);
	} else{
		fprintf(stderr, "ERROR: Could not recognize command [%s]\n", command);
		exit(-1);
	}

		// should never reach this point
	fprintf(stderr, "ERROR: Reached unreachable code, bailing out\n");
	return -1;
}
