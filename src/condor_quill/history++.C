/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
#include "sqlquery.h"
#include "historysnapshot.h"

#define NUM_PARAMETERS 3


static void Usage(char* name) 
{
  printf("Usage: %s [-l] [-f history-filename] [-name quill-name] [-constraint expr | cluster_id | cluster_id.proc_id | owner | -completedsince date/time]\n",name);
  exit(1);
}
static char * getDBConnStr(char *&quillName, char *&databaseIp, char *&databaseName);
static bool checkDBconfig();
//------------------------------------------------------------------------

static CollectorList * Collectors = NULL;
static	QueryResult result;
static	CondorQuery	quillQuery(SCHEDD_AD);
static	ClassAdList	quillList;

int
main(int argc, char* argv[])
{
  Collectors = NULL;
  HistorySnapshot *historySnapshot;
  SQLQuery queryhor;
  SQLQuery queryver;
  void **parameters;
  char *dbconn;
  int cluster=-1, proc=-1;
  char *completedsince = NULL;
  char *owner=NULL;
  bool readfromfile = false,remotequill=false;

  char* JobHistoryFileName=NULL;
  char *dbIpAddr=NULL, *dbName=NULL,*quillName=NULL;
  bool longformat=false;
  char* constraint=NULL;
  ExprTree *constraintExpr=NULL;

  int EndFlag=0;
  int ErrorFlag=0;
  int EmptyFlag=0;
  AttrList *ad=0;

  int st = 0;
  int flag = 1;

  char tmp[512];

  int i;
  parameters = (void **) malloc(NUM_PARAMETERS * sizeof(void *));
  myDistro->Init( argc, argv );

  queryhor.setQuery(HISTORY_ALL_HOR, NULL);
  queryver.setQuery(HISTORY_ALL_VER, NULL);


  readfromfile = checkDBconfig();

  for(i=1; i<argc; i++) {
    if (strcmp(argv[i],"-l")==0) {
      longformat=TRUE;   
    }
    else if(strcmp(argv[i], "-name")==0) {
      i++;
      if (argc <= i) {
	fprintf( stderr,
		 "Error: Argument -name requires the name of a quilld as a parameter\n" );
	exit(1);
      }

      if( !(quillName = get_daemon_name(argv[i])) ) {
	fprintf( stderr, "Error: unknown host %s\n",
		 get_host_part(argv[i]) );
	  printf("\n");
	  print_wrapped_text("Extra Info: The name given with the -name "
			     "should be the name of a condor_quilld process. "
			     "Normally it is either a hostname, or "
			     "\"name@hostname\". "
			     "In either case, the hostname should be the Internet "
			     "host name, but it appears that it wasn't.",
			     stderr);
	  exit(1);
      }
      sprintf (tmp, "%s == \"%s\"", ATTR_NAME, quillName);      
      
      quillQuery.addORConstraint (tmp);
      remotequill = true;
      readfromfile = false;
    }
    else if (strcmp(argv[i],"-f")==0) {
      if (i+1==argc || JobHistoryFileName) break;
      i++;
      JobHistoryFileName=argv[i];
      readfromfile = true;
    }
    else if (strcmp(argv[i],"-help")==0) {
	  Usage(argv[0]);
    }
    else if (strcmp(argv[i],"-constraint")==0) {
      if (i+1==argc || constraint) break;
      sprintf(tmp,"(%s)",argv[i+1]);
      constraint=tmp;
      i++;
      readfromfile = true;
    }
    else if (strcmp(argv[i],"-completedsince")==0) {
      i++;
      if (argc <= i) {
	fprintf(stderr,
		"Error: Argument -completedsince requires a date and optional timestamp as a parameter.\n");
	fprintf(stderr,
		"\t\te.g. condor_history -completedsince \"2004-10-19 10:23:54\"\n");
	exit(1);
      }

      if (constraint) break;
      constraint = completedsince;
      completedsince = strdup(argv[i]);
      parameters[0] = completedsince;
      queryhor.setQuery(HISTORY_COMPLETEDSINCE_HOR,parameters);
      queryver.setQuery(HISTORY_COMPLETEDSINCE_VER,parameters);
    }
    else if (sscanf (argv[i], "%d.%d", &cluster, &proc) == 2) {
      if (constraint) break;
      sprintf (tmp, "((%s == %d) && (%s == %d))", 
               ATTR_CLUSTER_ID, cluster,ATTR_PROC_ID, proc);
      constraint=tmp;
      parameters[0] = &cluster;
      parameters[1] = &proc;
      queryhor.setQuery(HISTORY_CLUSTER_PROC_HOR, parameters);
      queryver.setQuery(HISTORY_CLUSTER_PROC_VER, parameters);
    }
    else if (sscanf (argv[i], "%d", &cluster) == 1) {
      if (constraint) break;
      sprintf (tmp, "(%s == %d)", ATTR_CLUSTER_ID, cluster);
      constraint=tmp;
      parameters[0] = &cluster;
      queryhor.setQuery(HISTORY_CLUSTER_HOR, parameters);
      queryver.setQuery(HISTORY_CLUSTER_VER, parameters);
    }
    else {
      if (constraint) break;
      owner = (char *) malloc(512 * sizeof(char));
      sscanf(argv[i], "%s", owner);	
      sprintf(tmp, "(%s == \"%s\")", ATTR_OWNER, owner);
      constraint=tmp;
      parameters[0] = owner;
      queryhor.setQuery(HISTORY_OWNER_HOR, parameters);
      queryver.setQuery(HISTORY_OWNER_VER, parameters);
    }
  }
  if (i<argc) Usage(argv[0]);

  if (constraint) puts(constraint);

  config();

  if( constraint && Parse( constraint, constraintExpr ) ) {
     fprintf( stderr, "Error:  could not parse constraint %s\n", constraint );
     exit( 1 );
  }


  if(readfromfile) {
    //printf("Reading From File\n");
    if (!JobHistoryFileName) {
      JobHistoryFileName=param("HISTORY");
    }
    FILE* LogFile=fopen(JobHistoryFileName,"r");
    if (!LogFile) {
      fprintf(stderr,"History file not found or empty.\n");
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
	delete ad;
	continue;
      } 
      if( EmptyFlag ) {
	EmptyFlag=0;
	delete ad;
	continue;
      }
      if (!constraint || EvalBool(ad, constraintExpr)) {
	if (longformat) { 
	  ad->fPrint(stdout); printf("\n"); 
	} else {
	  displayJobShort(ad);
	}
      }
      delete ad;
    }
    fclose(LogFile);
  }

  
  else {
    if(remotequill) {
      if (Collectors == NULL) {
	Collectors = CollectorList::create();
      }
      result = Collectors->query ( quillQuery, quillList );

	  if(quillList.MyLength() == 0) {
		  printf("Error: Unknown quill server %s\n", quillName);
		  exit(1);
	  }

      quillList.Open();
      while ((ad = quillList.Next())) {
			  // get the address of the database
		  dbIpAddr = (char *) malloc(64 * sizeof(char));
		  dbName = (char *) malloc(64 * sizeof(char));
		  if (!ad->LookupString("DatabaseIpAddr", dbIpAddr) ||
			  !ad->LookupString("DatabaseName", dbName) || 
			  (ad->LookupInteger("IsRemotelyQueryable",flag) && !flag)) {
			  printf("Error: The quill daemon \"%s\" is not set up for database queries\n", quillName);
			  exit(1);
	}
      }
    }
    dbconn = getDBConnStr(quillName,dbIpAddr,dbName);
    historySnapshot = new HistorySnapshot(dbconn);
    printf ("\n\n-- Quill: %s : %s : %s\n", quillName, 
	    dbIpAddr, dbName);

    st = historySnapshot->sendQuery(&queryhor, &queryver, longformat);

    // query history table
    if (st == 0)
      printf("No historical jobs in the database match your query\n");
    
    historySnapshot->release();
    delete(historySnapshot);
  }

  if(owner) free(owner);
  if(completedsince) free(completedsince);
  if(parameters) free(parameters);
  if(dbIpAddr) free(dbIpAddr);
  if(dbName) free(dbName);
  if(quillName) free(quillName);
  if(dbconn) free(dbconn);
  return 0;
}


//------------------------------------------------------------------------

static bool EvalBool(AttrList* ad, ExprTree *tree)
{
  EvalResult result;
  
  // Evaluate constraint with ad in the target scope so that constraints
  // have the same semantics as the collector queries.  --RR
  if (!tree->EvalTree(NULL, ad, &result)) {
        // dprintf(D_ALWAYS, "can't evaluate constraint: %s\n", constraint);
    delete tree;
    return false;
  }
  
  if (result.type == LX_INTEGER) {
    return (bool)result.i;
  }
  
  return false;
}

/* this function for checking whether database can be used for querying in local machine */
static bool checkDBconfig() {
  if (!param("QUILL_NAME") || 
      !param("DATABASE_IPADDRESS") || 
      !param("DATABASE_NAME")) {
    return FALSE;
  }
  else
    return TRUE;
}


static char * getDBConnStr(char *&quillName, char *&databaseIp, char *&databaseName) {
  char            host[64],port[10];
  char            *tmpquillname, *tmpdatabaseip, *tmpdatabasename;
  
  if((!quillName && !(tmpquillname = param("QUILL_NAME"))) ||
     (!databaseIp && !(tmpdatabaseip = param("DATABASE_IPADDRESS"))) || 
     (!databaseName && !(tmpdatabasename = param("DATABASE_NAME")))) {
    fprintf( stderr, "Error: Could not find local quill info in condor_config file\n");
    fprintf(stderr, "\n");
    print_wrapped_text("Extra Info: "
		       "The most likely cause for this error "
		       "is that you have not defined QUILL_NAME/DATABASE_IPADDRESS/DATABASE_NAME "
		       "in the condor_config file.  You must "
		       "define this variable in the config file", stderr);
    exit( 1 );
  }

  if(!quillName) {
	  quillName = tmpquillname;
		  //quillName = (char *) malloc(64 * sizeof(char));
		  //strcpy(quillName, param("QUILL_NAME"));
  }
  if(!databaseIp) {
	  databaseIp = tmpdatabaseip;
		  //databaseIp= (char *) malloc(64 * sizeof(char));
		  //strcpy(databaseIp, param("DATABASE_IPADDRESS"));
  }
  if(!databaseName) {
	  databaseName = tmpdatabasename;
		  //databaseName = (char *) malloc(64 * sizeof(char));    
		  //strcpy(databaseName, param("DATABASE_NAME"));
  }
  char *ptr_colon = strchr(databaseIp, ':');
  strcpy(host, "host= ");
  strncat(host, 
	  databaseIp+1, 
	  ptr_colon - databaseIp - 1);
  strcpy(port, "port= ");
  strcat(port, ptr_colon+1);
  port[strlen(port)-1] = '\0';
  
  char *dbconn = (char *) malloc(128 * sizeof(char));
  sprintf(dbconn, "%s %s user=quill dbname=%s", host, port, databaseName);
  return dbconn;
}
