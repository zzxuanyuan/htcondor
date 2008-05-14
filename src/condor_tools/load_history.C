/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifdef WANT_QUILL

#include "condor_common.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_distribution.h"
#include "dc_collector.h"
#include "get_daemon_name.h"
#include "internet.h"
#include "print_wrapped_text.h"
#include "MyString.h"
#include "ad_printmask.h"
#include "directory.h"
#include "iso_dates.h"
#include "basename.h" // for condor_dirname

#include "history_utils.h"

#include "sqlquery.h"
#include "historysnapshot.h"
#include "jobqueuedatabase.h"
#include "pgsqldatabase.h"
#include "mysqldatabase.h"
#include "condor_ttdb.h"
#include "jobqueuecollection.h"
#include "dbms_utils.h"

#if HAVE_ORACLE
#undef ATTR_VERSION
#include "oracledatabase.h"
#endif

#define NUM_PARAMETERS 3

char    *mySubSystem = "TOOL";

static void Usage(char* name) 
{
  printf("Usage: %s -f history-filename [-name schedd-name jobqueue-birthdate]\n", name);
  exit(1);
}

static void doDBconfig();
static void readHistoryFromFile(char *JobHistoryFileName);

//------------------------------------------------------------------------

/*
static CollectorList * Collectors = NULL;
static	QueryResult result;
static	CondorQuery	quillQuery(QUILL_AD);
static	ClassAdList	quillList;
static  bool longformat=false;
static  bool customFormat=false;
static  bool backwards=false;
static  AttrListPrintMask mask;
static  char *BaseJobHistoryFileName = NULL;
static int cluster=-1, proc=-1;
static int specifiedMatch = 0, matchCount = 0;
*/
static JobQueueDatabase* DBObj=NULL;
static dbtype dt;
static char *ScheddName=NULL;
static int ScheddBirthdate = 0;

int
main(int argc, char* argv[])
{

  void **parameters;

  char* JobHistoryFileName=NULL;

  int i;
  parameters = (void **) malloc(NUM_PARAMETERS * sizeof(void *));
  myDistro->Init( argc, argv );

  config();
  Termlog = 1;
  dprintf_config("TOOL");

  for(i=1; i<argc; i++) {

    if (strcmp(argv[i],"-f")==0) {
		if (i+1==argc || JobHistoryFileName) break;
		i++;
		JobHistoryFileName=argv[i];
    }
    else if (strcmp(argv[i],"-help")==0) {
		Usage(argv[0]);
    }
    else if (strcmp(argv[i],"-name")==0) {
		if (i+1==argc || ScheddName) break;
		i++;
		ScheddName=strdup(argv[i]);
        if ((i+1==argc) || ScheddBirthdate) break;
        i++;
        ScheddBirthdate = atoi(argv[i]);
    }
/*    else if (strcmp(argv[i],"-debug")==0) {
          // dprintf to console
          Termlog = 1;
    }
*/
    else {
		Usage(argv[0]);
    }
  }
  if (i<argc) Usage(argv[0]);

  if (JobHistoryFileName == NULL) 
	  Usage(argv[0]);

  if ((ScheddName == NULL) || (ScheddBirthdate == 0)) {

    if (ScheddName) 
        fprintf(stdout, "You specified Schedd name without a Job Queue"
                        "Birthdate. Ignoring value %s\n", ScheddName);
        
    Daemon schedd( DT_SCHEDD, 0, 0 );

    if ( schedd.locate() ) {
        char *scheddname; 
        if( (scheddname = schedd.name()) ) {
            ScheddName = strdup(scheddname);
        } else {
            fprintf(stderr, "You did not specify a Schedd name and Job Queue "
                           "Birthdate on the command line "
                           "and there was an error getting the Schedd "
                           "name from the local Schedd Daemon Ad. Please "
                           "check that the SCHEDD_DAEMON_AD_FILE config "
                           "parameter is set and the file contains a valid "
                           "Schedd Daemon ad.\n");
            exit(1);
        }
        ClassAd *daemonAd = schedd.daemonAd();
        if(daemonAd) {
            if (!(daemonAd->LookupInteger( ATTR_JOB_QUEUE_BIRTHDATE, 
                        ScheddBirthdate) )) {
                // Can't find the job queue birthdate
                fprintf(stderr, "You did not specify a Schedd name and "
                           "Job Queue Birthdate on the command line "
                           "and there was an error getting the Job Queue "
                           "Birthdate from the local Schedd Daemon Ad. Please "
                           "check that the SCHEDD_DAEMON_AD_FILE config "
                           "parameter is set and the file contains a valid "
                           "Schedd Daemon ad.\n");
                exit(1);
            }
        }
    } else {

		fprintf(stderr, "You did not specify a Schedd name and Job Queue "
				        "Birthdate on the command line and there was "
				        "an error getting the Schedd Daemon Ad. Please "
				        "check that Condor is running and the SCHEDD_DAEMON_AD_FILE "
				        "config parameter is set and the file contains a valid "
				        "Schedd Daemon ad.\n");
		exit(1);
	}
  }

  doDBconfig();
  readHistoryFromFile(JobHistoryFileName);

  if(parameters) free(parameters);
  if(ScheddName) free(ScheddName);
  return 0;
}


//------------------------------------------------------------------------

static void doDBconfig() {

	if (param_boolean("QUILL_ENABLED", false) == false) {
		EXCEPT("Quill++ is currently disabled. Please set QUILL_ENABLED to "
			   "TRUE if you want this functionality and read the manual "
			   "about this feature since it requires other attributes to be "
			   "set properly.");
	}

    DBObj = getDBObj(dt);

	QuillErrCode ret_st;

	ret_st = DBObj->connectDB();
	if (ret_st == QUILL_FAILURE) {
		fprintf(stderr, "doDBconfig: unable to connect to DB--- ERROR");
		exit(1);
	}
}

QuillErrCode insertHistoryJob(AttrList *ad) {

	MyString errorSqlStmt;
	DBObj->beginTransaction();
	QuillErrCode ec = insertHistoryJobCommon(ad, DBObj, dt, errorSqlStmt, ScheddName, (time_t) ScheddBirthdate);
	DBObj->commitTransaction();
	return ec;
}

// Read the history from a single file and print it out. 
static void readHistoryFromFile(char *JobHistoryFileName)
{
    int EndFlag   = 0;
    int ErrorFlag = 0;
    int EmptyFlag = 0;
    AttrList *ad = NULL;

    MyString buf;
    
    int fd = safe_open_wrapper(JobHistoryFileName, O_RDONLY | O_LARGEFILE);
	if (fd < 0) {
        fprintf(stderr,"History file (%s) not found or empty.\n", JobHistoryFileName);
        exit(1);
    }
    
	FILE *LogFile = fdopen(fd, "r");
	if (!LogFile) {
        fprintf(stderr,"History file (%s) not found or empty.\n", JobHistoryFileName);
        exit(1);
    }

    while(!EndFlag) {

        if( !( ad=new AttrList(LogFile,"***", EndFlag, ErrorFlag, EmptyFlag) ) ){
            fprintf( stderr, "Error:  Out of memory\n" );
            exit( 1 );
        } 
        if( ErrorFlag ) {
            printf( "\t*** Warning: Bad history file; skipping malformed ad(s)\n" );
            ErrorFlag=0;
            if(ad) {
                delete ad;
                ad = NULL;
            }
            continue;
        } 
        if( EmptyFlag ) {
            EmptyFlag=0;
            if(ad) {
                delete ad;
                ad = NULL;
            }
            continue;
        }

		insertHistoryJob(ad);

        if(ad) {
            delete ad;
            ad = NULL;
        }
    }
    fclose(LogFile);
    return;
}
#endif /* WANT_QUILL */

