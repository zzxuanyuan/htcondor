#include <string.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

#ifndef ODBC_H
#define ODBC_H

class ODBC
{
private:

	SQLHENV	odbc_env;		      // Handle ODBC environment
	SQLHDBC	odbc_hdbc;		     // Handle connection
	SQLHSTMT	odbc_hstmt;
	bool 	connected;
	char	odbc_stat[10];
	char	odbc_msg[200];
	SQLINTEGER	odbc_err;
	SQLSMALLINT	odbc_mlen;
	char dsn[81];
	char user[31];
	char auth[81];
public:
	
	ODBC();
	ODBC(char *dsn, char *user, char *auth);
	~ODBC();
	long odbc_connect();
	long odbc_connect(char *dsn, char *user, char *auth);
	long odbc_disconnect();
	long odbc_sqlstmt(const char* statement);	
	bool isConnected() {return connected;}
	long odbc_bindcol(unsigned short col_number, void *result, int buffer_len, SQLSMALLINT col_type = SQL_DEFAULT);
	long odbc_fetch();

};

#endif
