#include "condor_common.h"   /* for <ctype.h>, <assert.h> */
#include "debug.h"
#include "util.h"

namespace dagman {

//-----------------------------------------------------------------------------
int util_popen (const char * cmd) {
    FILE *fp;
    debug_println (DEBUG_VERBOSE, "Running: %s", cmd);
    fp = popen (cmd, "r");
    int r;
    if (fp == NULL || (r = pclose(fp)) != 0) {
        if (DEBUG_LEVEL(DEBUG_NORMAL)) {
            printf ("WARNING: failure: %s", cmd);
            if (fp != NULL) printf (" returned %d", r);
            putchar('\n');
        }
    }
    return r;
}

} // namespace dagman
