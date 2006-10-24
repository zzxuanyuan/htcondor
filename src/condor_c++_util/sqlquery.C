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

#define avg_time_template_pgsql "SELECT avg((now() - 'epoch'::timestamp with time zone) - cast(QDate || ' seconds' as interval)) \
         FROM \
           (SELECT \
             c.QDate AS QDate, \
             (CASE WHEN p.JobStatus ISNULL THEN c.JobStatus ELSE p.JobStatus END) AS JobStatus \
            FROM (select cluster_id, proc, \
          	        NULL AS QDate, \
              	    jobstatus AS JobStatus \
	              FROM procads_horizontal where cluster_id != 0) AS p, \
                 (select cluster_id, \
 	                qdate AS QDate, \
	                jobstatus AS JobStatus \
	              FROM clusterads_horizontal where cluster_id != 0) AS c \
            WHERE p.cid = c.cid) AS h \
         WHERE (jobStatus != '3'::text) and (jobstatus != '4'::text) t"


/* oracle doesn't support avg over time interval, therefore, the following 
   expression converts to difference in seconds between two timestamps and 
   then compute avg over seconds passed.

   After the averge of number of seconds is computed, we convert the avg back 
   to the format of 'days hours:minutes:seconds' for display.
*/
#define avg_time_template_oracle "SELECT floor(t.elapsed/86400) || ' ' || floor(mod(t.elapsed, 86400)/3600) || ':' || floor(mod(t.elapsed, 3600)/60) || ':' || floor(mod(t.elapsed, 60))  FROM (SELECT avg((extract(day from (current_timestamp - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD'))) - floor(QDate/86400))*86400 + (extract(hour from (current_timestamp - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD'))) - floor(mod(QDate, 86400)/3600))*3600 + (extract(minute from (current_timestamp - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD'))) - floor(mod(QDate, 3600)/60))*60 + (extract(second from (current_timestamp - to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD'))) - mod(QDate, 60))) as elapsed \
         FROM \
           (SELECT \
             c.QDate AS QDate, \
             (CASE WHEN p.JobStatus ISNULL THEN c.JobStatus ELSE p.JobStatus END) AS JobStatus \
            FROM (select cluster_id, proc, \
          	        NULL AS QDate, \
              	    jobstatus AS JobStatus \
	              FROM quillwriter.procads_horizontal where cluster_id != 0) AS p, \
                 (select cluster_id, \
 	                qdate AS QDate, \
	                jobstatus AS JobStatus \
	              FROM quillwriter.clusterads_horizontal where cluster_id != 0) AS c \
            WHERE p.cid = c.cid) AS h \
         WHERE (jobStatus != '3'::text) and (jobstatus != '4'::text)) t"

SQLQuery::
SQLQuery ()
{
  query_str = 0;
}

SQLQuery::
SQLQuery (query_types qtype, void **parameters)
{
	createQueryString(qtype,parameters);
}


SQLQuery::
~SQLQuery ()
{
  if(query_str) free(query_str);
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

void SQLQuery::
setQuery(query_types qtype, void **parameters) 
{
  createQueryString(qtype, parameters);
}

int SQLQuery::
createQueryString(query_types qtype, void **parameters) {
  char *tmp;
  dbtype dt;

  tmp = param("QUILLPP_DB_TYPE");
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
  switch(qtype) {
    
  case HISTORY_ALL_HOR:
	  if (dt == T_PGSQL) {
		  sprintf(query_str, 
				  "SELECT * FROM HISTORY_HORIZONTAL ORDER BY cluster_id, proc");
	  } else if (dt == T_ORACLE) {
		  sprintf(query_str, 
				  "SELECT * FROM quillwriter.HISTORY_HORIZONTAL ORDER BY cluster_id, proc");
	  }
    
    break;
  case HISTORY_ALL_VER:
	  if (dt == T_PGSQL) {
		  sprintf(query_str, 
				  "SELECT * FROM HISTORY_VERTICAL ORDER BY cluster_id, proc");
	  } else if (dt == T_ORACLE) {
		  sprintf(query_str, 
				  "SELECT * FROM quillwriter.HISTORY_VERTICAL ORDER BY cluster_id, proc");
	  }

    break;
  case HISTORY_CLUSTER_HOR:
	  if (dt == T_PGSQL) {
		  sprintf(query_str, 
				  "SELECT * FROM HISTORY_HORIZONTAL WHERE cluster_id=%d ORDER BY cluster_id, proc",
				  *((int *)parameters[0]));
	  } else if (dt == T_ORACLE) {
		  sprintf(query_str, 
				  "SELECT * FROM quillwriter.HISTORY_HORIZONTAL WHERE cluster_id=%d ORDER BY cluster_id, proc",
				  *((int *)parameters[0]));
	  }

    break;
  case HISTORY_CLUSTER_VER:
	  if (dt == T_PGSQL) {
		  sprintf(query_str, 
				  "SELECT * FROM HISTORY_VERTICAL WHERE cluster_id=%d ORDER BY cluster_id, proc",
				  *((int *)parameters[0]));		  
	  } else if (dt == T_ORACLE) {
		  sprintf(query_str, 
				  "SELECT * FROM quillwriter.HISTORY_VERTICAL WHERE cluster_id=%d ORDER BY cluster_id, proc",
				  *((int *)parameters[0]));
	  }
    break;
  case HISTORY_CLUSTER_PROC_HOR:
	  if (dt == T_PGSQL) {
		  sprintf(query_str, 
				  "SELECT * FROM HISTORY_HORIZONTAL WHERE cluster_id=%d and proc=%d ORDER BY cluster_id, proc",
				  *((int *)parameters[0]), *((int *)parameters[1]));
	  } else if (dt == T_ORACLE) {
		  sprintf(query_str, 
				  "SELECT * FROM quillwriter.HISTORY_HORIZONTAL WHERE cluster_id=%d and proc=%d ORDER BY cluster_id, proc",
				  *((int *)parameters[0]), *((int *)parameters[1]));
	  }

    break;  
  case HISTORY_CLUSTER_PROC_VER:
	  if (dt == T_PGSQL) {
		  sprintf(query_str, 
				  "SELECT * FROM HISTORY_VERTICAL WHERE cluster_id=%d and proc=%d "
				  "ORDER BY cluster_id, proc",
				  *((int *)parameters[0]), *((int *)parameters[1]));
	  } else if (dt == T_ORACLE) {
		  sprintf(query_str, 
				  "SELECT * FROM quillwriter.HISTORY_VERTICAL WHERE cluster_id=%d and proc=%d "
				  "ORDER BY cluster_id, proc",
				  *((int *)parameters[0]), *((int *)parameters[1]));
	  }

    break;  
  case HISTORY_OWNER_HOR:
	  if (dt == T_PGSQL) {
		  sprintf(query_str,
				  "SELECT * FROM HISTORY_HORIZONTAL WHERE \"Owner\"='\"%s\"' "
				  "ORDER BY cluster_id,proc",
				  ((char *)parameters[0]));
	  } else if (dt == T_ORACLE) {
		  sprintf(query_str,
				  "SELECT * FROM quillwriter.HISTORY_HORIZONTAL WHERE \"Owner\"='\"%s\"' "
				  "ORDER BY cluster_id,proc",
				  ((char *)parameters[0]));
	  }

	  break;
  case HISTORY_OWNER_VER:
	  if (dt == T_PGSQL) {
		  sprintf(query_str,
				  "SELECT hv.cluster_id,hv.proc,hv.attr,hv.val FROM "
				  "HISTORY_HORIZONTAL hh, HISTORY_VERTICAL hv "
				  "WHERE hh.cluster_id=hv.cluster_id AND hh.proc=hv.proc AND hh.\"Owner\"='\"%s\"'"
				  " ORDER BY cluster_id,proc",
				  ((char *)parameters[0]));
	  } else if (dt == T_ORACLE) {
		  sprintf(query_str,
				  "SELECT hv.cluster_id,hv.proc,hv.attr,hv.val FROM "
				  "quillwriter.HISTORY_HORIZONTAL hh, quillwriter.HISTORY_VERTICAL hv "
				  "WHERE hh.cluster_id=hv.cluster_id AND hh.proc=hv.proc AND hh.\"Owner\"='\"%s\"'"
				  " ORDER BY cluster_id,proc",
				  ((char *)parameters[0]));
	  }

	  break;
  case HISTORY_COMPLETEDSINCE_HOR:
	  if (dt == T_PGSQL)
		  sprintf(query_str,
				  "SELECT * FROM History_Horizontal "
				  "WHERE \"CompletionDate\" > "
				  "date_part('epoch', '%s'::timestamp with time zone) "
				  "ORDER BY cluster_id,proc",
				  ((char *)parameters[0]));
	  else if (dt == T_ORACLE)
		  sprintf(query_str,
				  "SELECT * FROM quillwriter.History_Horizontal "
				  "WHERE (to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD') + to_dsinterval(floor(\"CompletionDate\"/86400) || ' ' || floor(mod(\"CompletionDate\",86400)/3600) || ':' || floor(mod(\"CompletionDate\", 3600)/60) || ':' || mod(\"CompletionDate\", 60))) > "
				  "to_timestamp_tz('%s', 'MM/DD/YYYY HH24:MI:SS TZD') "
				  "ORDER BY cluster_id,proc",
				  ((char *)parameters[0]));
		  
	  break;
  case HISTORY_COMPLETEDSINCE_VER:
	  if (dt == T_PGSQL)
		  sprintf(query_str,
				  "SET enable_mergejoin=false; "
				  "SELECT hv.cluster_id,hv.proc,hv.attr,hv.val FROM "
				  "HISTORY_HORIZONTAL hh, HISTORY_VERTICAL hv "
				  "WHERE hh.cluster_id=hv.cluster_id AND hh.proc=hv.proc AND "
				  "hh.\"CompletionDate\" > "
				  "date_part('epoch', '%s'::timestamp with time zone) "
				  "ORDER BY hh.cluster_id,hh.proc",
				  ((char *)parameters[0]));
	  else if (dt == T_ORACLE)
		  sprintf(query_str,
				  "SELECT hv.cluster_id,hv.proc,hv.attr,hv.val FROM "
				  "quillwriter.HISTORY_HORIZONTAL hh, quillwriter.HISTORY_VERTICAL hv "
				  "WHERE hh.cluster_id=hv.cluster_id AND hh.proc=hv.proc AND "
				  "(to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD') + to_dsinterval(floor(hh.\"CompletionDate\"/86400) || ' ' || floor(mod(hh.\"CompletionDate\",86400)/3600) || ':' || floor(mod(hh.\"CompletionDate\", 3600)/60) || ':' || mod(hh.\"CompletionDate\", 60))) > "
				  "to_timestamp_tz('%s', 'MM/DD/YYYY HH24:MI:SS TZD') "
				  "ORDER BY hh.cluster_id,hh.proc",
				  ((char *)parameters[0]));
	  break;
  case QUEUE_AVG_TIME:
	  if (dt == T_PGSQL) 
		  sprintf(query_str, avg_time_template_pgsql);
	  else if (dt == T_ORACLE) 
		  sprintf(query_str, avg_time_template_oracle);
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
}

