#include "odbc.h"
#include "condor_api.h"

ODBC::ODBC()
{
	connected = false;
	strcpy(dsn,"");
	strcpy(user,"");
	strcpy(auth,"");
}
ODBC::ODBC(char *dsn, char *user, char *auth)
{
	connected = false;
	strncpy(this->dsn,dsn,80);
	strncpy(this->user,user,30);
	strncpy(this->auth,auth,80);
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
		return odbc_retcode;
        }
	dprintf(D_FULLDEBUG,"Connected %s:%s:%s:\n",dsn,user,auth);
	connected = true;
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
long ODBC::odbc_sqlstmt(const char* statement)
{
	int odbc_retcode;
	dprintf(D_FULLDEBUG,"%s Insert\n",statement);
	odbc_retcode=SQLExecDirect( odbc_hstmt,(SQLCHAR*)statement,SQL_NTS);
	if ((odbc_retcode != SQL_SUCCESS) && (odbc_retcode != SQL_SUCCESS_WITH_INFO))
	{
		dprintf(D_FULLDEBUG,"Error in Statement %d\n",odbc_retcode);
		SQLGetDiagRec(SQL_HANDLE_DBC, odbc_hdbc,1, (SQLCHAR*)odbc_stat,&odbc_err,(SQLCHAR*)odbc_msg,100,&odbc_mlen);
		dprintf(D_FULLDEBUG,"%s (%d)\n",odbc_msg,odbc_err);
	}
	return odbc_retcode;
}
long ODBC::odbc_bindcol(unsigned short col_number, void *result, int buffer_len, SQLSMALLINT col_type)
{
	return 	(SQLBindCol(odbc_hstmt, col_number, col_type, result,(SQLINTEGER)buffer_len, &odbc_err));
}
long ODBC::odbc_fetch()
{
	return (SQLFetch(odbc_hstmt));

}


