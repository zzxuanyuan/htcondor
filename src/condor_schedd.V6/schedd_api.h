#ifndef _schedd_api_h
#define _schedd_api_h

#include "condor_common.h"
#include "condor_classad.h"
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
  FileInfo(MyString name, unsigned long size);
  ~FileInfo();

  MyString name;
  unsigned long size;
};

class Job
{
public:
  Job(int clusterId, int jobId);
  ~Job();

  int submit(struct ClassAdStruct jobAd);
  int declare_file(MyString name,
                   int size);
  int send_file(MyString name,
                int offset,
                char * data,
                int data_length);
  int get_file(MyString name,
               int offset,
               int length,
               unsigned char * &data);

  int get_spool_list(List<FileInfo> & file_list);

  int abort();

  int getClusterID();

protected:
  int clusterId;
  int jobId;
  HashTable<MyString, JobFile> *requirements;
  MyString *spoolDirectory;
};

#endif
