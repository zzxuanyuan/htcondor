#ifndef _schedd_api_h
#define _schedd_api_h

#include "condor_common.h"
#include "condor_classad.h"
#include "CondorError.h"
#include "MyString.h"
#include "HashTable.h"
#include "directory.h"
#include "list.h"

#include "soap_scheddH.h"

class JobFile
{
 public:
	JobFile();
	~JobFile();

	int file;
	MyString name;
	int size;
	int currentOffset;
};

class FileInfo
{
 public:
	FileInfo(const char *name, unsigned long size);
	~FileInfo();

	char *name;
	unsigned long size;
};

class Job
{
 public:
	Job(int clusterId, int jobId);
	~Job();

	int initialize(CondorError &errstack);

	int submit(const struct ClassAdStruct &jobAd,
			   CondorError &errstack);
	int declare_file(const MyString &name,
					 int size,
					 CondorError &errstack);
	int put_file(const MyString &name,
				 int offset,
				 char * data,
				 int data_length,
				 CondorError &errstack);
	int get_file(const MyString &name,
				 int offset,
				 int length,
				 unsigned char * &data,
				 CondorError &errstack);
	
	int get_spool_list(List<FileInfo> &file_list,
					   CondorError &errstack);

	int abort(CondorError &errstack);

	int getClusterID();

 protected:
	int clusterId;
	int jobId;
	HashTable<MyString, JobFile> *declaredFiles;
	MyString spoolDirectory;
};

#endif
