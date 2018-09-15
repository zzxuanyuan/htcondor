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

#include "../condor_contrib/condor_storage_optimizer/dc_storage_optimizer.h"

using namespace boost::python;

struct StorageOptimizer {

  StorageOptimizer()
  {
    m_storage_optimizer = new DCStorageOptimizer();
  }
  
  ~StorageOptimizer()
  {
    if (m_storage_optimizer)
      delete m_storage_optimizer;
  }
  
  void pingStorageOptimizer(const std::string storageOptimizer) {
  }

  object listStorageOptimizers(const std::string& requirements = "") {
    printf("In python binding listStorageOptimizers\n");//##
    CondorError err;
    std::list<compat_classad::ClassAd> result_list;
    
    int rc = m_storage_optimizer->listStorageOptimizers(requirements, result_list, err);
    
    if(rc) {
      PyErr_Format(PyExc_RuntimeError, "Error getting list of StorageOptimizers: %s", err.getFullText().c_str());
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

private:
  DCStorageOptimizer* m_storage_optimizer;
};

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(listStorageOptimizers_overloads, listStorageOptimizers, 0, 1);

void export_storage_optimizer()
{
    class_<StorageOptimizer>("StorageOptimizer", "Client-side operations for the HTCondor StorageOptimizer")
        .def("pingStorageOptimizer", &StorageOptimizer::pingStorageOptimizer, "Ping Storage Optimizer\n"
            ":param storageOptimizer: A Storage Optimizer's Name\n")
        .def("listStorageOptimizers", &StorageOptimizer::listStorageOptimizers, listStorageOptimizers_overloads("Get list of StorageOptimizer Classads\n"
            ":param requirements: Requirement expression to match against\n"
            ":return: A list of ads of all storage optimizers match requirements expression\n"))
        ;
}
