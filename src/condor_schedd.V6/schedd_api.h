#include "condor_common.h"
#include "condor_classad.h"
#include "MyString.h"
#include "HashTable.h"

#include "soap_scheddH.h"

class JobFile
{
public:
  FILE * file;
  MyString name;
  int size;
  int currentOffset;
};

template class HashTable<MyString, JobFile>;

class Job
{
public:
  Job(int clusterId, int jobId);
  ~Job();

  int submit(struct condorCore__ClassAdStruct jobAd);
  int declare_file(MyString name,
                   int size);
  int send_file(MyString name,
                int offset,
                char * data,
                int data_length);

  int abort();

protected:
  int clusterId;
  int jobId;
  HashTable<MyString, JobFile> *requirements;
  MyString *spoolDirectory;
};
