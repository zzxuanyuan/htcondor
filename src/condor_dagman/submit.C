#include "condor_common.h" /* for <stdio.h>,<stdlib.h>,<string.h>,<unistd.h> */

//
// Local DAGMan includes
//
#include "util.h"
#include "debug.h"
#include "submit.h"
#include "parser.h"

namespace dagman {

static bool submit_try (const std::string & exe,
                        const std::string & command,
                        CondorID & condorID) {
  
    FILE * fp = popen(command.c_str(), "r");
    if (fp == NULL) {
        debug_println (DEBUG_QUIET, "%s: popen() failed!", command.c_str());
        return false;
    }
  
    //----------------------------------------------------------------------
    // Parse Condor's return message for Condor ID.  This desperately
    // needs to be replaced by a Condor Submit API.
    //
    // Typical condor_submit output for Condor v6 looks like:
    //
    //   Submitting job(s).
    //   Logging submit event(s).
    //   1 job(s) submitted to cluster 2267.
    //----------------------------------------------------------------------

    Parser parser(fp);
    Parser::Result result = Parser::Result_OK;
    std::string token;
    
    while ( (result = parser.GetToken(token)) != Parser::Result_EOF ) {
        if (token == "cluster") {
            result = parser.GetToken(token);
            if (result == Parser::Result_OK) {
                // token should be of the form "2267."
                // strip off the '.'
                condorID._cluster =
                    atoi(std::string(token, 0, token.size()-1).c_str());
                if (DEBUG_LEVEL(DEBUG_DEBUG_2)) {
                    printf ("%s assigned condorID ", exe.c_str());
                    condorID.Print();
                    putchar('\n');
                }
            }
            break;
        }
    }
    if (pclose(fp) == -1) perror (command.c_str());
    return result != Parser::Result_EOF;
}

//-------------------------------------------------------------------------
bool submit_submit (const std::string & cmdFile, CondorID & condorID) {
    // the '-p' parameter to condor_submit will now cause condor_submit
    // to pause ~4 seconds after a successfull submit.  this prevents
    // the race condition of condor_submit finishing before dagman
    // does a pclose, which at least on SGI IRIX causes a nasty 'Broken Pipe'
    // [joev] - took this out on Todd's advice ....
  
    // const std::string exe = "condor_submit -p";
    const std::string exe = "condor_submit";
    std::string command = exe + ' ' + cmdFile + " 2>&1";
  
    bool success = false;
    const int tries = 6;
  
    for (int i = 1 ; i <= tries && !success ; i++) {
        success = submit_try (exe, command, condorID);
        if (!success) {
            const int wait = 10;
            debug_println (DEBUG_NORMAL, "condor_submit try %d/%d failed, "
                           "will try again in %d seconds", i, tries, wait);
            sleep(wait);
        }
    }
    if (!success && DEBUG_LEVEL(DEBUG_QUIET)) {
        printf ("condor_submit failed after %d tries.\n", tries);
        printf ("Submit command was: %s\n", command.c_str());
    }
    return success;
}

} // namespace dagman
