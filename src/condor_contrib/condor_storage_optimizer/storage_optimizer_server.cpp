#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "compat_classad.h"
#include "file_transfer.h"
#include "condor_version.h"
#include "classad_log.h"
#include "classad_hashtable.h"
#include "get_daemon_name.h"
#include "ipv6_hostname.h"
#include <list>
#include <map>
#include "basename.h"

#include "storage_optimizer_server.h"
#include "dc_storage_optimizer.h"

#include <sstream>

StorageOptimizerServer::StorageOptimizerServer()
{
}

StorageOptimizerServer::~StorageOptimizerServer()
{
}
