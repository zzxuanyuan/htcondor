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
  
  ~CacheflowManager()
  {
    if (m_cacheflow_manager)
      delete m_cacheflow_manager;
  }

  int pingCacheflowManager(const std::string cacheflowManager) {

  }

  boost::shared_ptr<ClassAdWrapper> getStoragePolicy(classad::ClassAd& jobAd) {
    printf("In getStoragePolicy\n");
    compat_classad::ClassAd jobAd2(jobAd);
//    std::string new_cacheflowManager = cacheflowManager;
    compat_classad::ClassAd responseAd;
    CondorError err;
    int rc = m_cacheflow_manager->getStoragePolicy(jobAd2, responseAd, err);
    if (rc) {
      PyErr_Format(PyExc_RuntimeError, "Error creating cache directory: %s", err.getFullText().c_str());
      throw_error_already_set();
    }
    boost::shared_ptr<ClassAdWrapper> wrapper(new ClassAdWrapper());
    wrapper->CopyFrom(responseAd);
    return wrapper;
  }

private:
  DCCacheflowManager* m_cacheflow_manager;

};

void export_cacheflow_manager()
{
    class_<CacheflowManager>("CacheflowManager", "Client-side operations for the HTCondor CacheflowManager")
        .def("pingCacheflowManager", &CacheflowManager::pingCacheflowManager, "Ping Cacheflow Manager\n"
            ":param cacheflowManager: A Cacheflow Manager's Name\n")
        .def("getStoragePolicy", &CacheflowManager::getStoragePolicy, "Get Storage Policy\n"
            ":param jobAd: A classad describing the job importance and reliability requirement\n"
            ":return: A classad describing the storage policy assigned to the current cached\n")
        ;
}
