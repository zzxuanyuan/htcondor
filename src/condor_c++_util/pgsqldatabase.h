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

#ifndef _PGSQLDATABASE_H_
#define _PGSQLDATABASE_H_

#include "condor_common.h"
#include "libpq-fe.h"
#include "sqlquery.h"
#include "database.h"
#include "quill_enums.h"

#ifndef MAX_FIXED_SQL_STR_LENGTH
#define MAX_FIXED_SQL_STR_LENGTH 2048
#endif

//! PGSQLDataabse: Database for PostgreSQL
//
class PGSQLDatabase : public Database
{
public:
	
	PGSQLDatabase(const char* connect);
	~PGSQLDatabase();

		// connection method
	QuillErrCode         connectDB();
	QuillErrCode		 disconnectDB();
    QuillErrCode         checkConnection();
	QuillErrCode         resetConnection();

		// General DB processing methods
	QuillErrCode		 beginTransaction();
	QuillErrCode 		 commitTransaction();
	QuillErrCode 		 rollbackTransaction();

		// update methods
	QuillErrCode 	 	 execCommand(const char* sql, 
									 int &num_result);
	QuillErrCode 	 	 execCommand(const char* sql);

		// query methods
	QuillErrCode 	 	 execQuery(const char* sql);
	QuillErrCode   		 fetchNext();
	const char*	         getValue(int col);
	int  				 getIntValue(int col);

	QuillErrCode         releaseQueryResult();
private:
	PGconn		         *connection;	//!< connection object
	PGresult	         *queryRes; 	//!< result for general query
	char				 *con_str;		//!< connection string
	int 				  num_result;
	int                   row_idx;
};

#endif /* _PGSQLDATABSE_H_ */

