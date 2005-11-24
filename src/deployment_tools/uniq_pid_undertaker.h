#ifndef UNIQ_PID_UNDERTAKER_H
#define UNIQ_PID_UNDERTAKER_H

//***
//constant static variables
//***
const static char* UNDERTAKER_CMD = "uniq_pid_undertaker";
const static char* UNDERTAKER_USAGE = "uniq_pid_undertaker [--block] [--file pidfile] [--precision seconds]\n";

//***
// function declarations
//***
int undertaker_main( int argc, char* argv[], 
					 const char* pidfile, bool blockflag, 
					 unsigned int precision_range );
#endif		
