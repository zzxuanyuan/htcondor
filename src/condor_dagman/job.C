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
    printf ("Job -------------------------------------\n");
    printf ("          JobID: %d\n", m_jobID);
    printf ("       Job Name: %s\n", m_jobName.c_str());
    printf ("     Job Status: %s\n", status_t_names[m_Status].c_str());
    printf ("       Cmd File: %s\n", m_cmdFile.c_str());
    printf ("      Condor ID: ");
    m_CondorID.Print();
    putchar('\n');
  
    for (int i = 0 ; i < 3 ; i++) {
        printf ("%15s: ", queue_t_names[i].c_str());

        std::list<int>::const_iterator it;
        for (it = m_queues[i].begin() ; it != m_queues[i].end() ; it++)
            printf ("%d, ", *it);
        printf ("<END>\n");
    }
}

//---------------------------------------------------------------------------
void Job::Print (bool condorID) const {
    printf ("ID: %4d Name: %s", m_jobID, m_jobName.c_str());
    if (condorID) {
        printf ("  CondorID: ");
        m_CondorID.Print();
    }
}

//---------------------------------------------------------------------------
void job_print (Job * job, bool condorID) {
    if (job == NULL) printf ("(UNKNOWN)");
    else job->Print(condorID);
}

}
