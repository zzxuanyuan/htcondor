#ifndef JOB_H
#define JOB_H

#include <list>
#include <string>

//
// Local DAGMan includes
//
#include "types.h"
#include "debug.h"
#include "script.h"

namespace dagman {

/**  A Job instance will be used to pass job attributes to the
     AddJob() function.
*/
class Job {
  public:
  
    /** Enumeration for specifying which queue for Add() and Remove().
        If you change this enum, you *must* also update queue_t_names
    */
    enum queue_t { Q_INCOMING, Q_WAITING, Q_OUTGOING };

    /// The string names for the queue_t enumeration
    static const std::string queue_t_names[];
  
    /** The Status of a Job
        If you update this enum, you *must* also update status_t_names
    */
    enum status_t {
        /** Job is ready */               STATUS_READY,
        /** Job has been submitted */     STATUS_SUBMITTED,
        /** Job is done */                STATUS_DONE,
        /** Job exited abnormally */      STATUS_ERROR
    };

    /// The string names for the status_t enumeration
    static const std::string status_t_names[];
  
    /** Constructor
        @param jobName Name of job in dag file.  String is deep copied.
        @param cmdFile Path to condor cmd file.  String is deep copied.
    */
    Job (const std::string & jobName, const std::string & cmdFile);
  
    ///
    ~Job();
  
    /** */ inline std::string GetJobName () const { return m_jobName; }
    /** */ inline std::string GetCmdFile () const { return m_cmdFile; }
    /** */ inline JobID_t     GetJobID   () const { return m_jobID;   }

    inline operator std::string() const { return toString(); }

    /** Create string representation of this job
        @param condorID Indicates whether the condorID be included
     */
    std::string toString (bool condorID = false) const;

    Script * m_scriptPre;
    Script * m_scriptPost;

    ///
    inline std::list<JobID_t> & GetQueueRef (const queue_t queue) {
        return m_queues[queue];
    }

    /** Add a job to one of the queues.  Adds the job with ID jobID to
        the incoming, outgoing, or waiting queue of this job.
        @param jobID ID of the job to be added.  Should not be this job's ID
        @param queue The queue to add the job to
        @return true: success, false: failure (lack of memory)
    */
    inline bool Add (const queue_t queue, const JobID_t jobID) {
        m_queues[queue].push_back(jobID);
        return true;
    }

    /** Returns true if this job is ready for submittion.
        @return true if job is submitable, false if not
    */
    inline bool CanSubmit () const {
        return (IsEmpty(Job::Q_WAITING) && m_Status == STATUS_READY);
    }

    /** Remove a job from one of the queues.  Removes the job with ID
        jobID from the incoming, outgoing, or waiting queue of this job.
        @param jobID ID of the job to be removed.  Should not be this job's ID
        @param queue The queue to add the job to
        @return true: success, false: failure (jobID not found in queue)
    */
    void Remove (const queue_t queue, const JobID_t jobID);

    ///
    inline bool IsEmpty (const queue_t queue) const {
        return m_queues[queue].empty();
    }
 
    /** Dump the contents of this Job to stdout for debugging purposes.
        @param level Only do the dump if the current debug level is >= level
    */
    void Dump () const;
    
    /** */ CondorID m_CondorID;
    /** */ status_t m_Status;
  
  private:
  
    /// filename of condor submit file
    std::string m_cmdFile;
  
    /// name given to the job by the user
    std::string m_jobName;
  
    /** Job queue's
      
        incoming -> dependencies coming into the Job
        outgoing -> dependencies going out of the Job
        waiting -> Jobs on which the current Job is waiting for output 
    */
    std::list<JobID_t> m_queues[3];
  
    /** The ID of this job.  This serves as a primary key for Job's, where each
        Job's ID is unique from all the rest
    */
    JobID_t m_jobID;

    /** Ensures that all jobID's are unique.  Starts at zero and increments
        by one for every Job object that is constructed
    */
    static JobID_t m_jobID_counter;
};

ostream & operator << (ostream & out, const Job & job);
std::string toString (const Job * const job, bool condorID = false);

} // namespace dagman

#endif /* ifndef JOB_H */
