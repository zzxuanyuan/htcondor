-----------------------------------------------------------------------------
           Condor DAGMan (Directed Acyclic Graph Manager)
-----------------------------------------------------------------------------
        Current Version:  1
                   Date:  May 13, 1999
      Current Developer:  Colby O'Donnell <colbster@cs.wisc.edu>
                 Period:  Fall 1998 - Spring 1999
         Past Developer:  George Varghese <joev@cs.wisc.edu>
                 Period:  Spring 1998
-----------------------------------------------------------------------------

Introduction
------------

This document is intended for future developers of condor_dagman.

First things first, before touching any source code, you should do the
following to get familiar with both DAGMan and Condor:

  1)  Read the Condor Manual, specifically the DAGMan section (2.10)
      http://www.cs.wisc.edu/condor/manual/v6.1/

  2)  Read this document.

  3)  Subscribe to the condor_team mailing list via majordomo@cs.wisc.edu.
      Read Colby's archived Condor e-mail stored at:
        /p/condor/workspaces/shared/colbster/saved_email.pine

  4)  Talk to the Condor team members about their knowledge of DAGMan and its
      history.  (See the Condor Team Members section of this document)

  5)  Read the Doc++ developer documentation for DAGMan, Daemon Core, Condor
      C++ Util (specifically the UserLog API).

  6)  Read the source code before modifying it.  If you have unanswered
      questions, Colby would be happy to answer them.

Condor Team Members
-------------------

  Colby O'Donnell <colbster@cs.wisc.edu>

     Colby worked on DAGMan for a year, and is the one who wrote the file you
     are now reading.  Talk to him if you have questions that failed to be
     answered by this document, other team members, the user manual, or the
     source code.  Even though he is graduated and working in Seattle, the
     e-mail address above should be valid indefinitely.

  Miron Livny <miron@cs.wisc.edu>

     Miron is important for obviously reasons.  But take note that he is most
     familiar with the Termination Queue algorithm that DAGMan currently uses
     to match DAG jobs to UserLog events during recovery.  More on that later
     in this document.  Miron will also have the most to say about the needed
     reliable transactions for a submit protocol.

  Rajesh Raman <raman@cs.wisc.edu>

     Rajesh is the ClassAd expert, and it the most valuable person to talk
     to concerning future plans and ideas for DAGMan.  Rajesh and Colby had
     many conversations about how the next version of DAGMan should use
     ClassAds to represent job dependency.  More on that later in this
     document.  Rajesh and Colby are the ones who pioneered STL.  Rajesh's
     experience has likely surpassed Colby, simply because the ClassAd
     implementation has many more lines of code that DAGMan.

  Todd Tannenbaum <tannenba@cs.wisc.edu>

     Todd is our Windows NT expert, and is the principal author of
     condor_daemon_core, which is responsible for providing OS services to
     Condor daemons in a portable way.  Since DAGMan uses condor_daemon_core,
     it is very important to understand the services provided by it.  Ask
     Todd about how a network command socket can be requested by DAGMan
     to daemon core in order to allow 3rd party software to control a running
     DAGMan process.

  Jim Basney <jbasney@cs.wisc.edu>

     Scheduling and checkpointing expert.  Jim will provide very good insight
     into any scheduling aspect of DAGMan.  He is the most familiar with 
     condor_schedd (the condor scheduler) and can help guide you when
     considering how the next version of DAGMan might interact with the
     schedd.

  Derek Wright <wright@cs.wisc.edu>

     Derek is an staff member, who will provide general Condor guidance, such
     as: how to set up your build environment, how to properly use CVS, how to
     follow Condor coding standards, what features of C++ are acceptable and
     workable at this time (i.e. STL, namespaces), how to use RUST, and many
     more things.

  Jeff Ballard <ballard@cs.wisc.edu>

     Jeff is also a staff member who spent considerable time making Condor
     compile with the new egcs compiler.  Jeff is also the manager of RUST.
     If a RUST ticket comes in for DAGMan, it will be assigned to you by Jeff.

  Mike Yoder <yoderme@cs.wisc.edu>

     Condor PVM expert.  Mike and Colby pioneered the Condor Doc++ effort.  If
     you have questions on how to compile the current Doc++ developer
     documentation for Condor, or have questions on how to write it, Mike will
     probably know the answer.

Current CVS Situation
---------------------

The latest and greatest DAGMan source code lives on the 'newads2' CVS branch.
This branch will soon be merge back into the trunk.  Derek will be the best
person to explain what this means.

The DAGMan code on the trunk was last modify around December 1998, when it was
prepared for Colby's trip to the University of Washington to visit a group at
the Applied Physics Lab.  (The details of that trip can be found in Colby's
archived e-mail, from Miron, and by reading the RUST tickets owned by
colbster.

The 'newads2' branch was created for Rajesh and Colby.  Both were converting
their code (classads and dagman) to use STL and the egcs compiler.  The DAGMan
code in that branch uses STL, fixes several bugs which can be found by reading
CVS logs, and added a couple nice parsing features.

Your first task could very likely be the merging of the newads2 branch into
the trunk, once the stable Condor 6.2 is put on its own branch, and the
developer 6.3 version is put on the trunk.

Building DAGMan
---------------

The DAGMan source code lives in the standard Condor src/ tree, under
condor_dagman.  It gets built automatically when you compiler condor.  Talk to
other team members (Derek especially) to find out how to set up a Condor build
environment correctly.

The Imakefile is used to generate the Makefile.  Issue the command
"../condor_imake" in the working directory.  After the Makefile is generated,
type 'make'.  The DAGMan executable is 'condor_dagman'.

DAGMan and Daemon Core:
-----------------------

For better or for worse, DAGMan uses Daemon Core (DC).  That means that DAGMan
does not have its own main(), but rather DAGMan uses the DC callback
philosophy.  At the present time, one could argue that DC is completely
unnecessary for DAGMan.  DC is only being used to generate a periodic timer
callback (which could be done with sleep), and to handle SIGUSR1 (which is
easy to do in Unix).

For that little benefit, the cost is an enormous bloat in condor_dagman
code size, as most of the condor libraries must be linked with DC.  So a dagman
executable that once was 300-600k (depending on OS, arch, and debugging
symbols), is now over 4 MB.

However, future versions of DAGMan will likely beg for DC.  Once Condor is
released in Window NT form, DC will provide a portable way to handle signals,
and call sleep.  It will also offer easy-to-create network command sockets.
There are probably a few other nice features that Todd could think of to help
convince you that using DC is worth the present hassle.


DAGMan and the UserLog API:
---------------------------

The UserLog API is the common means of reading and writing job events to a log
file.  The Condor system generates those events, and DAGMan reads them to
learn the state of its jobs.  Reading the log file is quite nice.  However,
detecting that new events have been written to the log is not nice at all.
Currently, DAGMan pulls the file, by doing a stat() every two seconds.  If the
log files grew since the last stat(), then DAGMan begins reading the log via
the UserLog API.  (See DAG::DetectLogGrowth() in dag.C)

It is likely that a log file will always be needed for recoverability.  The
debatable aspect is whether there is a better way to for DAGMan to interface
with Condor while it is running.

Rajesh is currently working on new ClassAds, which will eventually offer a
'trigger' mechanism, such that a process can register callbacks with a ClassAd
collection server.  For example, DAGMan could set up a collection view that
represents all the running jobs in a DAG.  If any job leaves the view (changes
to _not_ running), then a trigger would be generated, and a callback function
would be invoked in DAGMan, which could handle the event.  This trigger and
callback method would be more elegant that pulling a log file.

The limitation of triggers is that they would not likely be reliable (i.e. a
lost trigger is not resent).  So the log file will probably continue to be the
final authority on job events.

DAGMan and condor_submit:
-------------------------

One of the biggest drawbacks of Condor in general is its lack of API support.
For example, the only way for DAGMan to submit a job is via the command-line
condor_submit program.  The is run via popen().  (See submit_submit() in
submit.C) popen() is clumsy at best.  A big problem is that a failed popen()
of condor_submit does not guarantee that a job was not submitted to Condor.
Thus, it is remotely possible for DAGMan to submit a job twice, because it
believes that the first submission failed.  This would break the current job
termination queue, which is extremely sensitive to the order of events in the
UserLog (see the Job Termination Queue section).

In the works is an API for submitting jobs to Condor.  This submit API would
utilize the classad transaction API that Colby and Rajesh were working on
during Spring 1999.  

Job Termination Queue:
----------------------

Jobs in Condor are given a CondorID in order to uniquely identify them.  The
CondorID is essentially a key (three integers, only one of which is used
nowadays).  The trouble with the current submission system is that the
CondorID is selected by condor_submit at submit time.  So DAGMan has no
control over what ID is used.

DAGMan must keep track of jobs in the DAG, preserving their dependencies.  The
user gives each job in the DAG a name.  Unfortunately, that name cannot be
given to Condor at present time.  So DAGMan must establish a mapping between a
job's DAG name, and the job's CondorID.  Since DAGMan cannot choose the
CondorID before submission, it must look at the log file to see what Condor ID
was assigned to the job it just submitted.  (condor_submit does print the
assigned CondorID to the screen before exiting, but as was mentioned in the
condor_submit section, it is possible for condor_submit to fail, but the job
still makes it into the Condor system.)

After condor_submit terminates, it may be a few seconds, or many seconds until
the submit event is seen in the UserLog.  Within that time, it is very
possible for some other event to occur with other jobs, such as a termination
event, a checkpoint, etc.  Even more likely is the possibility that DAGMan
wishes to submit more than one job at a time.  It would be unreasonable to
have to wait for the submit event of one job to appear in the log before the
next job can be submitted.

Thus we wish DAGMan to be able to submit several jobs simultaneously (in a
predictable order).  When job submit events appear in the log, DAGMan must
know which job is to be matched with that event, based on the order of
submission, and the order in the log.

The length of time needed for a job to complete is completely unpredictable,
as is the nature of Condor.  Since the submission of jobs in a DAG often
follows the termination of other jobs, the total order in which jobs are
submitted greatly depends on the order of job termination.

The one guarantee that we can rely on (and it is absolutely essential), is
that if job A is submitted to Condor before job B, then the submit event of
job A must appear before the submit event of job B in the log.  That rule,
along with the fact that DAGMan carefully orders its jobs in the DAG, is
enough for us to create an algorithm which can correctly and predictably match
submit events in the log with the correct DAG job name, during normal
operation and during recovery.  What I mean by DAGMan carefully ordering its
jobs is the following: given any set of jobs in a DAG, DAGMan will always
submit them in the same order.

The algorithm used is the job termination queue, which was suggested by Miron,
and implemented by Colby.  The termination queue (see Dag::m_termQ in dag.h)
is a list of TQI's (termination queue items).  A TQI (see class TQI in dag.h)
is the mapping of a parent job, to a list of children jobs not yet seen in the
log (not yet run).

When a job terminates (i.e. when a terminate event appears in the log), a TQI
is created with that job as the parent, and its children as the TQI children.
The TQI is appended to the end of the termination queue.  As submit events are
found in the log, the DAG jobs they should be matched with is well defined by
the termination queue.  The first child of the first TQI item is deleted from
the termination queue and used as the match.  Note that the child could appear
multiple times under other TQI's (because it could be the child of more than
one parent that terminated).  In that case, all duplicate entries are also
deleted (since we don't want to submit the child job twice).

Future Plans:
-------------

The job termination queue is quite complicated, and begs to be eliminated.
Rajesh and Colby have talked about doing that by putting extra attributes in a
job's ClassAd, such as its DAG name, and a DagID mapping it to an active DAG.

The job's ClassAd could also contain the job's dependency information.  Thus
DAGMan would be much less obligated to keep track of job status in memory, and
recovery from a log file that consisted of similar ClassAds would make
recovery close to trivial.

It is very clear that DAGMan should use ClassAds to represent the DAG, the
jobs in the DAG, and to interface with a future condor_submit API and the
Condor user log file.  It's likely that DAGMan should interact with the
schedd collection server, possibly using triggers and other proposed
collection features.  (Ask Rajesh what a collection is.)

What is not so clear (and could be potentially good research) is _what_ DAGMan
could put in a job ClassAd, and how to use them.  There are many
possibilities, and Rajesh is full of ideas here.


Dagman version 1:   (+ is feature, - is limitation)
-----------------
  1)  + Dagman allows user to specify job dependencies
  2)  + Dagman output level specified dynamically
  3)  + Dagman will safely recover an unfinished Dag
  4)  + User can completely delete a running Dag.
  5)  - All jobs in a Dag must go to exactly one unique condor log file
  6)  - All jobs must exit normally, else Dag will be aborted

Dagman version 2:
-----------------
  5)  + All jobs in a Dag must go to one log file, but log file can be
        shared with other Dags.

  6)  + A job can be "undone", or there is some notion of a job instance.
        Hence, a job that exits abnormally or is cancelled by the user can
        be rerun such that the new run's log entry is unique from the old
        run's log entry (in terms of recovery)

  7)  + A general purpose command socket will be used to direct Dagman
        while it's running.  Commands like CANCEL_JOB X or DELETE_ALL
        would be supported, as well as notification messages like
        JOB_SUBMIT or JOB_TERMINATE, etc.  Eventually, a Java Gui would
        graphically represent the Dag's state, and offer buttons and dials
        for graphic Dag manipulation.

Status of Features:
-------------------

Feature 1) is done, and works.
Feature 2) The debug option (-debug <integer>) allows the user to
           specify the level of output.  The debug levels are as follows:

     Option    Meaning  Explanation
     ---------------------------------------------------------------------
     -debug 0  SILENT   absolutely no output during normal operation
                        good for running Dagman from a script
     -debug 1  QUIET    only print critical message to the screen
     -debug 2  NORMAL   Normal output
     -debug 3  VERBOSE  Verbose output, all relevant to the end user
     -debug 4  DEBUG_1  Simple debug output, only helpful for developers
     -debug 5  DEBUG_2  Detailed outer loop debug output
     -debug 6  DEBUG_3  Flood your screen with inner loop output.  Highly
                        recommend that all output be redirected to a file,
                        since an xterm would be heavily strained.
     -debug 6  DEBUG_4  Rarely used.  Produces maximum output.


  There has been a lot of talk (Todd, Derek) about whether DAGMan should use
  the standard Condor Daemon dprintf() to print conditional output.  The
  problem with dprintf() is that it's designed for the traditional Condor
  Daemon.  DAGMan is arguably _not_ a daemon, but more like a user level
  process that happens to be acting as a resource manager.

Feature 3) Uses a Job Termination Queue to map UserLog Events to 
           DAG jobs during recovery.

Feature 4) Works via condor_rm of the DAGMan job.  The schedd realizes that
           the DAGMan job is a metascheduler, and thus sends a SIGUSR1 to
           DAGMan.  DAGMan responds to this by quickly issuing a 'condor_rm'
           of all its currently running jobs, before exiting.

Limit   5) Having all jobs in a Dag going to the same logfile will be a
           permanent limitation.  However, if two or more Dag's point to the
           same log, a notion of Dag instance must be added to each node's log
           entry, so that a Dag recovery can differentiate its job's log
           entries from another Dag's job log entries.  In version 2, Dagman
           should add a ClassAd attribute that maps a job to a DAG, and can
           handle multiple instances of that job, or of the DAG.

Limit   6) To overcome this limit, and job instance must be notioned.
           Since Dagman will only run once instance at a time, we do not
           have to worry about instance concurrency.  A job instance is
           either currently running, or was cancelled and possibly
           replaced by a new instance.  condor_submit must support naming
           a job, either via command argument or ClassAd attribute.

Feature 7) If you build it (good command socket), they (GUIs) will come.


condor_submit_dag script:
-------------------------

Because DAGMan is a Daemon Core program, some arguments are extracted from
argv before being given to main_init().  Since we don't want to expose the
user to those options, Derek wrote a condor_submit_dag script in perl.

The script also does sanity checks, such as verifying that all Condor Command
files point to the same log file.  Once DAGMan uses ClassAds, it will be easy
for DAGMan itself to verify that the attribute for the Condor log file is the
same for all ClassAds, and can even change and add it if necessary.

Source Code:
------------

The main initialization of DAGMan takes place in main.C.  Signal and timer
callbacks are requested from Daemon Core.  Next the configuration file is read
into memory by called the parse function (see below).

The main engine of the entire DAGMan program is the Dag class.  Only one Dag
object is created during the run of DAGMan.  The DAG object contains a list of
Job objects (see below).  Whenever there is log activity,
Dag::ProcessLogEvents() is called.  ProcessLogEvents contains the switch logic
for handling the job events that appear in the log.  For example, if a job
terminates, then its ready children are submitted to Condor.
Dag::ProcessLogEvents() can works during recovery (see the prototype and
doc++) and during normal operation.

The parse() function, in parse.{h,C}, reads a DAG from the configuration file
into memory.  It instanciates the one (and only) Dag object, and inserts job
objects into the DAG.  parse() is called from main.C.

The Parser class, located in parser.{h,C}, is used by the parse() function and
the submit_submit() function.  A Parser object can parse input by char, token,
or line.  A token in DAGMan is considered a group of non-white space
characters.  One good feature that was requested in RUST was support for line
continuation using the '\' character.  The Parser class supports that.

The Script class is located in script.{h,C}, and is represents a PRE or POST
script of a DAG job.  A Script object is responsive for running the script it
represents.  See Script::Run().

The Job class (see job.{h,C}) represents a job node on the DAG.  The job has a
name, optionally has a PRE and POST script, a CondorID (which is filled in
when the job's submit event is read from the log file), and a status.  In
Condor, a job has many different states, but Condor is only concerned with
four basic states:  1) Before Submission, 2) Submitted in Condor, 3)
successful termination, 4) Fatal Error.  You will find these documented in
job.h.  See Job::status_t.
