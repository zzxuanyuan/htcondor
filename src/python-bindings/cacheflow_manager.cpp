// Note - pyconfig.h must be included before condor_common to avoid
// re-definition warnings.
# include <pyconfig.h>

#include "condor_adtypes.h"
#include "dc_collector.h"
#include "condor_version.h"

#include <memory>
#include <boost/python.hpp>

#include "old_boost.h"
#include "classad_wrapper.h"

#include "../condor_contrib/condor_cacheflow_manager/dc_cacheflow_manager.h"

using namespace boost::python;

struct CacheflowManager {

  CacheflowManager()
  {
    m_cacheflow_manager = new DCCacheflowManager();
  }
  
  ~Cached()
  {
    if (m_cacheflow_manager)
      delete m_cacheflow_manager;
  }

  void pingCacheflowManager(const std::string cacheflowManager) {
  }

private:
  DCCacheflowManager* m_cacheflow_manager;

};

void export_cacheflow_manager()
{
    class_<CacheflowManager>("CacheflowManager", "Client-side operations for the HTCondor CacheflowManager")
        .def(init<const ClassAdWrapper &>(":param ad: An ad containing the location of the schedd"))
        .def("pingCacheflowManager", &Cached::pingCacheflowManager, "Ping Cacheflow Manager\n"
            ":param cacheflowManager: A Cacheflow Manager's Name\n")
        ;
}
