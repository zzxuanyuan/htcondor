/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 

#define _POSIX_SOURCE

#include "condor_common.h"

#include "log.h"

LogRecord::LogRecord()
{
}


LogRecord::~LogRecord()
{
}

bool LogRecord::readword(FILE *fp, char * &str)
{
	int		i, bufsize = 1024;
	char	*buf = (char *)malloc(bufsize);

	// ignore leading whitespace
	do {
		buf[0] = fgetc( fp );
		if( buf[0] == EOF && !feof( fp ) ) {
			free( buf );
			return( false );
		}
	} while (isspace(buf[0]) && buf[0] != EOF );

	// read until whitespace
	for (i = 1; !isspace(buf[i-1]) && buf[i-1]!='\0' && buf[i-1]!=EOF; i++) {
		if (i == bufsize) {
			buf = (char *)realloc(buf, bufsize*2);
			bufsize *= 2;
		} 
		buf[i] = fgetc( fp );
		if( buf[i] == EOF && !feof( fp ) ) {
			free( buf );
			return( false );
		}
	}
	buf[i-1] = '\0';
	str = strdup(buf);
	free(buf);
	return true;
}

bool LogRecord::readline(FILE *fp, char * &str)
{
	int		i, bufsize = 1024;
	char	*buf = (char *)malloc(bufsize);

	// ignore leading whitespace
	do {
		buf[0] = fgetc( fp );
		if( buf[0] == EOF && !feof( fp ) ) {
			free( buf );
			return( false );
		}
	} while (isspace(buf[0]) && buf[0] != EOF );

	// read until newline
	for (i = 1; buf[i-1]!='\n' && buf[i-1] != '\0' && buf[i-1] != EOF; i++) {
		if (i == bufsize) {
			buf = (char *)realloc(buf, bufsize*2);
			bufsize *= 2;
		} 
		buf[i] = fgetc( fp );
		if( buf[i] == EOF && !feof( fp ) ) {
			free( buf );
			return( false );
		}
	}
	buf[i-1] = '\0';
	str = strdup(buf);
	free(buf);
	return true;
}

bool LogRecord::Write(FILE *fp)
{
	return (WriteHeader(fp) && WriteBody(fp) && WriteTail(fp));
}

bool LogRecord::Read(FILE *fp)
{
	return (ReadHeader(fp) && ReadBody(fp) && ReadTail(fp));
}

bool LogRecord::WriteHeader(FILE *fp)
{
	return (fprintf(fp, "%d ", op_type)>=0);
}

bool LogRecord::WriteTail(FILE *fp)
{
	return (fprintf(fp, "\n")>=0);
}

bool LogRecord::ReadHeader(FILE *fp)
{
	return (fscanf(fp, "%d ", &op_type)>=0);
}

bool LogRecord::ReadTail(FILE *fp)
{
	return true;
}
