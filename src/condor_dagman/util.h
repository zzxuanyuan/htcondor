#ifndef _UTIL_H_
#define _UTIL_H_

#include <string>
#include "condor_system.h"   /* for <stdio.h> */

namespace dagman {

/** Execute a command, printing verbose messages and failure warnings.
    @param cmd The command or script to execute
    @return The return status of the command
*/
extern "C" int util_popen (const char * cmd);

} // namespace dagman

#endif /* #ifndef _UTIL_H_ */
