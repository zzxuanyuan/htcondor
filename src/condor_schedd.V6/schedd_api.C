#include "condor_common.h"
#include "condor_classad.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

// Things to include for the stubs
#include "condor_version.h"
#include "condor_attributes.h"
#include "scheduler.h"
#include "condor_qmgr.h"
#include "MyString.h"
#include "internet.h"

#include "condor_ckpt_name.h"
#include "condor_config.h"

#include "schedd_api.h"

Job::Job(int clusterId, int jobId)
{
  this->clusterId = clusterId;
  this->jobId = jobId;

  requirements = new HashTable<MyString, JobFile>(64, MyStringHash, rejectDuplicateKeys);

  char * Spool = param("SPOOL");
  if (Spool) { // XXX: Else big error
    spoolDirectory = new MyString(strdup(gen_ckpt_name(Spool, clusterId, jobId, 0)));

    struct stat stats;
    if (stat(spoolDirectory->GetCStr(), &stats)) {
      if ((mkdir(spoolDirectory->GetCStr(), 0777) < 0)) {
        // mkdir can return 17 = EEXIST (dirname exists) or 2 = ENOENT (path not found)
        dprintf(D_FULLDEBUG,
                "Job::declareFile: mkdir(%s) failed, errno: %d\n",
                spoolDirectory->GetCStr(),
                errno);
      }
    }
  }
}

Job::~Job()
{
  // XXX: Duplicate code with abort(), almost.
  MyString currentKey;
  JobFile jobFile;
  requirements->startIterations();
  while (requirements->iterate(currentKey, jobFile)) {
    fclose(jobFile.file);
    requirements->remove(currentKey);
  }

  delete requirements;
  delete spoolDirectory;
}

int
Job::abort()
{
  MyString currentKey;
  JobFile jobFile;
  requirements->startIterations();
  while (requirements->iterate(currentKey, jobFile)) {
    fclose(jobFile.file);
    requirements->remove(currentKey);
    remove(jobFile.name.GetCStr());
  }

  remove(spoolDirectory->GetCStr());

  return 0;
}

int
Job::getClusterID()
{
  return clusterId;
}

int
Job::declare_file(MyString name,
                  int size)
{
  JobFile jobFile;
  jobFile.size = size;
  jobFile.currentOffset = 0;

  FILE *file;

  // XXX: Handle errors!
  // XXX: How do I get the FS separator?
  jobFile.name = name;
  file = fopen((*spoolDirectory + "/" + jobFile.name).GetCStr(), "w");
  jobFile.file = file;
  requirements->insert(MyString(name), jobFile);

  return 0;
}

int
Job::submit(struct condorCore__ClassAdStruct jobAd)
{
  int i,rval;

  // XXX: This is ugly, and only should happen when spooling, i.e. not always with cedar.
  rval = SetAttributeString(clusterId, jobId, ATTR_JOB_IWD, spoolDirectory->GetCStr());
  if (rval < 0) {
    return rval;
  }

  StringList transferFiles;
  MyString currentKey;
  JobFile jobFile;
  requirements->startIterations();
  while (requirements->iterate(currentKey, jobFile)) {
    transferFiles.append(jobFile.name.GetCStr());
  }
  rval = SetAttributeString(clusterId, jobId, ATTR_TRANSFER_INPUT_FILES, transferFiles.print_to_string());
  if (rval < 0) {
    return rval;
  }

  for (i=0; i < jobAd.__size; i++ ) {
    const char* name = jobAd.__ptr[i].name;
    const char* value = jobAd.__ptr[i].value;
    if (!name) continue;
    if (!value) value="UNDEFINED";

    if ( jobAd.__ptr[i].type == 's' ) {
      // string type - put value in quotes as hint for ClassAd parser

      // We need to make sure the Iwd is rewritten so files
      // in the spool directory can be found.
      if ((NULL != spoolDirectory) &&
          (0 == strcmp(name, "Iwd"))) {
        value = spoolDirectory->GetCStr();
      }

      rval = SetAttributeString(clusterId, jobId, name, value);
    } else {
      // all other types can be deduced by the ClassAd parser
      rval = SetAttribute(clusterId,jobId,name,value);
    }
    if ( rval < 0 ) {
      return rval;
    }
  }

  return 0;
}

int
Job::send_file(MyString name,
               int offset,
               char * data,
               int data_length)
{
  JobFile jobFile;
  if (-1 == requirements->lookup(MyString(name), jobFile)) {
    return 1; // Unknown file.
  }

  // XXX: Handle errors!
  fseek(jobFile.file, offset, SEEK_SET);
  fwrite(data, sizeof(unsigned char), data_length, jobFile.file);
  fflush(jobFile.file);

  return 0;
}

int
Job::get_file(MyString name,
              int offset,
              int length,
              unsigned char * &data)
{
  // XXX: Handle errors!
  FILE * file = fopen((*spoolDirectory + "/" + name).GetCStr(), "r");
  fseek(file, offset, SEEK_SET);
  fread(data, sizeof(unsigned char), length, file);
  fclose(file);

  return 0;
}
