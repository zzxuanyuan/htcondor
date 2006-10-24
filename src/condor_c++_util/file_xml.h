#include <string.h>
#include <file_lock.h>
#include <condor_attrlist.h>
#include "quill_enums.h"
#include "file_sql.h"
#ifndef FILEXML_H
#define FILEXML_H

/*
 * Config params used by XML Logger:
 * 
 * WANT_XML_LOG: Switch to turn off XML Logging
 * MAX_XML_LOG: Max size of XML Log (in bytes)
 * <SUBSYS>_XMLLOG: Filename of XML Log used by <SUBSYS>
 *
 */

class FILEXML : public FILESQL
{

/*
private:
	bool 	is_open;
	bool 	is_locked;
	char *outfilename;
	int fileflags;
	int outfiledes;
	FileLock *lock;
	FILE *fp;
*/
public:
	
	FILEXML() { is_dummy = true; }
	FILEXML(char *outfilename,int flags=O_WRONLY|O_CREAT|O_APPEND, bool dummy=false) : FILESQL(outfilename, flags) { is_dummy = dummy; }
	~FILEXML() {}

/*
	bool file_isopen();
	bool file_islocked();
	QuillErrCode file_open();
	QuillErrCode file_close();
	QuillErrCode file_lock();
	QuillErrCode file_unlock();
*/
	QuillErrCode file_newEvent(const char *eventType, AttrList *info);
	QuillErrCode file_updateEvent(const char *eventType, AttrList *info, 
								  AttrList *condition);
//	QuillErrCode file_deleteEvent(const char *eventType, AttrList *condition);

//	int  file_readline(MyString *buf);
	AttrList  *file_readAttrList();
//	QuillErrCode  file_truncate();

};

FILEXML *createInstanceXML();

#endif
