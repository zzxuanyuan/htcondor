
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

//extern PGSQLDatabase *DBObj;
extern ODBC *DBObj;

void schedd_file_checksum(char *filePathName, int fileSize, char *sum)
{
	int fd;
	char *data;
	Condor_MD_MAC *checker = new Condor_MD_MAC();
	unsigned char *checksum;

	fd = open(filePathName, O_RDONLY, 0);
	if (fd < 0) {
		dprintf(D_ALWAYS, "schedd_file_checksum: can't open %s\n", filePathName);
		return;
	}

	data = (char *)mmap(0, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
	if (data == ((char *) -1))
		dprintf(D_ALWAYS, "schedd_file_checksum: mmap failed\n");

	close(fd);

	checker->addMD((unsigned char *) data, fileSize);
	if (munmap(data, fileSize) < 0)
		dprintf(D_ALWAYS, "schedd_file_checksum: munmap failed\n");

	checksum = checker->computeMD();

	if (checksum){
        memcpy(sum, checksum, MAC_SIZE);
        free(checksum);
	}
	else
		dprintf(D_ALWAYS, "schedd_file_checksum: computeMD failed\n");

}

void schedd_files_Ins(ClassAd *procad, const char *type)
{
	char *fileName = NULL,
		*tmpFile = NULL,
		*fs_domain = NULL,    /* submitter's file system domain */
		*path = NULL,
		*globalJobId = NULL;

	char sqltext[MAXSQLLEN];
	char *pathname = (char *)malloc(_POSIX_PATH_MAX);

	struct stat file_status;
	char sum[MAC_SIZE];
	char hexSum[MAC_SIZE*2+1];
	bool freeFsDomain = FALSE;

	procad->LookupString(type, &tmpFile);

	if (!procad->LookupString(ATTR_FILE_SYSTEM_DOMAIN, &fs_domain)) {
			// my hostname for files if fs_domain not found
		fs_domain = my_full_hostname();
	} else
		freeFsDomain = TRUE;
	
	
	procad->LookupString(ATTR_JOB_IWD, &path);
	procad->LookupString(ATTR_GLOBAL_JOB_ID, &globalJobId);

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

	if (stat(pathname, &file_status) < 0) {
		dprintf(D_ALWAYS, "ERROR: File '%s' can not be accessed.\n", 
				pathname);
	}

	if (file_status.st_size > 0) {
		schedd_file_checksum(pathname, file_status.st_size, sum);
		for (int i = 0; i < MAC_SIZE; i++)
			sprintf(&hexSum[2*i], "%2x", sum[i]);

		hexSum[2*MAC_SIZE] = '\0';
	}
	else 
		hexSum[0] = '\0';

		// build sql to insert a file entry
		// file name
		// hostname
		// path
		// timestamp
		// checksum
		// size
		// usageType
		// globalJobId

	sprintf(sqltext, 
			"insert into files values('%s', '%s', '%s', '%s', %d, '%s', '%s', '%s')", 
			fileName, fs_domain, path, ctime(&file_status.st_mtime),
			(int)file_status.st_size, globalJobId, type, hexSum);

	free(path);
	free(pathname);
	free(globalJobId);
	free(tmpFile);

	if (freeFsDomain)
		free(fs_domain);

	dprintf (D_ALWAYS, "In schedd_files_DbIns. sqltext is: %s\n", sqltext);


		//DBObj->execCommand(sqltext);
	DBObj->odbc_sqlstmt(sqltext);
}

void schedd_files_DbIns(ClassAd *procad, bool preExec)
{
		//DBObj->beginTransaction();

	if (preExec) {
		schedd_files_Ins(procad, ATTR_JOB_CMD);
		schedd_files_Ins(procad, ATTR_JOB_INPUT);

		// to avoid duplicate records for files, use StringList
		// to manage the list and use file_contains to check if 
		// the list already contains a file.

		// user log file needs to be inserted again after job finished
		schedd_files_Ins(procad, ATTR_ULOG_FILE);
	} else {
			// post execution
		schedd_files_Ins(procad, ATTR_JOB_OUTPUT);
		schedd_files_Ins(procad, ATTR_JOB_ERROR);
		schedd_files_Ins(procad, ATTR_ULOG_FILE);
	}
		//DBObj->commitTransaction();
}
