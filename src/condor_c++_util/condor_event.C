/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include <string.h>
#include <errno.h>
#include "condor_event.h"
#include "user_log.c++.h"

//--------------------------------------------------------
#include "condor_debug.h"
//--------------------------------------------------------


#define ESCAPE { errorNumber=(errno==EAGAIN) ? ULOG_NO_EVENT : ULOG_UNK_ERROR;\
					 return 0; }

const char * ULogEventNumberNames[] = {
	"ULOG_SUBMIT          ", // Job submitted
	"ULOG_EXECUTE         ", // Job now running
	"ULOG_EXECUTABLE_ERROR", // Error in executable
	"ULOG_CHECKPOINTED    ", // Job was checkpointed
	"ULOG_JOB_EVICTED     ", // Job evicted from machine
	"ULOG_JOB_TERMINATED  ", // Job terminated
	"ULOG_IMAGE_SIZE      ", // Image size of job updated
	"ULOG_SHADOW_EXCEPTION"  // Shadow threw an exception
#if defined(GENERIC_EVENT)
	,"ULOG_GENERIC        "
#endif	    
	,"ULOG_JOB_ABORTED    "  // Job terminated
};

const char * ULogEventOutcomeNames[] = {
  "ULOG_OK       ",
  "ULOG_NO_EVENT ",
  "ULOG_RD_ERROR ",
  "ULOG_UNK_ERROR"
};


ULogEvent *
instantiateEvent (ULogEventNumber event)
{
	switch (event)
	{
	  case ULOG_SUBMIT:
		return new SubmitEvent;

	  case ULOG_EXECUTE:
		return new ExecuteEvent;

	  case ULOG_EXECUTABLE_ERROR:
		return new ExecutableErrorEvent;

	  case ULOG_CHECKPOINTED:
		return new CheckpointedEvent;

	  case ULOG_JOB_EVICTED:
		return new JobEvictedEvent;

	  case ULOG_JOB_TERMINATED:
		return new JobTerminatedEvent;

	  case ULOG_IMAGE_SIZE:
		return new JobImageSizeEvent;

	  case ULOG_SHADOW_EXCEPTION:
		return new ShadowExceptionEvent;

#if defined(GENERIC_EVENT)
	case ULOG_GENERIC:
		return new GenericEvent;
#endif

	  case ULOG_JOB_ABORTED:
		return new JobAbortedEvent;

	  default:
        EXCEPT( "Invalid ULogEventNumber" );

	}

    return 0;
}


ULogEvent::
ULogEvent()
{
	struct tm *tm;
	time_t     clock;

	eventNumber = (ULogEventNumber) - 1;
	cluster = proc = subproc = -1;

	(void) time ((time_t *)&clock);
	tm = localtime ((time_t *)&clock);
	eventTime = *tm;
}


ULogEvent::
~ULogEvent ()
{
}


int ULogEvent::
getEvent (FILE *file)
{
	if (!file) return 0;
	
	return (readHeader (file) && readEvent (file));
}


/* change log writing and reading to use new class ads */
int ULogEvent::
putEvent (FILE *file)
{
	if (!file) return 0;

	return (writeHeader (file) && writeEvent (file) && writeTail (file));
}

// This function reads in the header of an event from the UserLog and fills
// the fields of the event object.  It does *not* read the event number.  The 
// caller is supposed to read the event number, instantiate the appropriate 
// event type using instantiateEvent(), and then call readEvent() of the 
// returned event.
int ULogEvent::
readHeader (FILE *file)
{
	int retval;
	
	// read from file
	retval = fscanf (file, " (%d.%d.%d) %d/%d %d:%d:%d ", 
					 &cluster, &proc, &subproc,
					 &(eventTime.tm_mon), &(eventTime.tm_mday), 
					 &(eventTime.tm_hour), &(eventTime.tm_min), 
					 &(eventTime.tm_sec));

	// check if all fields were successfully read
	if (retval != 8)
	{
		return 0;
	}

	// recall that tm_mon+1 was written to log; decrement to compensate
	eventTime.tm_mon--;

	return 1;
}

// Write the tail for the event to the file
// this is only needed for classad events to close the bracket
int ULogEvent::
writeTail (FILE *file)
{
	int retval = 1;

#if defined(CLASSAD_LOGFILE)
	retval = fprintf (file, "]\n");
#endif

	// check that tail was written correctly
	if (retval < 0)
	{
		return 0;
	}

	return 1; 
}	

// Write the header for the event to the file
int ULogEvent::
writeHeader (FILE *file)
{
	int       retval;

	// write header
#if defined(CLASSAD_LOGFILE)
	// while we're at it, lets add the year
	// don't close the classad here in the header, writeTail does that
	retval = fprintf (file, 
					"["
						"MyType = \"UserLogEvent\"; "
						"EventType = %03d;"
						"Cluster = %03d;"
						"Proc = %03d;"
						"Subproc = %03d;"
						"Year = %02d;"
						"Month = %02d;"
						"Day = %02d;"
						"Hour = %02d;"
						"Min  = %02d;"
						"Sec  = %02d;",
					eventNumber, cluster, proc, subproc, 
					eventTime.tm_year, eventTime.tm_mon+1, eventTime.tm_mday, 
					eventTime.tm_hour, eventTime.tm_min, eventTime.tm_sec);
#else
	retval = fprintf (file, "%03d (%03d.%03d.%03d) %02d/%02d %02d:%02d:%02d ",
					  eventNumber, 
					  cluster, proc, subproc,
					  eventTime.tm_mon+1, eventTime.tm_mday, 
					  eventTime.tm_hour, eventTime.tm_min, eventTime.tm_sec);
#endif

	// check if all fields were sucessfully written
	if (retval < 0) 
	{
		return 0;
	}

	return 1;
}


// ----- the SubmitEvent class
SubmitEvent::
SubmitEvent()
{	
	submitHost [0] = '\0';
	eventNumber = ULOG_SUBMIT;
}

SubmitEvent::
~SubmitEvent()
{
}

int SubmitEvent::
writeEvent (FILE *file)
{	

#if defined(CLASSAD_LOGFILE)
	int retval = fprintf (file, 
			"%s = \"Job submitted\"; "
			"%s = \"%s\";",
			EventDesc, EventHost, submitHost);
#else
	int retval = fprintf (file, "Job submitted from host: %s\n", submitHost);
#endif
	if (retval < 0)
	{
		return 0;
	}
	
	return (1);
}

int SubmitEvent::
readEvent (FILE *file)
{
    int retval  = fscanf (file, "Job submitted from host: %s", submitHost);
    if (retval != 1)
    {
	return 0;
    }
    return 1;
}


#if defined(GENERIC_EVENT)
// ----- the GenericEvent class
GenericEvent::
GenericEvent()
{	
	info[0] = '\0';
	eventNumber = ULOG_GENERIC;
}

GenericEvent::
~GenericEvent()
{
}

int GenericEvent::
writeEvent(FILE *file)
{
#if defined(CLASSAD_LOGFILE)
	int retval = fprintf(file, 
			"%s = \"Generic event\"; "
			"%s = \"%s\";", EventDesc, EventInfo, info);
#else
    int retval = fprintf(file, "%s\n", info);
#endif
    if (retval < 0)
    {
	return 0;
    }
    
    return 1;
}

int GenericEvent::
readEvent(FILE *file)
{
    int retval = fscanf(file, "%[^\n]\n", info);
    if (retval < 0)
    {
	return 0;
    }
    return 1;
}
#endif
	

// ----- the ExecuteEvent class
ExecuteEvent::
ExecuteEvent()
{	
	executeHost [0] = '\0';
	eventNumber = ULOG_EXECUTE;
}

ExecuteEvent::
~ExecuteEvent()
{
}


int ExecuteEvent::
writeEvent (FILE *file)
{	
#if defined(CLASSAD_LOGFILE)
	int retval = fprintf (file,
			"%s = \"Job began execution\"; "
			"%s = \"%s\"; ",
			EventDesc, EventHost, executeHost);
#else
	int retval = fprintf (file, "Job executing on host: %s\n", executeHost);
#endif
	if (retval < 0)
	{
		return 0;
	}
	return 1;
}

int ExecuteEvent::
readEvent (FILE *file)
{
	int retval  = fscanf (file, "Job executing on host: %s", executeHost);
	if (retval != 1)
	{
		return 0;
	}
	return 1;
}


// ----- the ExecutableError class
ExecutableErrorEvent::
ExecutableErrorEvent()
{
	errType = (ExecErrorType) -1;
	eventNumber = ULOG_EXECUTABLE_ERROR;
}


ExecutableErrorEvent::
~ExecutableErrorEvent()
{
}

int ExecutableErrorEvent::
writeEvent (FILE *file)
{
	int retval;

#if defined(CLASSAD_LOGFILE)
	retval = fprintf (file, "%s = \"Executable error\"; ", EventDesc);
	retval = fprintf (file, "%s = %d;", EventError, errType);
#endif
	switch (errType)
	{
	  case CONDOR_EVENT_NOT_EXECUTABLE:
#if defined(CLASSAD_LOGFILE)
		retval = fprintf (file, "%s = \"Job file not executable.\";",EventInfo);
#else
		retval = fprintf (file, "(%d) Job file not executable.\n", errType);
#endif
		break;

	  case CONDOR_EVENT_BAD_LINK:
#if defined(CLASSAD_LOGFILE)
		retval = fprintf (file, "%s = \"Job not properly linked for Condor.\",",			EventInfo);
#else
		retval=fprintf(file,"(%d) Job not properly linked for Condor.\n", 
			errType);
#endif
		break;

	  default:
#if defined(CLASSAD_LOGFILE)
		retval = fprintf (file, "%s = \"[Bad error number.]\"; ", EventInfo);
#else
		retval = fprintf (file, "(%d) [Bad error number.]\n", errType);
#endif
	}
	if (retval < 0) return 0;

	return 1;
}


int ExecutableErrorEvent::
readEvent (FILE *file)
{
	int  retval;
	char buffer [128];

	// get the error number
	retval = fscanf (file, "(%d)", &errType);
	if (retval != 1) 
	{ 
		return 0;
	}

	// skip over the rest of the line
	if (fgets (buffer, 128, file) == 0)
	{
		return 0;
	}

	return 1;
}


// ----- the CheckpointedEvent class
CheckpointedEvent::
CheckpointedEvent()
{
	(void)memset((void*)&run_local_rusage,0,(size_t) sizeof(run_local_rusage));
	run_remote_rusage = run_local_rusage;

	eventNumber = ULOG_CHECKPOINTED;
}

CheckpointedEvent::
~CheckpointedEvent()
{
}

int CheckpointedEvent::
writeEvent (FILE *file)
{
	int retval;
#if defined(CLASSAD_LOGFILE)
	retval = fprintf (file, "%s = \"Job was checkpointed\"; ", EventDesc);
#else
	retval = fprintf (file, "Job was checkpointed.\n");
#endif

	if (retval != 0) {
		return 0;
	}

	retval = writeUsage (file, run_remote_rusage, run_local_rusage);
	return (retval > 0);	
}	


int CheckpointedEvent::
readEvent (FILE *file)
{
	int retval = fscanf (file, "Job was checkpointed.");

	char buffer[128];
	if (retval == EOF ||
		!readRusage(file,run_remote_rusage) || fgets (buffer,128,file) == 0  ||
		!readRusage(file,run_local_rusage)  || fgets (buffer,128,file) == 0)
		return 0;

	return 1;
}
		
// ----- the JobEvictedEvent class
JobEvictedEvent::
JobEvictedEvent ()
{
	eventNumber = ULOG_JOB_EVICTED;
	checkpointed = false;

	(void)memset((void*)&run_local_rusage,0,(size_t) sizeof(run_local_rusage));
	run_remote_rusage = run_local_rusage;

	sent_bytes = recvd_bytes = 0.0;
}

JobEvictedEvent::
~JobEvictedEvent ()
{
}

int JobEvictedEvent::
readEvent (FILE *file)
{
	int  retval1, retval2;
	int  ckpt;
	char buffer [128];

	if (((retval1 = fscanf (file, "Job was evicted.")) == EOF)  ||
		((retval2 = fscanf (file, "\n\t(%d) ", &ckpt)) != 1))
		return 0;
	checkpointed = (bool) ckpt;
	if (fgets (buffer, 128, file) == 0) return 0;
	

	if (!readRusage(file,run_remote_rusage) || fgets (buffer,128,file) == 0  ||
		!readRusage(file,run_local_rusage)  || fgets (buffer,128,file) == 0)
		return 0;


	if (fscanf (file, "\t%f  -  Run Bytes Sent By Job\n", &sent_bytes) == 0 ||
		fscanf (file, "\t%f  -  Run Bytes Received By Job\n",
				&recvd_bytes) == 0)
		return 1;				// backwards compatibility

	return 1;
}

int JobEvictedEvent::
writeEvent (FILE *file)
{
	int retval;

#if defined(CLASSAD_LOGFILE)
	if (fprintf (file, "%s = \"Job was evicted\"; ", EventDesc) < 0)
		return 0;
#else
	if (fprintf (file, "Job was evicted.\n\t(%d) ", (int) checkpointed) < 0)
		return 0;
#endif

	if (checkpointed)
#if defined(CLASSAD_LOGFILE)
		retval = fprintf (file, "EvictCheckpoint = true;");
#else
		retval = fprintf (file, "Job was checkpointed.\n\t");
#endif
	else
#if defined(CLASSAD_LOGFILE)
		retval = fprintf (file, "EvictCheckpoint = false;");
#else
		retval = fprintf (file, "Job was not checkpointed.\n\t");
#endif

	if ((retval < 0) || !writeUsage(file, run_remote_rusage, run_local_rusage))
		return 0;


#if defined(CLASSAD_LOGFILE)
	retval = fprintf (file, "Run_Bytes_Sent = %.0f; Run_Bytes_Received = %.0f;",
						sent_bytes, recvd_bytes);
#else
	retval = fprintf(file, "\t%.0f  -  Run Bytes Sent By Job\n\t%.0f  "
						"-  Run Bytes Received By Job\n", recvd_bytes);
#endif
	if (retval < 0) {
		return 1;				// backwards compatibility
	}	

	return 1;
}


// ----- JobAbortedEvent class
JobAbortedEvent::
JobAbortedEvent ()
{
	eventNumber = ULOG_JOB_ABORTED;
}

JobAbortedEvent::
~JobAbortedEvent()
{
}

int JobAbortedEvent::
writeEvent (FILE *file)
{

#if defined(CLASSAD_LOGFILE)
	if (fprintf (file, "%s = \"Job was aborted by the user\"; ") < 0) return 0;
#else
	if (fprintf (file, "Job was aborted by the user.\n") < 0) return 0;
#endif

	return 1;
}


int JobAbortedEvent::
readEvent (FILE *file)
{
	if (fscanf (file, "Job was aborted by the user.") == EOF)
		return 0;

	return 1;
}


// ----- JobTerminatedEvent class
JobTerminatedEvent::
JobTerminatedEvent ()
{
	eventNumber = ULOG_JOB_TERMINATED;
	coreFile[0] = '\0';
	returnValue = signalNumber = -1;

	(void)memset((void*)&run_local_rusage,0,(size_t)sizeof(run_local_rusage));
	run_remote_rusage=total_local_rusage=total_remote_rusage=run_local_rusage;

	sent_bytes = recvd_bytes = total_sent_bytes = total_recvd_bytes = 0.0;
}

JobTerminatedEvent::
~JobTerminatedEvent()
{
}

int JobTerminatedEvent::
writeEvent (FILE *file)
{
	int retval=0;

#if defined(CLASSAD_LOGFILE)
	if (fprintf (file, "%s = \"Job completed\"; ") < 0) return 0;
#else
	if (fprintf (file, "Job terminated.\n") < 0) return 0;
#endif
	if (normal)
	{
#if defined(CLASSAD_LOGFILE)
		if (fprintf (file, "Normal_termination = true; ") < 0) return 0;
#else
		if (fprintf (file,"\t(1) Normal termination (return value %d)\n\t", 
						  returnValue) < 0)
			return 0;
#endif
	}
	else
	{
#if defined(CLASSAD_LOGFILE)
		if (fprintf (file, "Normal_termination = false; ") < 0) return 0;
#else
		if (fprintf (file,"\t(0) Abnormal termination (signal %d)\n",
						  signalNumber) < 0)
			return 0;
#endif

		if (coreFile [0])
#if defined(CLASSAD_LOGFILE)
			retval = fprintf (file, "Corefile = \"%s\"; ", coreFile);
#else
			retval = fprintf (file, "\t(1) Corefile in: %s\n\t", coreFile);
#endif
		else
#if defined(CLASSAD_LOGFILE)
			retval = fprintf (file, "Corefile = \"NULL\"; ", coreFile);
#else
			retval = fprintf (file, "\t(0) No core file\n\t");
#endif
	}


	if ((retval < 0) || !writeUsage (file, run_remote_rusage, run_local_rusage))
		return 0;

#if defined(CLASSAD_LOGFILE)
	retval = fprintf (file, "Run_Bytes_Sent = %.0f; "
							"Run_Bytes_Received = %.0f;"
							"Total_Bytes_Sent = %.0f; "
							"Total_Bytes_Received = %.0f; ",
							sent_bytes, recvd_bytes, 
							total_sent_bytes, total_recvd_bytes);
#else
	retval = fprintf(file, "\t%.0f  -  Run Bytes Sent By Job\n"
							"\t%.0f -  Run Bytes Received By Job\n"
							"\t%.0f -  Total Bytes Sent By Job\n"
							"\t%.0f -  Total Bytes Received By Job\n",
							sent_bytes, recvd_bytes,
							total_sent_bytes, total_recvd_bytes);
#endif

	if (retval < 0) {
		return 1;				// backwards compatibility
	}

	return 1;
}


int JobTerminatedEvent::
readEvent (FILE *file)
{
	char buffer[128];
	int  normalTerm;
	int  gotCore;
	int  retval1, retval2;

	if ((retval1 = (fscanf (file, "Job terminated.") == EOF)) 	||
		(retval2 = fscanf (file, "\n\t(%d) ", &normalTerm)) != 1)
		return 0;

	if (normalTerm)
	{
		normal = true;
		if(fscanf(file,"Normal termination (return value %d)",&returnValue)!=1)
			return 0;
	}
	else
	{
		normal = false;
		if((fscanf(file,"Abnormal termination (signal %d)",&signalNumber)!=1)||
		   (fscanf(file,"\n\t(%d) ", &gotCore) != 1))
			return 0;

		if (gotCore)
		{
			if (fscanf (file, "Corefile in: %s", coreFile) != 1) 
				return 0;
		}
		else
		{
			if (fgets (buffer, 128, file) == 0) 
				return 0;
		}
	}

	// read in rusage values
	if (!readRusage(file,run_remote_rusage) || !fgets(buffer, 128, file) ||
		!readRusage(file,run_local_rusage)   || !fgets(buffer, 128, file) ||
		!readRusage(file,total_remote_rusage)|| !fgets(buffer, 128, file) ||
		!readRusage(file,total_local_rusage) || !fgets(buffer, 128, file))
		return 0;
	
	if (fscanf (file, "\t%f  -  Run Bytes Sent By Job\n", &sent_bytes) == 0 ||
		fscanf (file, "\t%f  -  Run Bytes Received By Job\n",
				&recvd_bytes) == 0 ||
		fscanf (file, "\t%f  -  Total Bytes Sent By Job\n",
				&total_sent_bytes) == 0 ||
		fscanf (file, "\t%f  -  Total Bytes Received By Job\n",
				&total_recvd_bytes) == 0)
		return 1;				// backwards compatibility

	return 1;
}


JobImageSizeEvent::
JobImageSizeEvent()
{
	eventNumber = ULOG_IMAGE_SIZE;
	size = -1;
}


JobImageSizeEvent::
~JobImageSizeEvent()
{
}


int JobImageSizeEvent::
writeEvent (FILE *file)
{
#if defined(CLASSAD_LOGFILE)
	if (fprintf (file, "%s = \"Image size updated\"; Image_Size = %d; ", 
						EventDesc, size) < 0) 
	{
		return 0;
	}
#else
	if (fprintf (file, "Image size of job updated: %d\n", size) < 0)
		return 0;
#endif

	return 1;
}


int JobImageSizeEvent::
readEvent (FILE *file)
{
	int retval;
	if ((retval=fscanf(file,"Image size of job updated: %d", &size)) != 1)
		return 0;

	return 1;
}

ShadowExceptionEvent::
ShadowExceptionEvent ()
{
	eventNumber = ULOG_SHADOW_EXCEPTION;
	message[0] = '\0';
	sent_bytes = recvd_bytes = 0.0;
}

ShadowExceptionEvent::
~ShadowExceptionEvent ()
{
}

int ShadowExceptionEvent::
readEvent (FILE *file)
{
	if (fscanf (file, "Shadow exception!\n\t") == EOF)
		return 0;
	if (fgets(message, BUFSIZ, file) == NULL) {
		message[0] = '\0';
		return 1;				// backwards compatibility
	}

	// remove '\n' from message
	message[strlen(message)-1] = '\0';

	if (fscanf (file, "\t%f  -  Run Bytes Sent By Job\n", &sent_bytes) == 0 ||
		fscanf (file, "\t%f  -  Run Bytes Received By Job\n",
				&recvd_bytes) == 0)
		return 1;				// backwards compatibility

	return 1;
}

int ShadowExceptionEvent::
writeEvent (FILE *file)
{
#if defined(CLASSAD_LOGFILE)
	if (fprintf (file, "%s = \"Shadow execption\"; ", EventDesc) < 0)
		return 0;
	if (fprintf (file, "%s = \"%s\"; ", EventInfo, message) < 0)
		return 0;
#else
	if (fprintf (file, "Shadow exception!\n\t") < 0)
		return 0;
	if (fprintf (file, "%s\n", message) < 0)
		return 0;
#endif

	int retval;
#if defined(CLASSAD_LOGFILE)
	retval = fprintf (file, "Run_Bytes_Sent = %.0f; Run_Bytes_Received = %.0f;",
						sent_bytes, recvd_bytes);
#else
	retval = fprintf(file, "\t%.0f  -  Run Bytes Sent By Job\n\t%.0f  "
						"-  Run Bytes Received By Job\n", recvd_bytes);
#endif

	if (retval < 0) {
		return 1;		// backwards compatibility
	}
	
	return 1;
}

static const int seconds = 1;
static const int minutes = 60 * seconds;
static const int hours = 60 * minutes;
static const int days = 24 * hours;

/* note that this function writes the usage info into a nested classad */
int ULogEvent::
writeUsage (FILE *file, rusage &remote, rusage &local) {
#if defined(CLASSAD_LOGFILE)
		if ((fprintf (file, "Run_Remote_Usage = [") < 0)	||
		(!writeRusage (file, remote)) 			||
		(fprintf (file, "];") < 0)							||
		(fprintf (file, "Run Local Usage = [") < 0)			||
		(!writeRusage (file, local)) 			||
		(fprintf (file, "];") < 0))
		return 0;
#else
		if ((!writeRusage (file, remote)) 		||
		(fprintf (file, "  -  Run Remote Usage\n\t") < 0) 	||
		(!writeRusage (file, local)) 			||
		(fprintf (file, "  -  Run Local Usage\n") < 0))
		return 0;
#endif
	return 1;
}

int ULogEvent::
writeRusage (FILE *file, rusage &usage)
{
	int usr_secs = usage.ru_utime.tv_sec;
	int sys_secs = usage.ru_stime.tv_sec;

	int usr_days, usr_hours, usr_minutes;
	int sys_days, sys_hours, sys_minutes;

	usr_days = usr_secs/days;  			usr_secs %= days;
	usr_hours = usr_secs/hours;			usr_secs %= hours;
	usr_minutes = usr_secs/minutes;		usr_secs %= minutes;
 	
	sys_days = sys_secs/days;  			sys_secs %= days;
	sys_hours = sys_secs/hours;			sys_secs %= hours;
	sys_minutes = sys_secs/minutes;		sys_secs %= minutes;
 	
	int retval;
#if defined(CLASSAD_LOGFILE)
	retval = fprintf (file,
		"Usr_days = %d;"
		"Usr_hours = %02d;"
		"Usr_minutes = %02d;"
		"Usr_secs = %02d;"
		"Sys_days = %d;"
		"Sys_hours = %02d;"
		"Sys_minutes = %02d;"
		"Sys_secs = %02d;",
		usr_days, usr_hours, usr_minutes, usr_secs, 
		sys_days, sys_hours, sys_minutes, sys_secs);
#else
	retval = fprintf (file, "\tUsr %d %02d:%02d:%02d, Sys %d %02d:%02d:%02d",
					  usr_days, usr_hours, usr_minutes, usr_secs,
					  sys_days, sys_hours, sys_minutes, sys_secs);
#endif

	return (retval > 0);
}


int ULogEvent::
readRusage (FILE *file, rusage &usage)
{
	int usr_secs, usr_minutes, usr_hours, usr_days;
	int sys_secs, sys_minutes, sys_hours, sys_days;
	int retval;

	retval = fscanf (file, "\tUsr %d %d:%d:%d, Sys %d %d:%d:%d",
					  &usr_days, &usr_hours, &usr_minutes, &usr_secs,
					  &sys_days, &sys_hours, &sys_minutes, &sys_secs);

	if (retval < 8)
	{
		return 0;
	}

	usage.ru_utime.tv_sec = usr_secs + usr_minutes*minutes + usr_hours*hours +
		usr_days*days;

	usage.ru_stime.tv_sec = sys_secs + sys_minutes*minutes + sys_hours*hours +
		sys_days*days;

	return (1);
}

