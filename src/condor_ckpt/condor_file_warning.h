
#ifndef CONDOR_FILE_WARNING_H
#define CONDOR_FILE_WARNING_H

#include "condor_common.h"

/**
A printf-like function to display an unsupported or
unsafe operation.  Depending on the image
mode and the syscall mode, we may want
to send the message different places.  In the
usual condor universe, this will cause a message
to be sent back to the user's email.  In the
standalone checkpointing world or when LocalSysCalls
is in effect, this will just put a message to stderr.
*/

extern "C" void _condor_file_warning( char *format, ... );

#endif
