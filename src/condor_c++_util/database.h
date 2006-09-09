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
#ifndef _DATABASE_H_
#define _DATABASE_H_

#include "quill_enums.h"

typedef enum {
	T_INT, 
	T_STRING,
	T_DOUBLE, 
	T_TIMESTAMP
} dataType;

//! Database
/*! It provides interfaces to talk to DBMS
 */
class Database
{
public:
	//! destructor
	virtual ~Database() {};

	//! connect to DBMS
	virtual QuillErrCode		connectDB() = 0;

	//! disconnect from DBMS
	virtual QuillErrCode		disconnectDB() = 0;

	//! begin Transaction
	virtual QuillErrCode		beginTransaction() = 0;
	//! commit Transaction
	virtual QuillErrCode		commitTransaction() = 0;
	//! abort Transaction
	virtual QuillErrCode		rollbackTransaction() = 0;

	//! execute a command
	/*! execute SQL which doesn't have any retrieved result, such as
	 *  insert, delete, and udpate.
	 */
	virtual QuillErrCode		execCommand(const char* sql, 
											int &num_result) = 0;
	virtual QuillErrCode		execCommand(const char* sql) = 0;

	//! execute a SQL query
	virtual QuillErrCode		execQuery(const char* sql) = 0;

	//! get a result for the executed SQL
	virtual QuillErrCode 		fetchNext() = 0;
	virtual const char*         getValue(int col) = 0;
	virtual int			        getIntValue(int col) = 0;

	//! release query result
	virtual QuillErrCode        releaseQueryResult() = 0;

	virtual QuillErrCode        checkConnection() = 0;
	virtual QuillErrCode        resetConnection() = 0;

protected:
	bool	connected; 	//!< connection status
};

#endif
