#ifndef UNIQ_PID_MIDWIFE_H
#define UNIQ_PID_MIDWIFE_H

//***
//constant static variables
//***
const static char* MIDWIFE_CMD = "uniq_pid_midwife";
const static char* MIDWIFE_USAGE = "uniq_pid_midwife [--noblock] [--file file] [--precision seconds] <program> [program_args]\n";


//***
// function declarations
//***
int midwife_main(int argc, char* argv[], const char* pidfile, bool blockflag, unsigned int precision_range);

#endif
