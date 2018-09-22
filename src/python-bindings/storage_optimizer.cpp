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

    StorageOptimizer() {
        m_storage_optimizer = new DCStorageOptimizer();
    }
  
    ~StorageOptimizer() {
        if (m_storage_optimizer)
            delete m_storage_optimizer;
    }
  
    void pingStorageOptimizer() {
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
        ;
}
