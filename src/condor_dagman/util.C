#include "condor_common.h"   /* for <ctype.h>, <assert.h> */
#include "debug.h"
#include "types.h"
#include "util.h"

namespace dagman {

//-----------------------------------------------------------------------------
bool util_popen (const char * cmd, std::string * error) {
    FILE *fp;
    debug_println (DEBUG_VERBOSE, "Running: %s", cmd);
    fp = popen (cmd, "r");

    if (fp == NULL) {
        if (error) *error = "failed to run";
        return false;
    }

    int status = pclose(fp);
    
    if ( WIFSIGNALED(status) ) {
        if (error) {
            *error = "was terminated by signal ";
            *error += to_string( WTERMSIG(status) );
        }
        return false;
    }
    
    if ( WIFEXITED(status) ) {
        int val = WEXITSTATUS(status);
        if (val != 0) {
            if (error) {
                *error = "exited with value ";
                *error += to_string( WEXITSTATUS(status) );
            }
            return false;
        }
    }

    if ( WIFSTOPPED(status) ) {
        if (error) {
            *error = "was stopped by signal ";
            *error += to_string( WSTOPSIG(status) );
        }
        return false;
    }

    return true;
}

} // namespace dagman
