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

#include "condor_ttdb.h"
#include <time.h>
#include "misc_utils.h"
#include <stdlib.h>
#include <stdio.h>

char *condor_ttdb_buildts(time_t *tv, dbtype dt)
{
	char tsv[100];
	struct tm *tm;	
	char *rv;

	tm = localtime(tv);		

	snprintf(tsv, 100, "%d/%d/%d %02d:%02d:%02d %s", 
			 tm->tm_mon+1,
			 tm->tm_mday,
			 tm->tm_year+1900,
			 tm->tm_hour,
			 tm->tm_min,
			 tm->tm_sec,
			 my_timezone(tm->tm_isdst));	

	switch(dt) {
	case T_ORACLE:
		rv = (char *)malloc(200);
		snprintf(rv, 200, "TO_TIMESTAMP_TZ('%s', 'MM/DD/YYYY HH24:MI:SS TZD')",
				 tsv);
		break;
	case T_PGSQL:
		rv = (char *)malloc(100);
		snprintf(rv, 100, "'%s'", 
				 tsv);		
		break;
	default:
		rv = NULL;
		break;
	}

	return rv;
}

char *condor_ttdb_buildseq(dbtype dt, char *seqName)
{
	char *rv;

	switch(dt) {
	case T_ORACLE:
		rv = (char *)malloc(50);
		snprintf(rv, 50, "%s.nextval", seqName);
		break;
	case T_PGSQL:
		rv = (char *)malloc(50);
		snprintf(rv, 50, "nextval('%s')", seqName);		
		break;
	default:
		rv = NULL;
		break;
	}

	return rv;
}

char *condor_ttdb_onerow_clause(dbtype dt)
{
	char *rv;

	switch(dt) {
	case T_ORACLE:
		rv = (char *)malloc(50);
		snprintf(rv, 50, " and ROWNUM <= 1");
		break;
	case T_PGSQL:
		rv = (char *)malloc(50);
		snprintf(rv, 50, " LIMIT 1");
		break;
	default:
		rv = NULL;
		break;
	}

	return rv;	
}
