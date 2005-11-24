#include "uniq_pid_tool_file_io.h"

//***
//constant static variables
//***

// pid file headers
const static char* CONFIRM_HEADER = "CONFIRM";

//pid file entry formats
const static char* SIGNATURE_FORMAT = "PPID = %i\nPID = %i\nBDAY = %li\nCONTROL_TIME = %li\n";
const static int NR_OF_SIGNATURE_ENTRIES = 4;
const static char* CONFIRMATION_FORMAT = "CONFIRM = %li\nCONTROL_TIME = %li\n";
const static int NR_OF_CONFIRM_ENTRIES = 2;

//***
// helper function declarations
//***
int extractSignature(char* inbuffer, process_signature_t* proc_sig);
int extractConfirmation(char* inbuffer, process_confirmation_t* proc_conf);

/*
  Creates the pid file
  Ensures we can create and write to it.
*/
FILE* 
createPidFile(const char* pidfile)
{

		// create and open the file
  mode_t pidfile_mode = 
    S_IRUSR | S_IWUSR | // user read/write
    S_IRGRP | S_IWGRP | // group read/write
    S_IROTH | S_IWOTH;  // everyone read/write
  int fd = open(pidfile, O_WRONLY | O_CREAT | O_EXCL, pidfile_mode);
  if( fd == -1 ){
    perror("ERROR: Could not create the pid file");
    return NULL;
  }

	  // convert to a file pointer
	  // ALL THIS NONSENSE BECAUSE fopen DOESN'T HAVE O_EXCL
  FILE* fp = fdopen(fd, "w");
  if( fp == NULL ){
    perror("ERROR: Could not convert the pid file file descriptor to a file pointer");
    return( NULL );
  }
  
	  //success
  return fp;
}


/*
  Reads the pid file contents into the given structures.
  Sets empty to true if the file was empty.
  Sets confirmed if the pid file has a confirmation entry and the
  process_confirmation_t structure has been filled.
  
  Programmers Note:
  This function reads in the entire file before attempting to do 
  any matching.  This is due to the uncertain position fscanf may
  leave the file pointer in when matching fails and my desire
  to jump over arbitrary data until I come to a good confirmation 
  header.
*/
int 
readPidFile(const char* pidfile, 
				process_signature_t* proc_sig, bool* empty, 
				process_confirmation_t* proc_conf, bool* confirmed)
{
  
		// assume we will get data
	*empty = false;  

		// clear the return structures
	clearProcessSignature(proc_sig);
	clearProcessConfirmation(proc_conf);

		// get the files size
	struct stat pidfile_stat;
	if( stat(pidfile, &pidfile_stat) == -1 ){
		perror("ERROR: Could not stat the pid file in readPidFile(...)");
		return -1;
	}	

		// ensure the file has data
	if( pidfile_stat.st_size == 0 ){
		*empty = true;
		return 0;
	}

		// open the file
	int fd = open(pidfile, O_RDONLY);
	if( fd == -1 ){
		perror("ERROR: Could not open the pid file in readPidFile(...)");
		return -1;
	}
  
		// read in the data
	char* inbuffer = new char[pidfile_stat.st_size];
	int bytesRead = read(fd, inbuffer, pidfile_stat.st_size);
	if( bytesRead == -1 ){
		perror("ERROR: Could not read from pid file in readPidFile(...)");
		return -1;
	}

		// close the file
	close(fd);

		// extract the signature
	int nr_extracted = extractSignature(inbuffer, proc_sig);
    
		// ensure we got something
	if( nr_extracted == 0 ){
		fprintf(stderr, "ERROR: Failed to match any entries of %s\n", pidfile);
		*empty = true;
	}
  
		// attempt to get the latest confirmation
	*confirmed = false;
	if( nr_extracted == NR_OF_SIGNATURE_ENTRIES ){

		char* remaining_buffer = strstr(inbuffer, CONFIRM_HEADER);
		process_confirmation_t temp_conf;
		while( remaining_buffer != NULL ){

				// extract the confirmation
			nr_extracted = extractConfirmation(remaining_buffer, &temp_conf);

				// only keep the confirmation if it is a complete entry
			if( nr_extracted == NR_OF_CONFIRM_ENTRIES ){
				memcpy(proc_conf, &temp_conf, sizeof(process_confirmation_t));
				*confirmed = true;
			}
	
				// increment the buffer
			remaining_buffer++;
			remaining_buffer = strstr(remaining_buffer, CONFIRM_HEADER);
		}
    
	}

		// cleanup
	delete[] inbuffer;
  
		//success
	return 0;
}

/*
  Writes the process signature to the given file pointer.
*/
int 
writeProcessSignature(FILE* fp, process_signature_t* proc_sig)
{
    int retval = fprintf(fp, 
						 SIGNATURE_FORMAT,
						 proc_sig->ppid,
						 proc_sig->pid,
						 proc_sig->bday,
						 proc_sig->control_time);

    if( retval < 0 ){
		perror("ERROR: Could not write the process signature");
		return -1;
    }

		// flush the write
    fflush(fp);

		// success
    return 0;
}

/*
  Writes the confirmation to the given file pointer.
*/
int 
writeConfirmation(FILE* fp, process_confirmation_t* proc_conf)
{
	int retval = fprintf(fp,
						 CONFIRMATION_FORMAT,
						 proc_conf->confirm_time,
						 proc_conf->control_time);

		// error
	if( retval < 0 ){
		perror("ERROR: Could not write the confirmation");
		return -1;
	}

		//flush the write
	fflush(fp);
  
		// success
	return 0;
}

/*
  Appends a confirmation to the end of the pid file.
*/
int 
appendConfirmation(const char *pidfile, process_confirmation_t* proc_conf)
{

		// open the file
	int fd = open(pidfile, O_WRONLY | O_APPEND);
	if( fd == -1 ){
		perror("ERROR: Could not open the pid file");
		return -1;
	}
  
		// convert to a file pointer
		// ALL THIS NONSENSE BECASE fopen DOESN'T HAVE O_CREAT
	FILE* fp = fdopen(fd, "a");
	if( fp == NULL ){
		perror("ERROR: Could not convert pid file from file descriptor to a file pointer");
		return -1;
	}

		// write the confirmation
	if( writeConfirmation(fp, proc_conf) == -1 ){
		fprintf(stderr, 
				"ERROR: Failed to write confirmation in appendConfirmation(...)");
		return -1;
	}

		// flush the write
	fflush(fp);

		// close the file
	fclose(fp);

		// sucess
	return 0;
}

//////////////////////////////////////////////////////////////////////
// Helper Functions
//////////////////////////////////////////////////////////////////////

/*
  Fills the process signature structure from the buffer.
  The buffer must be in SIGNATURE_FORMAT.
*/
int 
extractSignature(char* inbuffer, process_signature_t* proc_sig)
{
  
	int nr_extracted = sscanf( inbuffer, 
							   SIGNATURE_FORMAT, 
							   &(proc_sig->ppid),
							   &(proc_sig->pid),
							   &(proc_sig->bday),
							   &(proc_sig->control_time) );

	return( nr_extracted );
}

/*
  Fills the process confirmation structure from the buffer.
  The buffer must be in the form of CONFIRMATION_FORMAT
*/
int 
extractConfirmation(char* inbuffer, process_confirmation_t* proc_conf)
{

	int nr_extracted = sscanf( inbuffer, 
							   CONFIRMATION_FORMAT,
							   &(proc_conf->confirm_time),
							   &(proc_conf->control_time) );

	return( nr_extracted );
}
