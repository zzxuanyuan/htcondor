#ifndef UNIQ_PID_FILE_IO_H
#define UNIQ_PID_FILE_IO_H

#include "condor_common.h"
#include "uniq_pid_util.h"

//***
// function declarations
//***
FILE* createPidFile(const char* pidfile);
int readPidFile(const char* pidfile, 
		process_signature_t* proc_sig, bool* empty, 
		process_confirmation_t* proc_conf, bool* confirmed);
int writeProcessSignature(FILE* fp, process_signature_t* proc_sig);
int writeConfirmation(FILE* fp, process_confirmation_t* proc_conf);
int appendConfirmation(const char *pidfile, process_confirmation_t* proc_conf);

#endif
