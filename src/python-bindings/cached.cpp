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

#include "condor_cached/dc_cached.h"

using namespace boost::python;

struct Cached {

  Cached()
  {
    m_cached = new DCCached();
  }
  
  ~Cached()
  {
    if (m_cached)
      delete m_cached;
  }
  
  
  void createCacheDir(const std::string cacheName, const time_t &expiry) {
    
    std::string new_cacheName = cacheName;
    time_t new_expiry = expiry;
    
    CondorError err;
    int rc = m_cached->createCacheDir(new_cacheName, new_expiry, err);
    
  }
  
  
private:
  DCCached* m_cached;



};


void export_cached()
{
    class_<Cached>("Cached", "Client-side operations for the HTCondor Cached")
        .def("createCacheDir", &Cached::createCacheDir, "Create a Cache Directory\n"
            ":param cacheName: A name for the Cache\n"
            ":param expiry: A expiration time for the Cache\n")
        ;
}
