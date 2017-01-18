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

#include "../condor_contrib/condor_cached/dc_cached.h"

using namespace boost::python;

struct Cached {

  Cached()
  {
    m_cached = new DCCached();
  }
  
  Cached(const ClassAdWrapper &ad) 
  {
    std::string cache_name;
    if (!ad.EvaluateAttrString(ATTR_NAME, cache_name))
    {
      PyErr_SetString(PyExc_ValueError, "Cache name not specified");
      throw_error_already_set();
    }
    
    m_cached = new DCCached(cache_name.c_str());
    
  }
  
  ~Cached()
  {
    if (m_cached)
      delete m_cached;
  }
  
  
  void createCacheDir(const std::string cacheName, const time_t &expiry) {

    printf("In createCacheDir\n");//##
    std::string new_cacheName = cacheName;
    time_t new_expiry = expiry;
    
    CondorError err;
    int rc = m_cached->createCacheDir(new_cacheName, new_expiry, err);
    printf("m_cached->createCacheDir, and rc=%d\n",rc);//##
    if (rc) {
      PyErr_Format(PyExc_RuntimeError, "Error creating cache directory: %s", err.getFullText().c_str());
      throw_error_already_set();
    }
    
  }
  
  void uploadFiles(const std::string &cacheName, const list files) {
    printf("In uploadFiles begin\n");//## 
    if (py_len(files) == 0) {
      PyErr_SetString(PyExc_ValueError, "files list is empty");
      throw_error_already_set();
    }
    
    CondorError err;
    std::list<std::string> files_list;
    
    for( int i = 0; i < py_len(files); i++) {
      files_list.push_back( extract<std::string>(files[i]) );
      printf("file[%d]=%s\n", i, files_list.back().c_str());//##
    }
    
    int rc = m_cached->uploadFiles(cacheName, files_list, err);
    printf("In uploadFiles and rc=%d\n", rc);//##
    if (rc) {
      PyErr_Format(PyExc_RuntimeError, "Error uploading files: %s", err.getFullText().c_str());
      throw_error_already_set();
    }
    
  }
  
  
  void downloadFiles(const std::string &cacheName, const std::string& dest) {
    
    CondorError err;
    
    int rc = m_cached->downloadFiles(cacheName, dest, err);
    
    if (rc) {
      PyErr_Format(PyExc_RuntimeError, "Error downloading files: %s", err.getFullText().c_str());
      throw_error_already_set();
    }
    
  }
  
  
  void removeCacheDir(const std::string &cacheName) {
    
    CondorError err;
    
    int rc = m_cached->removeCacheDir(cacheName, err);
    
    if (rc) {
      PyErr_Format(PyExc_RuntimeError, "Error removing cache %s: %s", cacheName.c_str(), err.getFullText().c_str());
      throw_error_already_set();
    }
    
  }
  
  
  void setReplicationPolicy(const std::string &cacheName, const std::string &policy, const std::string methods) {
    
    CondorError err;
    
    int rc = m_cached->setReplicationPolicy(cacheName, policy, methods, err);
    
    if (rc) {
      PyErr_Format(PyExc_RuntimeError, "Error setting replication policy: %s", err.getFullText().c_str());
      throw_error_already_set();
    }
    
  }

  
  object listCacheDirs(const std::string &cacheName = "", const std::string &requirements = "") {
    
    CondorError err;
    std::list<compat_classad::ClassAd> result_list;
    
    int rc = m_cached->listCacheDirs(cacheName, requirements, result_list, err);
    
    if(rc) {
      PyErr_Format(PyExc_RuntimeError, "Error getting list of cache directories: %s", err.getFullText().c_str());
      throw_error_already_set();
    }
    
    // Convert the std::list to a python list
    list return_list;
    for(std::list<compat_classad::ClassAd>::iterator it = result_list.begin(); it != result_list.end(); it++) {
      
      boost::shared_ptr<ClassAdWrapper> wrapper(new ClassAdWrapper());
      wrapper->CopyFrom(*it);
      return_list.append(wrapper);
    }
    
    return return_list;
    
  }
  
  boost::shared_ptr<ClassAdWrapper> requestLocalCache(const std::string &cachedServer, const std::string &cacheName) {
    
    compat_classad::ClassAd responseAd;
    CondorError err;
    
    int rc = m_cached->requestLocalCache(cachedServer, cacheName, responseAd, err);
    
    if (rc) {
      THROW_EX(RuntimeError, err.getFullText().c_str());
    }
    
    boost::shared_ptr<ClassAdWrapper> wrapper(new ClassAdWrapper());
    wrapper->CopyFrom(responseAd);
    return wrapper;
    
  }
  
  
  
private:
  DCCached* m_cached;



};


BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(listCacheDirs_overloads, listCacheDirs, 0, 2);

void export_cached()
{
    class_<Cached>("Cached", "Client-side operations for the HTCondor Cached")
        .def(init<const ClassAdWrapper &>(":param ad: An ad containing the location of the schedd"))
        .def("createCacheDir", &Cached::createCacheDir, "Create a Cache Directory\n"
            ":param cacheName: A name for the Cache\n"
            ":param expiry: A expiration time for the Cache\n")
        .def("uploadFiles", &Cached::uploadFiles, "Upload files to a caches\n"
            ":param cacheName: The cache name\n"
            "param files: A list of files to upload\n")
        .def("downloadFiles",  &Cached::downloadFiles, "Download files from a Cache\n"
            ":param cacheName: The cache's name\n"
            ":param dest: Destination directory for the cache contents\n")
        .def("removeCacheDir", &Cached::removeCacheDir, "Remove Cache directory\n"
            ":param cacheName: Cache to delete\n")
        .def("setReplicationPolicy", &Cached::setReplicationPolicy, "Set replication policy for a cache\n", 
            ":param cacheName: Cache name\n"
            ":param policy: Policy to for cache replication\n"
            ":param methods: Methods for cache replication\n")
        .def("listCacheDirs", &Cached::listCacheDirs, listCacheDirs_overloads("Get list of Cache Classads\n"
            ":param cacheName: Cache name\n"
            ":param requirements: Requirement expression to match against\n"
            ":return: A list of ads in the cached either with cacheName or match requirements expression\n"))
        .def("requestLocalCache", &Cached::requestLocalCache, "Request a local cached to copy a cache\n"
            ":param cachedServer: Cached origin server\n"
            ":param cacheName: Cache Name\n"
            ":return: A classad describing the current state of the replication\n")
        ;
}
