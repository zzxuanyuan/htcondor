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
#include "sqlquery.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define avg_time_template "SELECT avg((now() - 'epoch'::timestamp with time zone) - cast(QDate || ' seconds' as interval)) \
         FROM \
           (SELECT \
             (CASE WHEN p.QDate ISNULL THEN c.QDate ELSE p.QDate END) AS QDate, \
             (CASE WHEN p.JobStatus ISNULL THEN c.JobStatus ELSE p.JobStatus END) AS JobStatus \
            FROM (select cid, pid, \
          	        max(CASE attr WHEN 'QDate'  THEN val ELSE NULL END) AS QDate, \
              	    max(CASE attr WHEN 'JobStatus'  THEN val ELSE NULL END) AS JobStatus \
	              FROM procads GROUP BY cid, pid having cid != 0) AS p, \
                 (select cid, \
 	                max(CASE attr WHEN 'QDate'  THEN val ELSE NULL END) AS QDate, \
	                max(CASE attr WHEN 'JobStatus'  THEN val ELSE NULL END) AS JobStatus \
	              FROM clusterads GROUP BY cid having cid != 0) AS c \
            WHERE p.cid = c.cid) AS h \
         WHERE (jobStatus != '3'::text) and (jobstatus != '4'::text);"


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

void SQLQuery::
createQueryString(query_types qtype, void **parameters) {  
  query_str = (char *) malloc(MAX_QUERY_SIZE * sizeof(char));
  switch(qtype) {
    
  case HISTORY_ALL_HOR:
    sprintf(query_str, 
	    "SELECT * FROM HISTORY_HORIZONTAL ORDER BY CID, PID;");
    break;
  case HISTORY_ALL_VER:
    sprintf(query_str, 
	    "SELECT * FROM HISTORY_VERTICAL ORDER BY CID, PID;");
    break;
  case HISTORY_CLUSTER_HOR:
    sprintf(query_str, 
	    "SELECT * FROM HISTORY_HORIZONTAL WHERE cid=%d ORDER BY CID, PID;",
	    *((int *)parameters[0]));
    break;
  case HISTORY_CLUSTER_VER:
    sprintf(query_str, 
	    "SELECT * FROM HISTORY_VERTICAL WHERE cid=%d ORDER BY CID, PID;",
	    *((int *)parameters[0]));
    break;
  case HISTORY_CLUSTER_PROC_HOR:
    sprintf(query_str, 
	    "SELECT * FROM HISTORY_HORIZONTAL WHERE cid=%d and pid=%d ORDER BY CID, PID;",
	    *((int *)parameters[0]), *((int *)parameters[1]));
    break;  
  case HISTORY_CLUSTER_PROC_VER:
    sprintf(query_str, 
	    "SELECT * FROM HISTORY_VERTICAL WHERE cid=%d and pid=%d ORDER BY CID, PID;",
	    *((int *)parameters[0]), *((int *)parameters[1]));
    break;  
  case HISTORY_OWNER_HOR:
    sprintf(query_str,
	    "SELECT * FROM HISTORY_HORIZONTAL WHERE \"Owner\"='\"%s\"' ORDER BY CID,PID;",
	    ((char *)parameters[0]));
    break;
  case HISTORY_OWNER_VER:
    sprintf(query_str,
	    "SELECT hv.cid,hv.pid,hv.attr,hv.val FROM HISTORY_HORIZONTAL hh, HISTORY_VERTICAL hv WHERE hh.cid=hv.cid AND hh.pid=hv.pid AND hh.\"Owner\"='\"%s\"' ORDER BY CID,PID;",
	    ((char *)parameters[0]));
    break;
  case HISTORY_COMPLETEDSINCE_HOR:
    sprintf(query_str,
	    "SELECT * FROM History_Horizontal WHERE 'epoch'::timestamp with time zone + cast(text(\"CompletionDate\")|| text(' seconds') as interval) > '%s ' ORDER BY CID,PID;",
	    ((char *)parameters[0]));
    break;
  case HISTORY_COMPLETEDSINCE_VER:
    sprintf(query_str,
	    "SELECT hv.cid,hv.pid,hv.attr,hv.val FROM HISTORY_HORIZONTAL hh, HISTORY_VERTICAL hv WHERE hh.cid=hv.cid AND hh.pid=hv.pid AND 'epoch'::timestamp with time zone + cast(text(hh.\"CompletionDate\")|| text(' seconds') as interval) > '%s ' ORDER BY CID,PID;",
	    ((char *)parameters[0]));
    break;
  case QUEUE_AVG_TIME:
    sprintf(query_str, avg_time_template);
    break;

  default:
    printf("error...incorrect query type specified...exiting\n");
    assert(1);
  }
}

void SQLQuery::
Print() 
{
  printf("Query = %s\n", query_str);
}

