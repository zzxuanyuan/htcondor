#include "odbc.h"
#include "condor_api.h"
#include "condor_config.h"

// define the pointer to database connection object here since the odbc.o is put into 
// the cplus_lib.a library. And that is because modules such as file_transfer.o and
// classad_log.o uses DBObj and they are part of cplus_lib.a. This way we won't get
// the DBObj undefined error during compilation of any code which needs cplus_lib.a.

// notice the DBObj is just a pointer, the real object should be created only when 
// a real database connection is needed. E.g. most daemons need database connection, 
// there we can create a database connection in the  main function of daemon process.
ODBC *DBObj = 0;

ODBC::ODBC()
{
	connected = false;
	strcpy(dsn,"");
	strcpy(user,"");
	strcpy(auth,"");
	timeout = 0;
	in_xact = false;
}
ODBC::ODBC(char *dsn, char *user, char *auth, char *timeout)
{
	connected = false;
	strncpy(this->dsn,dsn,80);
	strncpy(this->user,user,30);
	strncpy(this->auth,auth,80);
	this->timeout = atoi(timeout);
	in_xact = false;
}

ODBC::~ODBC()
{
	if(isConnected())
		odbc_disconnect();

	strcpy(dsn,"");
	strcpy(user,"");
	strcpy(auth,"");
	
}

long ODBC::odbc_connect()
{
	int odbc_retcode;

	if(isConnected())
		odbc_disconnect();
	odbc_retcode=SQLAllocHandle(SQL_HANDLE_ENV,SQL_NULL_HANDLE,&odbc_env);
	if ((odbc_retcode != SQL_SUCCESS) && (odbc_retcode != SQL_SUCCESS_WITH_INFO))
	{
		dprintf(D_FULLDEBUG,"Error AllocHandle\n");
		connected = false;
		return odbc_retcode;
	}

	odbc_retcode=SQLSetEnvAttr(odbc_env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC2, 0);
	if ((odbc_retcode != SQL_SUCCESS) && (odbc_retcode != SQL_SUCCESS_WITH_INFO))
	{
		dprintf(D_FULLDEBUG,"Error SetEnv\n");
		SQLFreeHandle(SQL_HANDLE_ENV, odbc_env);
		connected = false;
		return odbc_retcode;
	}

	// 2. allocate connection handle, set timeout
	odbc_retcode = SQLAllocHandle(SQL_HANDLE_DBC, odbc_env, &odbc_hdbc);
	if ((odbc_retcode != SQL_SUCCESS) && (odbc_retcode != SQL_SUCCESS_WITH_INFO))
	{
		dprintf(D_FULLDEBUG,"Error AllocHandle %d\n",odbc_retcode);
		SQLFreeHandle(SQL_HANDLE_ENV, odbc_env);
		connected = false;
		return odbc_retcode;
	}

	SQLSetConnectAttr(odbc_hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER *)5, 0);

	// 3. Connect to the datasource "web" 
	odbc_retcode = SQLConnect(odbc_hdbc, (SQLCHAR*) dsn, SQL_NTS,
				     (SQLCHAR*) user, SQL_NTS,
				     (SQLCHAR*) auth, SQL_NTS);
	if ((odbc_retcode != SQL_SUCCESS) && (odbc_retcode != SQL_SUCCESS_WITH_INFO))
	{
		dprintf(D_FULLDEBUG,"Error SQLConnect %d\n",odbc_retcode);
		SQLGetDiagRec(SQL_HANDLE_DBC, odbc_hdbc,1,
			      (SQLCHAR*)odbc_stat, &odbc_err,(SQLCHAR*)odbc_msg,100,&odbc_mlen);
		dprintf(D_FULLDEBUG,"%s (%d)\n",odbc_msg,odbc_err);
		SQLFreeHandle(SQL_HANDLE_ENV, odbc_env);
		connected = false;
		return odbc_retcode;
	}

	odbc_retcode=SQLAllocHandle(SQL_HANDLE_STMT, odbc_hdbc, &odbc_hstmt);
	if ((odbc_retcode != SQL_SUCCESS) && (odbc_retcode != SQL_SUCCESS_WITH_INFO)) 
	{
		dprintf(D_FULLDEBUG,"Error in AllocStatement %d\n",odbc_retcode);
		SQLGetDiagRec(SQL_HANDLE_DBC, odbc_hdbc,1, (SQLCHAR*)odbc_stat,&odbc_err,(SQLCHAR*)odbc_msg,100,&odbc_mlen);
		dprintf(D_FULLDEBUG,"%s (%d)\n",odbc_msg,odbc_err);
		SQLDisconnect(odbc_hdbc);
		SQLFreeHandle(SQL_HANDLE_DBC,odbc_hdbc);
		SQLFreeHandle(SQL_HANDLE_ENV, odbc_env);
		connected = false;
		return odbc_retcode;
	}

	dprintf(D_FULLDEBUG,"Connected %s:%s:%s:\n",dsn,user,auth);
	connected = true;

	char timeout_stmt[50];
	sprintf(timeout_stmt, "set statement_timeout= %d", timeout);

	odbc_sqlstmt(timeout_stmt);

	return odbc_retcode;
	
	
}

long ODBC::odbc_connect(char *dsn, char *user, char *auth)
{
	strncpy(this->dsn,dsn,80);
	strncpy(this->user,user,30);
	strncpy(this->auth,auth,80);

	return (odbc_connect());
}

long ODBC::odbc_disconnect()
{
	SQLFreeHandle(SQL_HANDLE_STMT,odbc_hstmt);
	SQLDisconnect(odbc_hdbc);
	SQLFreeHandle(SQL_HANDLE_DBC,odbc_hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, odbc_env);
	connected = false;
	dprintf(D_FULLDEBUG,"Disconnected\n");
	return(0);
}
long ODBC::odbc_beginxtstmt(const char * statement)
{
	int odbc_retcode;
	dprintf(D_FULLDEBUG,"%s Begin\n",statement);
	if(!connected)
	{
		if(in_xact)
			return SQL_ERROR;
		else
		{
			odbc_retcode = odbc_connect();
			if((odbc_retcode != SQL_SUCCESS) && (odbc_retcode != SQL_SUCCESS_WITH_INFO)) 
			{
				in_xact = false;
				dprintf(D_FULLDEBUG,"Failed to connect -- skipping begin statement\n");
				return odbc_retcode;
			}	
		}
	}
	odbc_retcode=SQLExecDirect( odbc_hstmt,(SQLCHAR*)statement,SQL_NTS);
	if ((odbc_retcode != SQL_SUCCESS) && (odbc_retcode != SQL_SUCCESS_WITH_INFO))
	{
		dprintf(D_FULLDEBUG,"Error in Statement %d\n",odbc_retcode);
		SQLGetDiagRec(SQL_HANDLE_DBC, odbc_hdbc,1, (SQLCHAR*)odbc_stat,&odbc_err,(SQLCHAR*)odbc_msg,100,&odbc_mlen);
		dprintf(D_FULLDEBUG,"%s (%d)\n",odbc_msg,odbc_err);
		odbc_disconnect();
		in_xact = false;
	}
	else // XXX We go to in_xact only if the begin statement succeeds. Is this what we want ?
		in_xact = true; 
	return odbc_retcode;

}
long ODBC::odbc_endxtstmt(const char * statement)
{
	int odbc_retcode;
	dprintf(D_FULLDEBUG,"%s End\n",statement);
	if(!connected)
	{
		//  XXX We connect anyway and let the DB decide what to do
		odbc_retcode = odbc_connect();
		if((odbc_retcode != SQL_SUCCESS) && (odbc_retcode != SQL_SUCCESS_WITH_INFO)) 
		{
			in_xact = false;
			dprintf(D_FULLDEBUG,"Failed to connect -- skipping end statement\n");
			return odbc_retcode;
		}	
	}
	odbc_retcode=SQLExecDirect( odbc_hstmt,(SQLCHAR*)statement,SQL_NTS);
	if ((odbc_retcode != SQL_SUCCESS) && (odbc_retcode != SQL_SUCCESS_WITH_INFO))
	{
		dprintf(D_FULLDEBUG,"Error in Statement %d\n",odbc_retcode);
		SQLGetDiagRec(SQL_HANDLE_DBC, odbc_hdbc,1, (SQLCHAR*)odbc_stat,&odbc_err,(SQLCHAR*)odbc_msg,100,&odbc_mlen);
		dprintf(D_FULLDEBUG,"%s (%d)\n",odbc_msg,odbc_err);
		odbc_disconnect();
	}
	in_xact = false; 
	return odbc_retcode;

}
long ODBC::odbc_sqlstmt(const char* statement)
{
	int odbc_retcode;
	dprintf(D_FULLDEBUG,"%s sqlstmt \n",statement,connected);
	if(!connected)
	{
		if(in_xact)
			return SQL_ERROR;
		else
		{
			odbc_retcode = odbc_connect();
			if((odbc_retcode != SQL_SUCCESS) && (odbc_retcode != SQL_SUCCESS_WITH_INFO)) 
			{
				in_xact = false;
				dprintf(D_FULLDEBUG,"Failed to connect -- skipping sql statement\n");
				return odbc_retcode;
			}	
		}
	}
	odbc_retcode=SQLExecDirect( odbc_hstmt,(SQLCHAR*)statement,SQL_NTS);
	if ((odbc_retcode != SQL_SUCCESS) && (odbc_retcode != SQL_SUCCESS_WITH_INFO))
	{
		dprintf(D_FULLDEBUG,"Error in Statement %d\n",odbc_retcode);
		SQLGetDiagRec(SQL_HANDLE_DBC, odbc_hdbc,1, (SQLCHAR*)odbc_stat,&odbc_err,(SQLCHAR*)odbc_msg,100,&odbc_mlen);
		dprintf(D_FULLDEBUG,"odbc_msg:%s (odbc_err=%d)\n",odbc_msg,odbc_err);
		odbc_disconnect();
	}
	return odbc_retcode;
}

long ODBC::odbc_bindcol(unsigned short col_number, void *result, int buffer_len, SQLSMALLINT col_type)
{
	if (!connected) {
		dprintf(D_FULLDEBUG,"odbc_bindcol: not connected yet\n");
		return SQL_ERROR;
	}
	
	return 	(SQLBindCol(odbc_hstmt, col_number, col_type, result,(SQLINTEGER)buffer_len, &odbc_err));
}

long ODBC::odbc_fetch()
{
	if (!connected) {
		dprintf(D_FULLDEBUG,"odbc_fetch: not connected yet\n");
		return SQL_ERROR;
	}
	
	return (SQLFetch(odbc_hstmt));

}

ODBC *createConnection() {
	ODBC *ptr;
	int odbc_retcode;

	char *tmp, *odbc_dsn, *odbc_user, *odbc_auth, *odbc_timeout;
	
	/* Parse ODBC Connection Params */
	tmp = param("ODBC_DSN");
	if( tmp ) {
		odbc_dsn = strdup(tmp);
		free(tmp);
	}
	else {
		odbc_dsn = strdup("condor");
	}

	tmp = param("ODBC_USER");
	if( tmp ) {
		odbc_user = strdup(tmp);
		free(tmp);
	}
	else {
		odbc_user = strdup("scidb");
	}

	tmp = param("ODBC_AUTH");
	if( tmp ) {
		odbc_auth = strdup(tmp);
		free(tmp);
	}
	else {
		odbc_auth = strdup("");
	}

	tmp = param("ODBC_TIMEOUT");
	if( tmp ) {
		odbc_timeout = strdup(tmp);
		free(tmp);
	}
	else {
		odbc_timeout = strdup("300");
	}

	ptr = new ODBC(odbc_dsn, odbc_user, odbc_auth, odbc_timeout);
        free(odbc_dsn);
	free(odbc_user);
	free(odbc_auth);
        free(odbc_timeout);

	odbc_retcode = ptr->odbc_connect();
	if ((odbc_retcode != SQL_SUCCESS) && (odbc_retcode != SQL_SUCCESS_WITH_INFO)) {
		dprintf(D_FULLDEBUG, "createConnection: connection failed\n");
	}

	return ptr;
}
