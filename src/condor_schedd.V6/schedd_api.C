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

  if (Spool) {
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
  } else {
    EXCEPT("SPOOL is not defined.");
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

JobFile::JobFile()
{
}

JobFile::~JobFile()
{
}

FileInfo::FileInfo(MyString name, unsigned long size)
{
  this->name = name;
  this->size = size;
}

FileInfo::~FileInfo()
{
}

int
Job::get_spool_list(List<FileInfo> & file_list)
{
	StatInfo directoryInfo(spoolDirectory->GetCStr());
	if (directoryInfo.IsDirectory()) {
		Directory directory(spoolDirectory->GetCStr());
		const char * name;
		FileInfo *info;
		while (NULL != (name = directory.Next())) {
				// XXX: What is MyString(name) fails?
			info = new FileInfo(MyString(name), directory.GetFileSize());
			if (info) {
				if (!file_list.Append(info)) {
					return 2;
				}
			}
		}

		return 0;
	} else {
		dprintf(D_ALWAYS, "spoolDirectory == '%s'\n", spoolDirectory->GetCStr());

		return 1;
	}
}

int
Job::declare_file(MyString name,
                  int size)
{
	JobFile jobFile;
	jobFile.size = size;
	jobFile.currentOffset = 0;

	FILE *file;

	jobFile.name = name;
	file = fopen((*spoolDirectory + DIR_DELIM_STRING + jobFile.name).GetCStr(), "w");
	if (file) {
		jobFile.file = file;
		if (requirements->insert(MyString(name), jobFile)) {
			return 2;
		}

		return 0;
	} else {
			// XXX: Is this OK? If we cannot open the file we assume it has
			// some sort of path separators in it and we will just leave
			// it alone. If someone tries to do send_file they will fail
			// though. A BETTER way would be to actually test 'name' for
			// path separators!
		return 0;
	}
}

int
Job::submit(struct condorCore__ClassAdStruct jobAd)
{
  int i, rval;

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

  int found_iwd = 0;
  for (i = 0; i < jobAd.__size; i++) {
    const char* name = jobAd.__ptr[i].name;
    const char* value = jobAd.__ptr[i].value;
    if (!name) continue;
    if (!value) value="UNDEFINED";

    if ( jobAd.__ptr[i].type == 's' ) {
      // string type - put value in quotes as hint for ClassAd parser

      found_iwd = found_iwd || !strcmp(name, ATTR_JOB_IWD);

      rval = SetAttributeString(clusterId, jobId, name, value);
    } else {
      // all other types can be deduced by the ClassAd parser
      rval = SetAttribute(clusterId,jobId,name,value);
    }
    if ( rval < 0 ) {
      return rval;
    }
  }

  // Trust the client knows what it is doing if there is an Iwd.
  if (!found_iwd) {
    // We need to make sure the Iwd is rewritten so files
    // in the spool directory can be found.
    if (NULL != spoolDirectory) {
      rval = SetAttributeString(clusterId, jobId, ATTR_JOB_IWD, spoolDirectory->GetCStr());
      if (rval < 0) {
        return rval;
      }
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

		// XXX: Should all data written be unwritten depending on where the
		// failure happens?

	if (jobFile.file) {
		if (fseek(jobFile.file, offset, SEEK_SET)) {
			return 2;
		}
		if (data_length != fwrite(data, sizeof(unsigned char), data_length, jobFile.file)) {
			return 3;
		}
		if (EOF == fflush(jobFile.file)) {
			return 4;
		}
	} else {
			// This happens if declare_file could not open the 'name'.
		return 5;
	}

  return 0;
}

int
Job::get_file(MyString name,
              int offset,
              int length,
              unsigned char * &data)
{
  FILE * file = fopen((*spoolDirectory + "/" + name).GetCStr(), "r");

  if (file) {
    if (fseek(file, offset, SEEK_SET)) {
      return 2;
    }
    if (length != fread(data, sizeof(unsigned char), length, file)) {
      return 3;
    }
    if (EOF == fclose(file)) {
      return 4;
    }
  } else {
    return 1;
  }

  return 0;
}
