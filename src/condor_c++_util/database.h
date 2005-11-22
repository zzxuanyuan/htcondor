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

//! Database
/*! It provides interfaces to talk to DBMS
 */
class Database
{
public:
	//! destructor
	virtual ~Database() {};

	//! connect to DBMS
	virtual int		connectDB() = 0;
	//! connect to DBMS
	virtual int		connectDB(const char* connect) = 0;
	//! disconnect from DBMS
	virtual int		disconnectDB() = 0;

	//! begin Transaction
	virtual int		beginTransaction() = 0;
	//! commit Transaction
	virtual int		commitTransaction() = 0;
	//! abort Transaction
	virtual int		rollbackTransaction() = 0;

	//! execute a command
	/*! execaute SQL which doesn't have any retrieved result, such as
	 *  insert, delete, and udpate.
	 */
	virtual int		execCommand(const char* sql) = 0;
	//! execute a SQL query
	virtual int		execQuery(const char* sql) = 0;

	//! get a result for the executed SQL
	virtual const char*	getValue(int row, int col) = 0;

	//! release a query result
	virtual int             releaseQueryResult() = 0;

	//! get a DBMS error message
	virtual char*	getDBError() = 0;

	//! put bulk data into DBMS
	virtual int		sendBulkData(char* data) = 0;
	//! put an end flag for bulk loading
	virtual int		sendBulkDataEnd() = 0;

	bool	isConnected() {return connected;};

protected:
	char	*con_str;	//!< connection string
	bool	connected; 	//!< connection status
};

#endif
