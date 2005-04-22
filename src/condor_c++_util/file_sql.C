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
	strcpy(outfilename,"");
	fileflags = O_WRONLY|O_CREAT|O_APPEND;
	outfiledes = -1;
	fp = NULL;
}
FILESQL::FILESQL(char *outfilename, int flags = O_WRONLY|O_CREAT|O_APPEND)
{
	is_open = false;
	is_locked = false;
	strncpy(this->outfilename,outfilename,256);
	fileflags = flags;
	outfiledes = -1;
	fp = NULL;
}

FILESQL::~FILESQL()
{
	if(file_isopen())
		file_close();
	is_open = false;
	is_locked = false;
	strcpy(this->outfilename,"");
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
	delete lock; /* This also releases the lock on the file, were it held */
	retval = close(outfiledes);
	if(retval < 0)
		dprintf(D_ALWAYS,"Error closing SQL log file %s : %s",outfilename,strerror(errno));
	is_open = false;
	is_locked = false;
	outfiledes = -1;
	return retval;
}

long FILESQL::file_sqlstmt(const char* statement)
{
	int retval = 0;
	if(!is_open)
	{
		dprintf(D_ALWAYS,"Error in logging SQL statement \'%s\' to file : File not open",statement);
		return -1;
	}
	dprintf(D_FULLDEBUG,"Logging %s to file %s\n",statement,outfilename);
	if(file_lock() < 0)
		return -1;
	
	retval = write(outfiledes,statement,strlen(statement)); /* The statement excluding newline */
	retval = write(outfiledes,"\n",1); /* Now the newline*/
//	fsync(outfiledes);
	if(retval < 0) /* There was an error */
		dprintf(D_ALWAYS,"Error in logging SQL statement \'%s\' to file : %s",statement,strerror(errno));

	if(file_unlock() < 0)
		return -1;
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

int FILESQL::file_readline(char *buf) 
{
	if(!fp)
		fp = fdopen(outfiledes, "r");

	return fscanf(fp, " %[^\n]", buf);
}

FILESQL *createInstance() 
{ 
/* Passing paramName is a hack to get around reading the right parameter depending on daemon name */
	FILESQL *ptr = NULL;
	int retval;
	char *tmp, *outfilename;
	char tmpParamName[50];
	
	/* Parse FILESQL Params */
	sprintf(tmpParamName, "%s_SQLLOG", get_mySubSystem());
	tmp = param(tmpParamName);
	if( tmp ) {
		outfilename = strdup(tmp);
		free(tmp);
	}
	else {
		outfilename = strdup("sql.log");
	}
	ptr = new FILESQL(outfilename, O_WRONLY|O_CREAT|O_APPEND);

	retval = ptr->file_open();
	if (retval < 0) 
	{
		dprintf(D_ALWAYS, "FILESQL createInstance failed\n");
	}

	return ptr;
}

