#ifndef _SCRIPT_H_
#define _SCRIPT_H_

#include <string>

namespace dagman {

class Job;

class Script {
  public:
    /// True is this script is a POST script, false if a PRE script
    bool m_post;

    /// Return value of the script
    int  m_retValScript;

    /// Return value of the job run.  Only valid of POST script
    int  m_retValJob;

    /// Has this script been logged?
    bool   m_logged;

    /** Runs the script and sets m_retValScript.
        @return returns m_retValScript
    */
    int Run ();

    ///
    inline std::string GetCmd () const { return m_cmd; }

    ///
    Script (bool post, const std::string & cmd, Job * job);

    ///
    ~Script();

  protected:

    ///
    Job  * m_job;

    ///
    std::string m_cmd;
};

} // namespace dagman

#endif
