#include "condor_common.h"
#include "condor_string.h"

// DAGMan Includes
#include "job.h"

namespace dagman {

//---------------------------------------------------------------------------
JobID_t Job::m_jobID_counter = 0;  // Initialize the static data memeber

//---------------------------------------------------------------------------
const std::string Job::queue_t_names[] = {
    "QUEUE_INCOMING",
    "QUEUE_WAITING ",
    "QUEUE_OUTGOING",
};

//---------------------------------------------------------------------------
const std::string Job::status_t_names[] = {
    "STATUS_READY    ",
    "STATUS_SUBMITTED",
    "STATUS_DONE     ",
    "STATUS_ERROR    ",
};

//---------------------------------------------------------------------------
Job::~Job() {}

//---------------------------------------------------------------------------
Job::Job (const std::string & jobName, const std::string & cmdFile):
    m_scriptPre  (NULL),
    m_scriptPost (NULL),
    m_Status     (STATUS_READY),
    m_cmdFile    (cmdFile),
    m_jobName    (jobName)
{
    // _condorID struct initializes itself

    // jobID is a primary key (a database term).  All should be unique
    m_jobID = m_jobID_counter++;
}

//---------------------------------------------------------------------------
void Job::Remove (const queue_t queue, const JobID_t jobID) {
    m_queues[queue].remove(jobID);
}  

//---------------------------------------------------------------------------
void Job::Dump () const {
    cout << "Job -------------------------------------"     << endl;
    cout << "          JobID: " << toString(m_jobID)        << endl;
    cout << "       Job Name: " << m_jobName                << endl;
    cout << "     Job Status: " << status_t_names[m_Status] << endl;
    cout << "       Cmd File: " << m_cmdFile                << endl;
    cout << "      Condor ID: " << m_CondorID               << endl;
  
    for (int i = 0 ; i < 3 ; i++) {
        fflush(stdout);
        printf ("%15s: ", queue_t_names[i].c_str());

        std::list<int>::const_iterator it;
        for (it = m_queues[i].begin() ; it != m_queues[i].end() ; it++)
            cout << toString(*it) << ", ";
        cout << "<END>" << endl;
    }
}

//---------------------------------------------------------------------------
std::string Job::toString (bool condorID) const {
    std::string s;
    s += "[id=";
    s += dagman::toString(m_jobID);  // this should be padded with 4 spaces
    s += "; name=\"";
    s += m_jobName;
    s += "\"";
    if (condorID) {
        s += "; condorID=\"";
        s += m_CondorID;
        s += "\"";
    }
    s += ']';
    return s;
}

//---------------------------------------------------------------------------
ostream & operator << (ostream & out, const Job & job) {
    out << (std::string) job;
    return out;
}

//---------------------------------------------------------------------------
std::string toString (const Job * const job, bool condorID) {
    if (job == NULL) return "(UNKNOWN)";
    else return job->toString(condorID);
}

} // namespace dagman
