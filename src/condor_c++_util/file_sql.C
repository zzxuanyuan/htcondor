#include "condor_api.h"
#include "condor_config.h"
#include <errno.h>
#include <string.h>
#include "file_sql.h"
#include "get_mysubsystem.h"
#include <sys/stat.h>

#define FILESIZELIMT 1900000000L

FILESQL::FILESQL()
{
    is_dummy = false;
	is_open = false;
	is_locked = false;
	outfilename = (char *) 0;
	fileflags = O_WRONLY|O_CREAT|O_APPEND;
	outfiledes = -1;
	fp = NULL;
	lock = (FileLock *)0;
}

FILESQL::FILESQL(const char *outfilename, int flags)
{
    is_dummy = false;
	is_open = false;
	is_locked = false;
	this->outfilename = strdup(outfilename);
	fileflags = flags;
	outfiledes = -1;
	fp = NULL;
	lock = (FileLock *)0;
}

FILESQL::~FILESQL()
{
	if(file_isopen()) {
		file_close();
	}
	is_open = false;
	is_locked = false;
	if (outfilename) free(outfilename);
	outfiledes = -1;
	fp = NULL;
}

bool FILESQL::file_isopen()
{
	return is_open;
}

bool FILESQL::file_islocked()
{
	return is_locked;
}

QuillErrCode FILESQL::file_truncate() {
	int retval;

    if (is_dummy) return SUCCESS;

	if(!file_isopen()) {
		dprintf(D_ALWAYS, "Error calling truncate: the file needs to be first opened\n");
		return FAILURE;
	}

	retval = ftruncate(outfiledes, 0);

	if(retval < 0) {
		dprintf(D_ALWAYS, "Error calling ftruncate, errno = %d\n", errno);
		return FAILURE;
	}

	return SUCCESS;
}

QuillErrCode FILESQL::file_open()
{
    if (is_dummy) return SUCCESS;
    
	if (!outfilename) {
		dprintf(D_ALWAYS,"No SQL log file specified");
		return FAILURE;
	}

	outfiledes = open(outfilename,fileflags,0644);

	if(outfiledes < 0)
	{
		dprintf(D_ALWAYS,"Error opening SQL log file %s : %s",outfilename,strerror(errno));
		is_open = false;
		return FAILURE;
	}
	else
	{
		is_open = true;
		lock = new FileLock(outfiledes,NULL); /* Create a lock object when opening the file */
		return SUCCESS;
	}
}

QuillErrCode FILESQL::file_close()
{
	int retval;
	
    if (is_dummy) return SUCCESS;
	
    if (!is_open)
		return FAILURE;

	if (lock) {
			/* This also releases the lock on the file, were it held */
		delete lock; 
		lock  = (FileLock *)0;
	}

		/* fp is associated with the outfiledes, we should be closing one or 
		 * the other, but not both. Otherwise the second one will give an 
		 * error.
		 */
	if (fp) {
		fclose(fp);
		fp = NULL;
	}
	else {
		retval = close(outfiledes);
		if(retval < 0)
			dprintf(D_ALWAYS,"Error closing SQL log file %s : %s",outfilename,strerror(errno));
	}

	is_open = false;
	is_locked = false;
	outfiledes = -1;

	if (retval < 0) {
		return FAILURE;
	}
	else {
		return SUCCESS;
	}
}

QuillErrCode FILESQL::file_lock()
{
    if (is_dummy) return SUCCESS;
	
	if(!is_open)
	{
		dprintf(D_ALWAYS,"Error locking :SQL log file %s not open yet",outfilename);
		return FAILURE;
	}

	if(is_locked)
		return SUCCESS;

	if(lock->obtain(WRITE_LOCK) == 0) /* 0 means an unsuccessful lock */
	{
		dprintf(D_ALWAYS,"Error locking SQL log file %s ",outfilename);
		return FAILURE;
	}
	else
		is_locked = true;

	return SUCCESS;
}

QuillErrCode FILESQL::file_unlock()
{
    if (is_dummy) return SUCCESS;
	
	if(!is_open)
	{
		dprintf(D_ALWAYS,"Error unlocking :SQL log file %s not open yet",outfilename);
		return FAILURE;
	}

	if(!is_locked)
		return SUCCESS;

	if(lock->release() == 0) /* 0 means an unsuccessful lock */
	{
		dprintf(D_ALWAYS,"Error unlocking SQL log file %s ",outfilename);
		return FAILURE;
	}
	else
		is_locked = false;

	return SUCCESS;
}

int FILESQL::file_readline(MyString *buf) 
{
    if (is_dummy) return TRUE;
	
	if(!fp)
		fp = fdopen(outfiledes, "r");

	return (buf->readLine(fp, true));
}

AttrList *FILESQL::file_readAttrList() 
{
	AttrList *ad = 0;

	if(!fp)
		fp = fdopen(outfiledes, "r");

	int EndFlag=0;
	int ErrorFlag=0;
	int EmptyFlag=0;

    if( !( ad=new AttrList(fp,"***\n", EndFlag, ErrorFlag, EmptyFlag) ) ){
		dprintf(D_ALWAYS, "file_readAttrList Error:  Out of memory\n" );
		exit( 1 );
    }

    if( ErrorFlag ) {
		dprintf( D_ALWAYS, "\t*** Warning: Bad Log file; skipping malformed Attr List\n" );
		ErrorFlag=0;
		delete ad;
		ad = 0;
    } 

	if( EmptyFlag ) {
		dprintf( D_ALWAYS, "\t*** Warning: Empty Attr List\n" );
		EmptyFlag=0;
		delete ad;
		ad = 0;
    }

	return ad;
}

QuillErrCode FILESQL::file_newEvent(const char *eventType, AttrList *info) {
	int retval = 0;
	struct stat file_status;

	if(!is_open)
	{
		dprintf(D_ALWAYS,"Error in logging to file : File not open");
		return FAILURE;
	}

	if(file_lock() == FAILURE) {
		return FAILURE;
	}

	fstat(outfiledes, &file_status);

		// only write to the log if it's not exceeding the log size limit
	if (file_status.st_size < FILESIZELIMT) {
		retval = write(outfiledes,"NEW ", strlen("NEW "));
		retval = write(outfiledes,eventType, strlen(eventType));
		retval = write(outfiledes,"\n", strlen("\n"));

		MyString temp;
		const char *tempv;
	
		retval = info->sPrint(temp);
		tempv = temp.Value();
		retval = write(outfiledes,tempv, strlen(tempv));

		retval = write(outfiledes,"***",3); /* Now the delimitor*/
		retval = write(outfiledes,"\n",1); /* Now the newline*/
	}
	
	if(file_unlock() == FAILURE) {
		return FAILURE;
	}

	if (retval < 0) {
		return FAILURE;
	} else {
		return SUCCESS;	
	}
}

QuillErrCode FILESQL::file_updateEvent(const char *eventType, 
									   AttrList *info, 
									   AttrList *condition) {
	int retval = 0;
	struct stat file_status;

	if(!is_open)
	{
		dprintf(D_ALWAYS,"Error in logging to file : File not open");
		return FAILURE;
	}

	if(file_lock() == FAILURE) {
		return FAILURE;
	}

	fstat(outfiledes, &file_status);

		// only write to the log if it's not exceeding the log size limit
	if (file_status.st_size < FILESIZELIMT) {
		retval = write(outfiledes,"UPDATE ", strlen("UPDATE "));
		retval = write(outfiledes,eventType, strlen(eventType));
		retval = write(outfiledes,"\n", strlen("\n"));

		MyString temp, temp1;
		const char *tempv;

		retval = info->sPrint(temp);
		tempv = temp.Value();
		retval = write(outfiledes,tempv, strlen(tempv));

		retval = write(outfiledes,"***",3); /* Now the delimitor*/
		retval = write(outfiledes,"\n",1); /* Now the newline*/

		retval = condition->sPrint(temp1);
		tempv = temp1.Value();
		retval = write(outfiledes,tempv, strlen(tempv));
		
		retval = write(outfiledes,"***",3); /* Now the delimitor*/
		retval = write(outfiledes,"\n",1); /* Now the newline*/	
	}

	if(file_unlock() == FAILURE) {
		return FAILURE;
	}

	if (retval < 0) {
		return FAILURE;	
	} else {
		return SUCCESS;	
	}
}

#if 0
QuillErrCode FILESQL::file_deleteEvent(const char *eventType, 
									   AttrList *condition) {
	int retval = 0;
	struct stat file_status;

	if(!is_open)
	{
		dprintf(D_ALWAYS,"Error in logging to file : File not open");
		return FAILURE;
	}

	if(file_lock() == FAILURE) {
		return FAILURE;
	}

	fstat(outfiledes, &file_status);

		// only write to the log if it's not exceeding the log size limit
	if (file_status.st_size < FILESIZELIMT) {
		retval = write(outfiledes,"DELETE ", strlen("DELETE "));
		retval = write(outfiledes,eventType, strlen(eventType));
		retval = write(outfiledes,"\n", strlen("\n"));

		MyString temp;
		const char *tempv;
	
		retval = condition->sPrint(temp);
		tempv = temp.Value();
		retval = write(outfiledes,tempv, strlen(tempv));

		retval = write(outfiledes,"***",3); /* Now the delimitor*/
		retval = write(outfiledes,"\n",1); /* Now the newline*/
	}

	if(file_unlock() == FAILURE) {
		return FAILURE;
	}

	if (retval < 0) {
		return FAILURE;	
	}
	else {
		return SUCCESS;
	}

}
#endif

/* We put FileObj definition here because modules such as file_transfer.o and
 * classad_log.o uses FILEObj and they are part of cplus_lib.a. This way we 
 * won't get the FILEObj undefined error during compilation of any code which 
 * needs cplus_lib.a. Notice the FILEObj is just a pointer, the real object 
 * should be created only when a real database connection is needed. E.g. 
 * most daemons need database connection, there we can create a database 
 * connection in the  main function of daemon process.
 */

FILESQL *FILEObj = NULL;

FILESQL *createInstance() { 
	FILESQL *ptr = NULL;
	char *tmp, *outfilename;
	char *tmpParamName;
	const char *daemon_name;
	
	daemon_name = get_mySubSystem();

	tmpParamName = (char *)malloc(10+strlen(daemon_name));

		/* build parameter name based on the daemon name */
	sprintf(tmpParamName, "%s_SQLLOG", daemon_name);
	tmp = param(tmpParamName);
	free(tmpParamName);

	if( tmp ) {
		outfilename = tmp;
	}
	else {
		tmp = param ("LOG");		

		if (tmp) {
			outfilename = (char *)malloc(strlen(tmp) + 10);
			sprintf(outfilename, "%s/sql.log", tmp);
			free(tmp);
		}
		else {
			outfilename = (char *)malloc(10);
			sprintf(outfilename, "sql.log");
		}
	}

	ptr = new FILESQL(outfilename, O_WRONLY|O_CREAT|O_APPEND);

	free(outfilename);

	if (ptr->file_open() == FAILURE) {
		dprintf(D_ALWAYS, "FILESQL createInstance failed\n");
	}

	return ptr;
}

