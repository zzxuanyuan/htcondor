#include "condor_api.h"
#include "condor_config.h"
#include <errno.h>
#include <string.h>
#include "file_sql.h"
#include "get_mysubsystem.h"

// define the pointer to database connection object here since the odbc.o is put into 
// the cplus_lib.a library. And that is because modules such as file_transfer.o and
// classad_log.o uses FILEObj and they are part of cplus_lib.a. This way we won't get
// the FILEObj undefined error during compilation of any code which needs cplus_lib.a.

// notice the FILEObj is just a pointer, the real object should be created only when 
// a real database connection is needed. E.g. most daemons need database connection, 
// there we can create a database connection in the  main function of daemon process.
FILESQL *FILEObj = 0;
//extern int errno;

FILESQL::FILESQL()
{
	is_open = false;
	is_locked = false;
	outfilename = (char *) 0;
	fileflags = O_WRONLY|O_CREAT|O_APPEND;
	outfiledes = -1;
	fp = NULL;
	lock = (FileLock *)0;
}

FILESQL::FILESQL(char *outfilename, int flags)
{
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
	if(file_isopen())
		file_close();
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

int FILESQL::file_truncate() {
	int retval;

	if(!file_isopen()) {
		dprintf(D_ALWAYS, "Error calling truncate: the file needs to be first opened\n");
		return -1;
	}

	retval = ftruncate(outfiledes, 0);

	if(retval < 0) {
		dprintf(D_ALWAYS, "Error calling ftruncate, errno = %d\n", errno);
		return -1;
	}

	return 1;
}

long FILESQL::file_open()
{
	if (!outfilename) {
		dprintf(D_ALWAYS,"No SQL log file specified");
		return -1;
	}

	outfiledes = open(outfilename,fileflags,0644);

	if(outfiledes < 0)
	{
		dprintf(D_ALWAYS,"Error opening SQL log file %s : %s",outfilename,strerror(errno));
		is_open = false;
		return -1;
	}
	else
	{
		is_open = true;
		lock = new FileLock(outfiledes,NULL); /* Create a lock object when opening the file */
		return 0;
	}
}

long FILESQL::file_close()
{
	int retval =0;
	
	if (!is_open)
		return retval;

	if (lock) {
		delete lock; /* This also releases the lock on the file, were it held */
		lock  = (FileLock *)0;
	}

		// fp is associated with the outfiledes, we should be closing one or the other, 
		// but not both. Otherwise the second one will give an error 
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
	return retval;
}

long FILESQL::file_lock()
{
	if(!is_open)
	{
		dprintf(D_ALWAYS,"Error locking :SQL log file %s not open yet",outfilename);
		return -1;
	}

	if(is_locked)
		return 0;

	if(lock->obtain(WRITE_LOCK) == 0) /* 0 means an unsuccessful lock */
	{
		dprintf(D_ALWAYS,"Error locking SQL log file %s ",outfilename);
		return -1;
	}
	else
		is_locked = true;

	return 0;
}

long FILESQL::file_unlock()
{
	if(!is_open)
	{
		dprintf(D_ALWAYS,"Error unlocking :SQL log file %s not open yet",outfilename);
		return -1;
	}

	if(!is_locked)
		return 0;

	if(lock->release() == 0) /* 0 means an unsuccessful lock */
	{
		dprintf(D_ALWAYS,"Error unlocking SQL log file %s ",outfilename);
		return -1;
	}
	else
		is_locked = false;

	return 0;
}

int FILESQL::file_readline(MyString *buf) 
{
	if(!fp)
		fp = fdopen(outfiledes, "r");

	return (buf->readLine(fp, true));

		//return fscanf(fp, " %[^\n]", buf);
}

AttrList *FILESQL::file_readAttrList() 
{
	AttrList *ad = 0;

	if(!fp)
		fp = fdopen(outfiledes, "r");

	int EndFlag=0;
	int ErrorFlag=0;
	int EmptyFlag=0;

    if( !( ad=new AttrList(fp,"***", EndFlag, ErrorFlag, EmptyFlag) ) ){
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

long FILESQL::file_newEvent(const char *eventType, AttrList *info) {
	int retval = 0;

	if(!is_open)
	{
		dprintf(D_ALWAYS,"Error in logging to file : File not open");
		return -1;
	}

	if(file_lock() < 0)
		return -1;

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

	
	if(file_unlock() < 0)
		return -1;

	return retval;	
}

long FILESQL::file_updateEvent(const char *eventType, AttrList *info, AttrList *condition) {
	int retval = 0;

	if(!is_open)
	{
		dprintf(D_ALWAYS,"Error in logging to file : File not open");
		return -1;
	}

	if(file_lock() < 0)
		return -1;


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
	
	if(file_unlock() < 0)
		return -1;

	return retval;	
}

#if 0
long FILESQL::file_deleteEvent(const char *eventType, AttrList *condition) {
	int retval = 0;

	if(!is_open)
	{
		dprintf(D_ALWAYS,"Error in logging to file : File not open");
		return -1;
	}

	if(file_lock() < 0)
		return -1;

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

	if(file_unlock() < 0)
		return -1;

	return retval;	
}
#endif

FILESQL *createInstance() { 
	FILESQL *ptr = NULL;
	int retval;
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

	retval = ptr->file_open();

	if (retval < 0) 
	{
		dprintf(D_ALWAYS, "FILESQL createInstance failed\n");
	}

	return ptr;
}

