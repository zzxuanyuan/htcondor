#include "condor_common.h"
#include "MyString.h"
#include "HashTable.h"
#include "list.h"

#include "schedd_api.h"

template class HashTable<MyString, JobFile>;
template class List<FileInfo>;
template class Item<FileInfo>;
