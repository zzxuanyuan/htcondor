#include "condor_common.h"
#include "condor_string.h"  /* for strnewp() */

//
// Local DAGMan includes
//
#include "debug.h"
#include "submit.h"
#include "util.h"
#include "dag.h"

namespace dagman {

//---------------------------------------------------------------------------
ostream & operator << (ostream & out, const std::list<JobID_t> & jobs) {
    std::list<JobID_t>::const_iterator it;
    out << '(';
    for (it = jobs.begin() ; it != jobs.end() ; it++) out << (*it) << ',';
    cout << "<end>)";
    return out;
}

//---------------------------------------------------------------------------
void TQI::Print () const {
    printf ("Job: ");
    job_print(parent);
    printf (", Children: ");
    cout << children;
    // children.Display(cout);
}

//---------------------------------------------------------------------------
Dag::Dag(const std::string & condorLogName, const std::string & lockFileName,
         const int numJobsRunningMax) :
    m_condorLogName        (condorLogName),
    m_condorLogInitialized (false),
    m_condorLogSize        (0),
    m_lockFileName         (lockFileName),
    m_termQLock            (false),
    m_numJobsDone          (0),
    m_numJobsFailed        (0),
    m_numJobsRunning       (0),
    m_numJobsRunningMax    (numJobsRunningMax)
{
}

//-------------------------------------------------------------------------
Dag::~Dag() {
    unlink(m_lockFileName.c_str()); // remove the file being used as semaphore
}

//-------------------------------------------------------------------------
bool Dag::Bootstrap (bool recovery) {
    //--------------------------------------------------
    // Add a virtual dependency for jobs that have no
    // parent.  In other words, pretend that there is
    // an invisible God job that has the orphan jobs
    // as its children.  (Ignor the phyllisophical ramifications).
    // The God job will be the value (Job *) NULL, which is
    // fitting, since the existance of god is an unsolvable concept
    //--------------------------------------------------

    assert (!m_termQLock);
    m_termQLock = true;

    {
        TQI * god = new TQI;   // Null parent and empty children list

        std::list<Job *>::iterator it;
        for (it = m_jobs.begin() ; it != m_jobs.end() ; it++) {
            if ((*it)->IsEmpty(Job::Q_INCOMING)) {
                god->children.push_back ((*it)->GetJobID());
            }
        }

        m_termQ.push_back (god);
    }
    
     m_termQLock = false;
    
     //--------------------------------------------------
     // Update dependencies for pre-terminated jobs
     // (jobs marks DONE in the dag input file)
     //--------------------------------------------------
     std::list<Job *>::iterator it;
     for (it = m_jobs.begin() ; it != m_jobs.end() ; it++) {
         if ((*it)->m_Status == Job::STATUS_DONE) TerminateJob(*it);
     }
    
     debug_println (DEBUG_VERBOSE, "Number of Pre-completed Jobs: %d",
                    NumJobsDone());
    
     if (recovery) {
         debug_println (DEBUG_NORMAL, "Running in RECOVERY mode...");
         if (!ProcessLogEvents (recovery)) return false;
     }
     return SubmitReadyJobs();
}

//-------------------------------------------------------------------------
bool Dag::AddDependency (Job * parent, Job * child) {
    assert (parent != NULL);
    assert (child  != NULL);
    
    if (!parent->Add (Job::Q_OUTGOING,  child->GetJobID())) return false;
    if (!child->Add  (Job::Q_INCOMING, parent->GetJobID())) return false;
    if (!child->Add  (Job::Q_WAITING,  parent->GetJobID())) return false;
    return true;
}

//-------------------------------------------------------------------------
Job * Dag::GetJob (const JobID_t jobID) const {
    std::list<Job *>::const_iterator it;
    for (it = m_jobs.begin() ; it != m_jobs.end() ; it++) {
        if ((*it)->GetJobID() == jobID) return *it;
    }
    return NULL;
}

//-------------------------------------------------------------------------
bool Dag::DetectLogGrowth (int checkInterval) {
    
    debug_printf (DEBUG_DEBUG_4, "%s: checkInterval=%d -- ", __FUNCTION__,
                  checkInterval);
    
    if (!m_condorLogInitialized) {
        m_condorLog.initialize(m_condorLogName.c_str());
        m_condorLogInitialized = true;
    }
    
    int fd = m_condorLog.getfd();
    assert (fd != 0);
    struct stat buf;
    sleep (checkInterval);
    
    if (fstat (fd, & buf) == -1)
        debug_perror (2, DEBUG_QUIET, m_condorLogName.c_str());
    
    int oldSize = m_condorLogSize;
    m_condorLogSize = buf.st_size;
    
    bool growth = (buf.st_size > oldSize);
    debug_printf (DEBUG_DEBUG_4, "%s\n", growth ? "GREW!" : "No growth");
    return growth;
}

//-------------------------------------------------------------------------
bool Dag::ProcessLogEvents (bool recovery) {
    
    if (!m_condorLogInitialized) {
        m_condorLogInitialized =
            m_condorLog.initialize(m_condorLogName.c_str());
    }
    
    bool done = false;  // Keep scaning until ULOG_NO_EVENT
    bool result = true;
    static unsigned int log_unk_count;

    while (!done) {
        
        ULogEvent * e;  // refer to condor_event.h
        ULogEventOutcome outcome = m_condorLog.readEvent(e);
        
        CondorID condorID;
        if (e != NULL) condorID = CondorID (e->cluster, e->proc, e->subproc);
        
        debug_printf (DEBUG_VERBOSE, " Log outcome: %s",
                      ULogEventOutcomeNames[outcome]);
        
        if (outcome != ULOG_UNK_ERROR) log_unk_count = 0;

        switch (outcome) {
            
            //----------------------------------------------------------------
          case ULOG_NO_EVENT:      
            debug_printf (DEBUG_VERBOSE, "\n");
            done = true;
            break;
            //----------------------------------------------------------------
          case ULOG_RD_ERROR:
            debug_printf (DEBUG_QUIET, "  ERROR: failure to read log\n");
            done   = true;
            result = false;
            break;
            //----------------------------------------------------------------
          case ULOG_UNK_ERROR:
            log_unk_count++;
            if (recovery || log_unk_count >= 5) {
                debug_printf (DEBUG_QUIET, "  ERROR: Unknown log error");
                result = false;
            }
            debug_printf (DEBUG_VERBOSE, "\n");
            done   = true;
            break;
            //----------------------------------------------------------------
          case ULOG_OK:
            
            if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
                putchar (' ');
                condorID.Print();
                putchar (' ');
            }
            
            if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
                printf ("  Event: %s ", ULogEventNumberNames[e->eventNumber]);
                printf ("for Job ");
            }
            
            switch(e->eventNumber) {
                
                //--------------------------------------------------
              case ULOG_EXECUTABLE_ERROR:
              case ULOG_JOB_ABORTED:
              {
                  Job * job = GetJob (condorID);
                  
                  if (DEBUG_LEVEL(DEBUG_VERBOSE)) job_print (job,true);
                  
                  // If this is one of our jobs, then we must inform the user
                  // that UNDO is not yet handled
                  if (job != NULL) {
                      if (DEBUG_LEVEL(DEBUG_QUIET)) {
                          printf ("\n------------------------------------\n");
                          job->Print(true);
                          printf (" resulted in %s.\n"
                                  "This version of Dagman does not support "
                                  "job resubmition, so this DAG must be "
                                  "aborted.\n"
                                  "Version 2 of Dagman will support job UNDO "
                                  "so that an erroneous job can be "
                                  "resubmitted\n"
                                  "while the dag is still running.\n",
                                  ULogEventNumberNames[e->eventNumber]);
                      }
                      done   = true;
                      result = false;
                  }
              }
               break;
              
              case ULOG_CHECKPOINTED:
              case ULOG_JOB_EVICTED:
              case ULOG_IMAGE_SIZE:
              case ULOG_SHADOW_EXCEPTION:
              case ULOG_GENERIC:
              case ULOG_EXECUTE:
                if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
                    Job * job = GetJob (condorID);
                    job_print(job,true);
                    putchar ('\n');
                }
                break;
                
                //--------------------------------------------------
              case ULOG_JOB_TERMINATED:
              {
                  Job * job = GetJob (condorID);
                  
                  if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
                      job_print(job, true);
                      putchar ('\n');
                  }
                  
                  if (job == NULL) {
                      debug_println (DEBUG_QUIET,
                                     "Unknown terminated job found in log");
                      done   = true;
                      result = false;
                      break;
                  }

                  JobTerminatedEvent * termEvent = (JobTerminatedEvent*) e;

                  //
                  // Execute a post script if it exists
                  //
                  if (job->m_scriptPost != NULL) {
                      job->m_scriptPost->m_retValJob = termEvent->normal
                                                     ? termEvent->returnValue
                                                       : -1;
                      int ret = job->m_scriptPost->Run();
                      if (ret != 0) {
                          job->m_Status = Job::STATUS_ERROR;
                          if (DEBUG_LEVEL(DEBUG_QUIET)) {
                              printf ("POST Script of Job ");
                              job->Print();
                              printf (" failed with status %d\n", ret);
                          }
                          done   = true;
                      }
                  }

                  //
                  // If the job terminated abnormally, abort DAGMan
                  //
                  if (! (termEvent->normal &&
                         termEvent->returnValue == 0)) {
                      job->m_Status = Job::STATUS_ERROR;
                      if (DEBUG_LEVEL(DEBUG_QUIET)) {
                          printf ("Job ");
                          job_print(job,true);
                          printf (" terminated with ");
                          if (termEvent->normal) {
                              printf ("status %d.", termEvent->returnValue);
                          } else {
                              printf ("signal %d.", termEvent->signalNumber);
                          }
                          putchar ('\n');
                      }
                      done   = true;
                  } else {
                      job->m_Status = Job::STATUS_DONE;
                      TerminateJob(job);
                      if (DEBUG_LEVEL(DEBUG_DEBUG_1)) Print_TermQ();
                      if (!recovery) {
                          debug_printf (DEBUG_VERBOSE, "\n");
                          if (SubmitReadyJobs() == false) {
                              done   = true;
                              result = false;
                          }
                      }
                  }
                  if (job->m_Status == Job::STATUS_ERROR) m_numJobsFailed++;
              }
               break;
              
               //--------------------------------------------------
              case ULOG_SUBMIT:
                
                // search Job List for next eligible
                // Job. This will get updated with the CondorID info
                Job * job = GetSubmittedJob(recovery);
                
                if (job == NULL) {
                    debug_println (DEBUG_QUIET,
                                   "Unknown submitted job found in log");
                    done = true;
                    result = false;
                    break;
                } else {
                    job->m_CondorID = condorID;
                    if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
                        job_print (job, true);
                        putchar ('\n');
                    }
                    if (!recovery) {
                        debug_printf (DEBUG_VERBOSE, "\n");
                        if (SubmitReadyJobs() == false) {
                            done   = true;
                            result = false;
                        }
                    }
                }
                
                if (DEBUG_LEVEL(DEBUG_DEBUG_1)) Print_TermQ();
                break;
            }
            break;
        }
    }
    if (DEBUG_LEVEL(DEBUG_VERBOSE) && recovery) {
        printf ("    -----------------------\n");
        printf ("       Recovery Complete\n");
        printf ("    -----------------------\n");
    }
    return result;
}

//---------------------------------------------------------------------------
Job * Dag::GetJob (const std::string & jobName) const {
    std::list<Job *>::const_iterator it;
    for (it = m_jobs.begin() ; it != m_jobs.end() ; it++) {
        if (jobName == (*it)->GetJobName()) return *it;
    }
    return NULL;
}

//---------------------------------------------------------------------------
Job * Dag::GetJob (const CondorID condorID) const {
    std::list<Job *>::const_iterator it;
    for (it = m_jobs.begin() ; it != m_jobs.end() ; it++) {
        if ((*it)->m_CondorID == condorID) return *it;
    }
    return NULL;
}

//-------------------------------------------------------------------------
bool Dag::Submit (Job * job) {
    assert (job != NULL);

    if (job->m_scriptPre != NULL) {
        int ret = job->m_scriptPre->Run();
        if (ret != 0) {
            if (DEBUG_LEVEL(DEBUG_QUIET)) {
                printf ("PRE Script of Job ");
                job->Print();
                printf (" failed with status %d\n", ret);
            }
            job->m_Status = Job::STATUS_ERROR;
            m_numJobsFailed++;
            return true;
        }
    }

    if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
        printf ("Submitting JOB ");
        job->Print();
    }
  
    CondorID condorID(0,0,0);
    if (!submit_submit (job->GetCmdFile(), condorID)) {
        job->m_Status = Job::STATUS_ERROR;
        m_numJobsFailed++;
        return true;
    }

    job->m_Status = Job::STATUS_SUBMITTED;

    m_numJobsRunning++;
    assert (m_numJobsRunningMax == 0 ||
            m_numJobsRunning <= m_numJobsRunningMax);

    if (DEBUG_LEVEL(DEBUG_VERBOSE)) {
        printf (", ");
        condorID.Print();
        putchar('\n');
    }

    return true;
}

//---------------------------------------------------------------------------
void Dag::PrintJobList() const {
    printf ("Dag Job List:\n");

    std::list<Job *>::const_iterator it;
    for (it = m_jobs.begin() ; it != m_jobs.end() ; it++) {
        printf ("---------------------------------------");
        (*it)->Dump();
        putchar ('\n');
    }
    printf ("---------------------------------------");
    printf ("\t<END>\n");
}

//---------------------------------------------------------------------------
void Dag::Print_TermQ () const {
    printf ("Termination Queue:");
    if (m_termQ.empty()) {
        printf (" <empty>\n");
        return;
    } else putchar('\n');

    std::list<TQI *>::const_iterator it;

    for (it = m_termQ.begin() ; it != m_termQ.end() ; it++) {
        printf ("  ");
        (*it)->Print();
        putchar ('\n');
    }
}

//---------------------------------------------------------------------------
void Dag::RemoveRunningJobs () const {
    std::string cmd;
    unsigned int jobs = 0;  // Number of jobs appended to cmd so far

    std::list<Job *>::const_iterator it;
    for (it = m_jobs.begin() ; it != m_jobs.end() ; it++) {
        if (jobs == 0) cmd = "condor_rm";

        if ((*it)->m_Status == Job::STATUS_SUBMITTED) {
            cmd += ' ';
            cmd += to_string((*it)->m_CondorID._cluster);
            jobs++;
        }

        if (jobs > 0 && cmd.size() >= ARG_MAX - 10) {
            util_popen (cmd.c_str());
            jobs = 0;
        }
    }

    if (jobs > 0) {
        util_popen (cmd.c_str());
        jobs = 0;
    }
}

//-----------------------------------------------------------------------------
void Dag::Rescue (const std::string & rescue_file,
                  const std::string & datafile) const {
    FILE *fp = fopen(rescue_file.c_str(), "w");
    if (fp == NULL) {
        debug_println (DEBUG_QUIET,
                       "Could not open %s for writing.", rescue_file.c_str());
        return;
    }

    fprintf (fp, "# Rescue DAG file, created after running\n");
    fprintf (fp, "#   the %s DAG file\n", datafile.c_str());
    fprintf (fp, "#\n");
    fprintf (fp, "# Total number of jobs: %d\n", NumJobs());
    fprintf (fp, "# Jobs premarked DONE: %d\n", m_numJobsDone);
    fprintf (fp, "# Jobs that failed: %d\n", m_numJobsFailed);

    //
    // Print the names of failed Jobs
    //
    fprintf (fp, "#   ");
    std::list<Job *>::const_iterator it;
    for (it = m_jobs.begin() ; it != m_jobs.end() ; it++) {
        if ((*it)->m_Status == Job::STATUS_ERROR) {
            fprintf (fp, "%s,", (*it)->GetJobName().c_str());
        }
    }
    fprintf (fp, "<ENDLIST>\n\n");

    //
    // Print JOBS and SCRIPTS
    //
    for (it = m_jobs.begin() ; it != m_jobs.end() ; it++) {
        fprintf (fp, "JOB %s %s %s\n", (*it)->GetJobName().c_str(),
                 (*it)->GetCmdFile().c_str(),
                 (*it)->m_Status == Job::STATUS_DONE ? "DONE" : "");
        if ((*it)->m_scriptPre != NULL) {
            fprintf (fp, "SCRIPT PRE  %s %s\n", (*it)->GetJobName().c_str(),
                     (*it)->m_scriptPre->GetCmd().c_str());
        }
        if ((*it)->m_scriptPre != NULL) {
            fprintf (fp, "SCRIPT POST %s %s\n", (*it)->GetJobName().c_str(),
                     (*it)->m_scriptPost->GetCmd().c_str());
        }
    }

    //
    // Print Dependency Section
    //
    fprintf (fp, "\n");
    for (it = m_jobs.begin() ; it != m_jobs.end() ; it++) {
        std::list<JobID_t> & queue = (*it)->GetQueueRef(Job::Q_OUTGOING);
        if (!queue.empty()) {
            fprintf (fp, "PARENT %s CHILD", (*it)->GetJobName().c_str());
            
            std::list<JobID_t>::const_iterator jobIDit;
            for (jobIDit = queue.begin() ; jobIDit != queue.end() ;
                 jobIDit++) {
                Job * child = GetJob(*jobIDit);
                assert (child != NULL);
                fprintf (fp, " %s", child->GetJobName().c_str());
            }
            fprintf (fp, "\n");
        }
    }

    fclose(fp);
}


//===========================================================================
// Private Meathods
//===========================================================================

//-------------------------------------------------------------------------
void Dag::TerminateJob (Job * job) {
    assert (job != NULL);
    assert (job->m_Status == Job::STATUS_DONE);

    //
    // Report termination to all child jobs by removing parent's ID from
    // each child's waiting queue.
    //
    std::list<JobID_t> & qp = job->GetQueueRef(Job::Q_OUTGOING);
    std::list<JobID_t>::iterator it;
    for (it = qp.begin() ; it != qp.end() ; it++) {
        Job * child = GetJob(*it);
        assert (child != NULL);
        child->Remove(Job::Q_WAITING, job->GetJobID());
    }
    m_numJobsDone++;
    m_numJobsRunning--;
    assert (m_numJobsRunning >= 0);
    assert ((unsigned)m_numJobsDone <= m_jobs.size());

    //
    // Add job and its dependants to the termination queue
    //
    if (!job->IsEmpty(Job::Q_OUTGOING)) {
        assert (!m_termQLock);
        m_termQ.push_back ( new TQI(job,qp) );
    }
}

//---------------------------------------------------------------------------
Job * Dag::GetSubmittedJob (bool recovery) {

    assert (!m_termQLock);
    m_termQLock = true;

    Job * job_found = NULL;

    // The following loop scans the entire termination queue (m_termQ)
    // It has two purposes.  First it looks for a submittable child
    // of the first terminated job.  If the first terminated job has
    // no such child, then the loop ends, and this function will return
    // NULL (no job found).
    //
    // If such a child job is found, it is removed from its parent's list
    // of unsubmitted jobs.  If that causes the parent's child list to
    // become empty, then the parent itself is removed from the termination
    // queue.
    //
    // The rest of the termination queue is scanned for duplicates of the
    // earlier found child job.  Those duplicates are removed exactly the
    // same way the original child job was removed.

    bool found = false;  // Flag signally the discovery of a submitted child
    std::list<TQI *>::iterator tqi;
    for (tqi = m_termQ.begin() ; tqi != m_termQ.end() ;
         /* tqi iterated at end of loop */) {
        assert (!(*tqi)->children.empty());

        JobID_t match_ID;  // The current Job ID being examined
        JobID_t found_ID;  // The ID of the original child found

        bool found_on_this_line = false;  // for debugging

        std::list<JobID_t>::iterator child;
        for (child = (*tqi)->children.begin() ;
             child != (*tqi)->children.end() ;
             /* child iterated at end of loop */) {
            match_ID = *child;
            bool kill = false;  // Flag whether this child should be removed
            if (found) {
                if (match_ID == found_ID) {
                    assert (!found_on_this_line);
                    found_on_this_line = true;
                    kill = true;
                }
            } else {
                Job * job = GetJob(match_ID);
                assert (job != NULL);
                if (job->m_Status == (recovery ?
                                     Job::STATUS_READY :
                                     Job::STATUS_SUBMITTED)) {
                    found_ID           = match_ID;
                    found              = true;
                    kill               = true;
                    job_found          = job;
                    found_on_this_line = true;
                }
            }

            if (kill) {
                // The C++ Library Spec (http://www.dinkumware.com/htm_cpl/)
                // says that the erase meathod should
                // "return an iterator that designates the first element
                // remaining beyond any elements removed, or end() if no
                // such element exists."
                //   iterator erase(iterator it);
                // But it only returns void.

                // child = (*tqi)->children.erase(child);
                std::list<JobID_t>::iterator next_child = child;  next_child++;
                (*tqi)->children.erase(child);
                child = next_child;

                break; // There shouldn't be duplicate children for this job
            } else child++;
        }

        // Note that found must become true during the first terminated job
        // for the scan of the rest of the termination queue (m_termQ)
        // to continue
        if (!found) break;

        // If all the children are deleted, then delete this TQI
        if ((*tqi)->children.empty()) {
            // tqi = m_termQ.erase (tqi);
            std::list<TQI *>::iterator next_tqi = tqi;  next_tqi++;
            m_termQ.erase (tqi);
            tqi = next_tqi;
        } else tqi++;
    }
    m_termQLock = false;
    if (recovery && job_found != NULL) {
        job_found->m_Status = Job::STATUS_SUBMITTED;
        m_numJobsRunning++;
        assert (m_numJobsRunningMax == 0 ||
                m_numJobsRunning <= m_numJobsRunningMax);
    }
    return job_found;
}

//-------------------------------------------------------------------------
bool Dag::SubmitReadyJobs () {
    if (m_termQ.empty()) return true;
    assert (!m_termQLock);
    m_termQLock = true;

    std::list<TQI *>::const_iterator tqi = m_termQ.begin();
    assert (tqi != m_termQ.end());
    assert (!(*tqi)->children.empty());

    std::list<JobID_t>::iterator jobit;
    for (jobit = (*tqi)->children.begin() ;
         jobit != (*tqi)->children.end() && (
             m_numJobsRunningMax==0 || m_numJobsRunning < m_numJobsRunningMax);
         jobit++) {
        Job * job = GetJob(*jobit);
        assert (job != NULL);
        if (job->CanSubmit()) {
            if (!Submit (job)) {
                if (DEBUG_LEVEL(DEBUG_QUIET)) {
                    printf ("Fatal error submitting job ");
                    job->Print(true);
                    putchar('\n');
                }
                return false;
            }
        }
    }
    m_termQLock = false;
    return true;
}

} // using namespace dagman
