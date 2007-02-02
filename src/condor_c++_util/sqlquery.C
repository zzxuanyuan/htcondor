/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "sqlquery.h"
#include "quill_enums.h"
#include "condor_config.h"

#define quill_history_hor_select_list "scheddname, cluster_id, proc_id, qdate, owner, globaljobid, numckpts, numrestarts, numsystemholds, condorversion, condorplatform, rootdir, Iwd, jobuniverse, cmd, minhosts, maxhosts, jobprio, user_j, env, userlog, coresize, killsig, rank, in_j, transferin, out, transferout, err, transfererr, shouldtransferfiles, transferfiles, executablesize, diskusage, requirements, filesystemdomain, args, lastmatchtime, numjobmatches, jobstartdate, jobcurrentstartdate, jobruncount, filereadcount, filereadbytes, filewritecount, filewritebytes, fileseekcount, totalsuspensions, imagesize, exitstatus, localusercpu, localsyscpu, remoteusercpu, remotesyscpu, bytessent, bytesrecvd, rscbytessent, rscbytesrecvd, exitcode, jobstatus, enteredcurrentstatus, remotewallclocktime, lastremotehost, completiondate"

#define quill_history_ver_select_list "scheddname, cluster_id, proc_id, attr, val"

#define quill_avg_time_template_pgsql "SELECT avg((now() - 'epoch'::timestamp with time zone) - cast(QDate || ' seconds' as interval)) \
         FROM \
           (SELECT \
             c.QDate AS QDate, \
             (CASE WHEN p.JobStatus ISNULL THEN c.JobStatus ELSE p.JobStatus END) AS JobStatus \
            FROM (select cluster_id, proc_id, \
          	        NULL AS QDate, \
              	    jobstatus AS JobStatus \
	              FROM procads_horizontal where cluster_id != 0) AS p, \
                 (select cluster_id, \
 	                qdate AS QDate, \
	                jobstatus AS JobStatus \
	              FROM clusterads_horizontal where cluster_id != 0) AS c \
            WHERE p.cluster_id = c.cluster_id) AS h \
         WHERE (jobStatus != '3'::text) and (jobstatus != '4'::text) t"


/* oracle doesn't support avg over time interval, therefore, the following 
   expression converts to difference in seconds between two timestamps and 
   then compute avg over seconds passed.

   After the averge of number of seconds is computed, we convert the avg back 
   to the format of 'days hours:minutes:seconds' for display.
*/
#define quill_avg_time_template_oracle "SELECT floor(t.elapsed/86400) || ' ' || floor(mod(t.elapsed, 86400)/3600) || ':' || floor(mod(t.elapsed, 3600)/60) || ':' || floor(mod(t.elapsed, 60))  FROM (SELECT avg((extract(day from (current_timestamp - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD'))) - floor(QDate/86400))*86400 + (extract(hour from (current_timestamp - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD'))) - floor(mod(QDate, 86400)/3600))*3600 + (extract(minute from (current_timestamp - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD'))) - floor(mod(QDate, 3600)/60))*60 + (extract(second from (current_timestamp - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD'))) - mod(QDate, 60))) as elapsed \
         FROM \
           (SELECT \
             c.QDate AS QDate, \
             (CASE WHEN p.JobStatus ISNULL THEN c.JobStatus ELSE p.JobStatus END) AS JobStatus \
            FROM (select cluster_id, proc_id, \
          	        NULL AS QDate, \
              	    jobstatus AS JobStatus \
	              FROM quillwriter.procads_horizontal where cluster_id != 0) AS p, \
                 (select cluster_id, \
 	                qdate AS QDate, \
	                jobstatus AS JobStatus \
	              FROM quillwriter.clusterads_horizontal where cluster_id != 0) AS c \
            WHERE p.cluster_id = c.cluster_id) AS h \
         WHERE (jobStatus != '3'::text) and (jobstatus != '4'::text)) t"

SQLQuery::
SQLQuery ()
{
  query_str = 0;
  declare_cursor_str = 0;
  fetch_cursor_str = 0;
  close_cursor_str = 0;
  scheddname = 0;
  jobqueuebirthdate = 0;
}

SQLQuery::
SQLQuery (query_types qtype, void **parameters)
{
	createQueryString(qtype,parameters);
}


SQLQuery::
~SQLQuery ()
{
  if(query_str) 
	  free(query_str);
  if(declare_cursor_str) 
	  free(declare_cursor_str);
  if(fetch_cursor_str) 
	  free(fetch_cursor_str);
  if(close_cursor_str) 
	  free(close_cursor_str);
  if(scheddname) 
	  free(scheddname);
}

query_types SQLQuery::
getType() 
{
  return type;
}

char * SQLQuery::
getQuery() 
{
  return query_str;
}

char * SQLQuery::
getDeclareCursorStmt() 
{
  return declare_cursor_str;
}

char * SQLQuery::
getFetchCursorStmt() 
{
  return fetch_cursor_str;
}

char * SQLQuery::
getCloseCursorStmt() 
{
  return close_cursor_str;
}

void SQLQuery::
setQuery(query_types qtype, void **parameters) 
{
  createQueryString(qtype, parameters);
}

void SQLQuery::
setScheddname(char *name) 
{
	if(name) {
		scheddname = strdup(scheddname);
	}
}

void SQLQuery::
setJobqueuebirthdate(time_t birthdate)
{
	jobqueuebirthdate = birthdate;
}
int SQLQuery::
createQueryString(query_types qtype, void **parameters) {
  char *tmp;
  dbtype dt;

  tmp = param("QUILL_DB_TYPE");
  if (tmp) {
	  if (strcasecmp(tmp, "ORACLE") == 0) {
		  dt = T_ORACLE;
	  } else if (strcasecmp(tmp, "PGSQL") == 0) {
		  dt = T_PGSQL;
	  }
  } else {
	  dt = T_PGSQL; // assume PGSQL by default
  }
    
  query_str = (char *) malloc(MAX_QUERY_SIZE * sizeof(char));
  declare_cursor_str = (char *) malloc(MAX_QUERY_SIZE * sizeof(char));
  fetch_cursor_str = (char *) malloc(MAX_QUERY_SIZE * sizeof(char));
  close_cursor_str = (char *) malloc(MAX_QUERY_SIZE * sizeof(char));

  switch(qtype) {
    
  case HISTORY_ALL_HOR:
	  if (dt == T_PGSQL) {
			sprintf(declare_cursor_str, 
					"DECLARE HISTORY_ALL_HOR_CUR CURSOR FOR SELECT %s FROM Jobs_Horizontal_History ORDER BY scheddname, cluster_id, proc_id;", quill_history_hor_select_list);
			sprintf(fetch_cursor_str,
				"FETCH FORWARD 100 FROM HISTORY_ALL_HOR_CUR");
			sprintf(close_cursor_str,
				"CLOSE HISTORY_ALL_HOR_CUR");
	  } else if (dt == T_ORACLE) {
		  sprintf(query_str, 
				  "SELECT %s FROM quillwriter.Jobs_Horizontal_History ORDER BY scheddname, cluster_id, proc_id", quill_history_hor_select_list);
	  }
    
    break;
  case HISTORY_ALL_VER:
	  if (dt == T_PGSQL) {
		sprintf(declare_cursor_str, 
			"DECLARE HISTORY_ALL_VER_CUR CURSOR FOR SELECT %s FROM Jobs_Vertical_History ORDER BY scheddname, cluster_id, proc_id;", quill_history_ver_select_list);
		sprintf(fetch_cursor_str,
			"FETCH FORWARD 5000 FROM HISTORY_ALL_VER_CUR");
		sprintf(close_cursor_str,
			"CLOSE HISTORY_ALL_VER_CUR");
	  } else if (dt == T_ORACLE) {
		  sprintf(query_str, 
				  "SELECT %s FROM quillwriter.Jobs_Vertical_History ORDER BY scheddname, cluster_id, proc_id", quill_history_ver_select_list);
	  }

    break;
  case HISTORY_CLUSTER_HOR:
	  if (dt == T_PGSQL) {
		sprintf(declare_cursor_str, 
			"DECLARE HISTORY_CLUSTER_HOR_CUR CURSOR FOR SELECT %s FROM Jobs_Horizontal_History WHERE cluster_id=%d ORDER BY scheddname, cluster_id, proc_id;",
				quill_history_hor_select_list, 
				*((int *)parameters[0]));
		sprintf(fetch_cursor_str,
			"FETCH FORWARD 100 FROM HISTORY_CLUSTER_HOR_CUR");
		sprintf(close_cursor_str,
			"CLOSE HISTORY_CLUSTER_HOR_CUR");
	  } else if (dt == T_ORACLE) {
		  sprintf(query_str, 
				  "SELECT %s FROM quillwriter.Jobs_Horizontal_History WHERE cluster_id=%d ORDER BY scheddname, cluster_id, proc_id",
				  quill_history_hor_select_list, 
				  *((int *)parameters[0]));
	  }

    break;
  case HISTORY_CLUSTER_VER:
	  if (dt == T_PGSQL) {
		sprintf(declare_cursor_str, 
			"DECLARE HISTORY_CLUSTER_VER_CUR CURSOR FOR SELECT %s FROM Jobs_Vertical_History WHERE cluster_id=%d ORDER BY scheddname, cluster_id, proc_id;",
				quill_history_ver_select_list,
				*((int *)parameters[0]));
		sprintf(fetch_cursor_str,
			"FETCH FORWARD 5000 FROM HISTORY_CLUSTER_VER_CUR");
		sprintf(close_cursor_str,
			"CLOSE HISTORY_CLUSTER_VER_CUR");
	  } else if (dt == T_ORACLE) {
		  sprintf(query_str, 
				  "SELECT %s FROM quillwriter.Jobs_Vertical_History WHERE cluster_id=%d ORDER BY scheddname, cluster_id, proc_id",
				  quill_history_ver_select_list, 
				  *((int *)parameters[0]));
	  }
    break;
  case HISTORY_CLUSTER_PROC_HOR:
	  if (dt == T_PGSQL) {
    	sprintf(declare_cursor_str, 
			"DECLARE HISTORY_CLUSTER_PROC_HOR_CUR CURSOR FOR SELECT %s FROM Jobs_Horizontal_History WHERE cluster_id=%d and proc_id=%d ORDER BY scheddname, cluster_id, proc_id;",
				quill_history_hor_select_list,
				*((int *)parameters[0]), *((int *)parameters[1]));
		sprintf(fetch_cursor_str,
			"FETCH FORWARD 100 FROM HISTORY_CLUSTER_PROC_HOR_CUR");
		sprintf(close_cursor_str,
			"CLOSE HISTORY_CLUSTER_PROC_HOR_CUR");
	  } else if (dt == T_ORACLE) {
		  sprintf(query_str, 
				  "SELECT %s FROM quillwriter.Jobs_Horizontal_History WHERE cluster_id=%d and proc_id=%d ORDER BY scheddname, cluster_id, proc_id",
				  quill_history_hor_select_list, 
				  *((int *)parameters[0]), *((int *)parameters[1]));
	  }

    break;  
  case HISTORY_CLUSTER_PROC_VER:
	  if (dt == T_PGSQL) {
    	sprintf(declare_cursor_str, 
			"DECLARE HISTORY_CLUSTER_PROC_VER_CUR CURSOR FOR SELECT %s FROM Jobs_Vertical_History WHERE cluster_id=%d and proc_id=%d "
			"ORDER BY scheddname, cluster_id, proc_id;",
				quill_history_ver_select_list, 
				*((int *)parameters[0]), *((int *)parameters[1]));
		sprintf(fetch_cursor_str,
			"FETCH FORWARD 5000 FROM HISTORY_CLUSTER_PROC_VER_CUR");
		sprintf(close_cursor_str,
			"CLOSE HISTORY_CLUSTER_PROC_VER_CUR");
	  } else if (dt == T_ORACLE) {
		  sprintf(query_str, 
				  "SELECT %s FROM quillwriter.Jobs_Vertical_History WHERE cluster_id=%d and proc_id=%d "
				  "ORDER BY scheddname, cluster_id, proc_id",
				  quill_history_ver_select_list, 
				  *((int *)parameters[0]), *((int *)parameters[1]));
	  }

    break;  
  case HISTORY_OWNER_HOR:
	  if (dt == T_PGSQL) {
		sprintf(declare_cursor_str,
			"DECLARE HISTORY_OWNER_HOR_CUR CURSOR FOR SELECT %s FROM Jobs_Horizontal_History WHERE \"Owner\"='\"%s\"' "
			"ORDER BY scheddname, cluster_id, proc_id;",
				quill_history_hor_select_list,
			((char *)parameters[0]));
		sprintf(fetch_cursor_str,
			"FETCH FORWARD 100 FROM HISTORY_OWNER_HOR_CUR");
		sprintf(close_cursor_str,
			"CLOSE HISTORY_OWNER_HOR_CUR");
	  } else if (dt == T_ORACLE) {
		  sprintf(query_str,
				  "SELECT %s FROM quillwriter.Jobs_Horizontal_History WHERE \"Owner\"='\"%s\"' "
				  "ORDER BY scheddname, cluster_id,proc_id",
				  quill_history_hor_select_list,
				  ((char *)parameters[0]));
	  }

	  break;
  case HISTORY_OWNER_VER:
	  if (dt == T_PGSQL) {
		sprintf(declare_cursor_str,
			"DECLARE HISTORY_OWNER_VER_CUR CURSOR FOR SELECT hv.cluster_id,hv.proc_id,hv.attr,hv.val FROM "
			"Jobs_Horizontal_History hh, Jobs_Vertical_History hv "
			"WHERE hh.cluster_id=hv.cluster_id AND hh.proc_id=hv.proc_id AND hh.\"Owner\"='\"%s\"'"
			" ORDER BY scheddname, cluster_id,proc_id;",
			((char *)parameters[0]));
		sprintf(fetch_cursor_str,
			"FETCH FORWARD 5000 FROM HISTORY_OWNER_VER_CUR");
		sprintf(close_cursor_str,
			"CLOSE HISTORY_OWNER_VER_CUR");
	  } else if (dt == T_ORACLE) {
		  sprintf(query_str,
				  "SELECT hv.cluster_id,hv.proc_id,hv.attr,hv.val FROM "
				  "quillwriter.Jobs_Horizontal_History hh, quillwriter.Jobs_Vertical_History hv "
				  "WHERE hh.cluster_id=hv.cluster_id AND hh.proc_id=hv.proc_id AND hh.\"Owner\"='\"%s\"'"
				  " ORDER BY scheddname, cluster_id,proc_id",
				  ((char *)parameters[0]));
	  }

	  break;
  case HISTORY_COMPLETEDSINCE_HOR:
	  if (dt == T_PGSQL) {
		sprintf(declare_cursor_str,
			"DECLARE HISTORY_COMPLETEDSINCE_HOR_CUR CURSOR FOR SELECT %s FROM Jobs_Horizontal_History "
			"WHERE \"CompletionDate\" > "
			"date_part('epoch', '%s'::timestamp with time zone) "
			"ORDER BY scheddname, cluster_id,proc_id;",
				quill_history_hor_select_list,
				((char *)parameters[0]));
		sprintf(fetch_cursor_str,
			"FETCH FORWARD 100 FROM HISTORY_COMPLETEDSINCE_HOR_CUR");
		sprintf(close_cursor_str,
			"CLOSE HISTORY_COMPLETEDSINCE_HOR_CUR");
	  } else if (dt == T_ORACLE)
		  sprintf(query_str,
				  "SELECT %s FROM quillwriter.Jobs_Horizontal_History "
				  "WHERE (to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD') + to_dsinterval(floor(\"CompletionDate\"/86400) || ' ' || floor(mod(\"CompletionDate\",86400)/3600) || ':' || floor(mod(\"CompletionDate\", 3600)/60) || ':' || mod(\"CompletionDate\", 60))) > "
				  "to_timestamp_tz('%s', 'MM/DD/YYYY HH24:MI:SS TZD') "
				  "ORDER BY scheddname, cluster_id,proc_id",
				  quill_history_hor_select_list,
				  ((char *)parameters[0]));
		  
	  break;
  case HISTORY_COMPLETEDSINCE_VER:
	  if (dt == T_PGSQL) {
		sprintf(declare_cursor_str,
			"SET enable_mergejoin=false; "
			"DECLARE HISTORY_COMPLETEDSINCE_VER_CUR CURSOR FOR SELECT hv.cluster_id,hv.proc_id,hv.attr,hv.val FROM "
			"Jobs_Horizontal_History hh, Jobs_Vertical_History hv "
			"WHERE hh.cluster_id=hv.cluster_id AND hh.proc_id=hv.proc_id AND "
			"hh.\"CompletionDate\" > "
			"date_part('epoch', '%s'::timestamp with time zone) "
			"ORDER BY hh.scheddname, hh.cluster_id,hh.proc_id;",
			((char *)parameters[0]));
		sprintf(fetch_cursor_str,
			"FETCH FORWARD 5000 FROM HISTORY_COMPLETEDSINCE_VER_CUR");
		sprintf(close_cursor_str,
			"CLOSE HISTORY_COMPLETEDSINCE_VER_CUR");
	}
	  else if (dt == T_ORACLE)
		  sprintf(query_str,
				  "SELECT hv.cluster_id,hv.proc_id,hv.attr,hv.val FROM "
				  "quillwriter.Jobs_Horizontal_History hh, quillwriter.Jobs_Vertical_History hv "
				  "WHERE hh.cluster_id=hv.cluster_id AND hh.proc_id=hv.proc_id AND "
				  "(to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD') + to_dsinterval(floor(hh.\"CompletionDate\"/86400) || ' ' || floor(mod(hh.\"CompletionDate\",86400)/3600) || ':' || floor(mod(hh.\"CompletionDate\", 3600)/60) || ':' || mod(hh.\"CompletionDate\", 60))) > "
				  "to_timestamp_tz('%s', 'MM/DD/YYYY HH24:MI:SS TZD') "
				  "ORDER BY hh.scheddname, hh.cluster_id,hh.proc_id",
				  ((char *)parameters[0]));
	  break;
  case QUEUE_AVG_TIME:
	  if (dt == T_PGSQL) 
		  sprintf(query_str, quill_avg_time_template_pgsql);
	  else if (dt == T_ORACLE) 
		  sprintf(query_str, quill_avg_time_template_oracle);
	  break;
	  
  default:
	EXCEPT("Incorrect query type specified\n");
	return -1;
  }
  return 1;
}

void SQLQuery::
Print() 
{
  printf("Query = %s\n", query_str);
  printf("DeclareCursorStmt = %s\n", declare_cursor_str);
  printf("FetchCursorStmt = %s\n", fetch_cursor_str);
  printf("CloseCursorStmt = %s\n", close_cursor_str);
}

