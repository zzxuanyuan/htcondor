#include "condor_common.h"
#include "condor_string.h"

#include "script.h"
#include "util.h"
#include "job.h"
#include "types.h"

namespace dagman {

//-----------------------------------------------------------------------------
Script::~Script () {
    // Don't delete the m_job pointer!
}

//-----------------------------------------------------------------------------
Script::Script (bool post, const std::string & cmd, Job * job) :
    m_post         (post),
    m_retValJob    (-1),
    m_logged       (false),
    m_job          (job),
    m_cmd          (cmd)
{
}

//-----------------------------------------------------------------------------
bool Script::Run () {
    const char *delimiters = " \t";
    char * token;
    std::string send;
    char * cmd = strnewp(m_cmd.c_str());
    debug_printf (DEBUG_DEBUG_3, "About to parse: %s\n", cmd);
    for (token = strtok (cmd,  delimiters) ; token != NULL ;
         token = strtok (NULL, delimiters)) {
        if      (!strcasecmp(token, "$LOG"   )) send += m_logged ? '1' : '0';
        else if (!strcasecmp(token, "$JOB"   )) send += m_job->GetJobName();
        else if (!strcasecmp(token, "$RETURN")) send += to_string(m_retValJob);
        else                                    send += token;

        send += ' ';
    }
    delete [] cmd;
    debug_printf (DEBUG_DEBUG_3, "Parsed: %s\n", send.c_str());

    std::string error;

    if (!util_popen (send.c_str(), & error)) {
        if (DEBUG_LEVEL(DEBUG_QUIET)) {
            printf ("%s Script of Job ", m_post ? "POST" : "PRE");
            m_job->Print();
            printf (" %s\n", error.c_str());
        }
        return false;
    }

    return true;
}

} // namespace dagman
