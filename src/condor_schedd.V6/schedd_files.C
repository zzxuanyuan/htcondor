
#include "condor_common.h"
#include "schedd_files.h"
#include "condor_attributes.h"
#include "condor_md.h"
#include <sys/mman.h>
#include "basename.h"
#include "pgsqldatabase.h"
#include "my_hostname.h"
#include "odbc.h"

#define MAXSQLLEN 500
#define TIMELEN 30

//extern PGSQLDatabase *DBObj;
extern ODBC *DBObj;

bool schedd_file_checksum(
						  char *filePathName, 
						  int fileSize, 
						  char *sum)
{
	int fd;
	char *data;
	Condor_MD_MAC *checker = new Condor_MD_MAC();
	unsigned char *checksum;

	fd = open(filePathName, O_RDONLY, 0);
	if (fd < 0) {
		dprintf(D_FULLDEBUG, "schedd_file_checksum: can't open %s\n", filePathName);
		return FALSE;
	}

	data = (char *)mmap(0, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
	if (data == ((char *) -1))
		dprintf(D_FULLDEBUG, "schedd_file_checksum: mmap failed\n");

	close(fd);

	checker->addMD((unsigned char *) data, fileSize);
	if (munmap(data, fileSize) < 0)
		dprintf(D_FULLDEBUG, "schedd_file_checksum: munmap failed\n");

	checksum = checker->computeMD();

	if (checksum){
        memcpy(sum, checksum, MAC_SIZE);
        free(checksum);
	}
	else {
		dprintf(D_FULLDEBUG, "schedd_file_checksum: computeMD failed\n");
		return FALSE;
	}

	return TRUE;
}

bool schedd_files_check_file(
							 char *fileName, 
							 char *fs_domain,
							 char *path,
							 char *ascTime, 
							 int *fileid)
{
	char sqltext[MAXSQLLEN];
	int retcode;

	sprintf(sqltext, 
			"select f_id from files where f_name='%s' and f_path='%s' and f_host='%s' and f_ts='%s'",
			fileName, path, fs_domain, ascTime);

	retcode = DBObj->odbc_sqlstmt(sqltext);

	if ((retcode != SQL_SUCCESS) && (retcode != SQL_SUCCESS_WITH_INFO)) {
		return FALSE;
	}

	DBObj->odbc_bindcol(1, (void *)fileid, sizeof(int), SQL_C_LONG);
	retcode = DBObj->odbc_fetch();
	
	dprintf(D_FULLDEBUG, "schedd_file_check_file: sqltext is %s\n", sqltext);
	
	DBObj->odbc_closestmt();
	
	if (retcode == SQL_NO_DATA) {
		return FALSE;
	}
	else {		
		dprintf(D_FULLDEBUG, "schedd_file_check_file: fileid found %d\n", *fileid);
		return TRUE;
	}
}

int schedd_files_new_id() {
	char sqltext[MAXSQLLEN];
	int retcode;
	int fileid;

	sprintf(sqltext, 
			"select nextval('seqfileid')");
	
	retcode = DBObj->odbc_sqlstmt(sqltext);

	if((retcode != SQL_SUCCESS) && (retcode != SQL_SUCCESS_WITH_INFO)) {
		return -1;
	}

	DBObj->odbc_bindcol(1, (void *)&fileid, sizeof(int), SQL_C_LONG);
	retcode = DBObj->odbc_fetch();	
	
	DBObj->odbc_closestmt();
	
	if((retcode != SQL_SUCCESS) && (retcode != SQL_SUCCESS_WITH_INFO)) {
		return -1;
	}

	return fileid;
}

int schedd_files_ins_file(
						  int fileid,
						  char *fileName,
						  char *fs_domain,
						  char *path,
						  char *ascTime,
						  int fsize)
{
	char hexSum[MAC_SIZE*2+1];	
	char sum[MAC_SIZE];
	char pathname[_POSIX_PATH_MAX];
	char sqltext[MAXSQLLEN];
	int retcode;

	sprintf(pathname, "%s/%s", path, fileName);

	if ((fsize > 0) && schedd_file_checksum(pathname, fsize, sum)) {
		for (int i = 0; i < MAC_SIZE; i++)
			sprintf(&hexSum[2*i], "%2x", sum[i]);		
		hexSum[2*MAC_SIZE] = '\0';
	}
	else
		hexSum[0] = '\0';

	sprintf(sqltext, 
			"insert into files values(%d, '%s', '%s', '%s', '%s', %d, '%s')", 
			fileid, fileName, fs_domain, path, ascTime, fsize, hexSum);

	dprintf (D_FULLDEBUG, "In schedd_files_ins_file, sqltext is: %s\n", sqltext);

	retcode = DBObj->odbc_sqlstmt(sqltext);
	DBObj->odbc_closestmt();

	return retcode;
}

void schedd_files_ins_usage(
							char *globalJobId,
							int fileid,
							const char *type)
{
	char sqltext[MAXSQLLEN];
	sprintf(sqltext, 
			"insert into fileusages values('%s', %d, '%s')",
			globalJobId, fileid, type);
	dprintf (D_FULLDEBUG, "In schedd_files_ins_usage, sqltext is: %s\n", sqltext);
	DBObj->odbc_sqlstmt(sqltext);
	DBObj->odbc_closestmt();
}

void schedd_files_ins(
					  ClassAd *procad, 
					  const char *type)
{
	char *fileName = NULL,
		*tmpFile = NULL,
		*fs_domain = NULL,    /* submitter's file system domain */
		*path = NULL,
		*globalJobId = NULL,
		*tmp;

	char *pathname = (char *)malloc(_POSIX_PATH_MAX);

	struct stat file_status;
	bool freeFsDomain = FALSE;
	bool fileExist = TRUE;
	char ascTime[TIMELEN];
	int len;
	int fileid;
	int retcode;

		// get the file name (possibly with path) from classad
	procad->LookupString(type, &tmpFile);

		// get host machine name from classad
	if (!procad->LookupString(ATTR_FILE_SYSTEM_DOMAIN, &fs_domain)) {
			// use my hostname for files if fs_domain not found
		fs_domain = my_full_hostname();
	} else
		freeFsDomain = TRUE;
	
		// get current working directory
	procad->LookupString(ATTR_JOB_IWD, &path);

	procad->LookupString(ATTR_GLOBAL_JOB_ID, &globalJobId);

		// determine file name, path.
	if (fullpath(tmpFile)) {
		if (strcmp(tmpFile, "/dev/null") == 0)
			return; /* job doesn't care about this type of file */
		
		strcpy(pathname, tmpFile);
		
		free(path);	// free prev path first
		path = dirname(tmpFile);
		fileName = basename(tmpFile); 
	}
	else {
		sprintf(pathname, "%s/%s", path, tmpFile);
		fileName = tmpFile;
	}

		// get the file status which contains timestamp and size
	if (stat(pathname, &file_status) < 0) {
		dprintf(D_FULLDEBUG, "ERROR: File '%s' can not be accessed.\n", 
				pathname);
		fileExist = FALSE;
	}

	if(!fileExist) {
		goto schedd_files_ins_end;
	}

		// build ascii time to be stored in database
	tmp = ctime(&file_status.st_mtime);
	len = strlen(tmp);
	strncpy(ascTime, (const char *)tmp, len-1); /* ignore the last newline character */
	ascTime[len-1] = '\0';

	if (!schedd_files_check_file(fileName, fs_domain, path, ascTime, &fileid)) {
			// this file is not in the files table yet
			// generate a new file id
		fileid = schedd_files_new_id();

		if (fileid < 0) {
				// smth wrong with db that we get a negative sequence number
			goto schedd_files_ins_end;
		}
	
			// insert the file entry into the files table
		retcode = schedd_files_ins_file(fileid, fileName, fs_domain, path, ascTime, 
										file_status.st_size);
		
		if ((retcode != SQL_SUCCESS)  &&  (retcode != SQL_SUCCESS_WITH_INFO)) {
				// the file may have just been inserted by someone else 
			if(!schedd_files_check_file(fileName, fs_domain, path, ascTime, &fileid)) {
					// we still can't find file after failing to insert it
					// smth is wrong with db
				goto schedd_files_ins_end;
			}
		}
	}

		// insert a usage for this file by this job
	schedd_files_ins_usage(globalJobId, fileid, type);

schedd_files_ins_end:
	if (path) free(path);
	if (pathname) free(pathname);
	if (globalJobId) free(globalJobId);
	if (tmpFile) free(tmpFile);
	if (freeFsDomain) free(fs_domain);
}

void schedd_files(ClassAd *procad)

{
	schedd_files_ins(procad, ATTR_JOB_CMD);	
	schedd_files_ins(procad, ATTR_JOB_INPUT);
	schedd_files_ins(procad, ATTR_JOB_OUTPUT);
	schedd_files_ins(procad, ATTR_JOB_ERROR);
	schedd_files_ins(procad, ATTR_ULOG_FILE);
}
