/***************************************************************
 *
 * Copyright (C) 1990-2010, Redhat.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#ifndef VMGAHP_HYPERVISOR_FACTORY
#define VMGAHP_HYPERVISOR_FACTORY

#include "hypervisor.h"
#include <boost/function.hpp>
#include <map>

namespace condor
{
    namespace vmu
    {

     /**
     * hypervisor_factory is a boosty pluggable class factory, which
     * uses statics to ensure "there can be only one".
     *
     * @author Timothy St. Clair
     */
    class hypervisor_factory
    {
        friend class hypervisor;
    public:

        ///< pointer to a manufacture call which
        typedef boost::function<boost::shared_ptr<hypervisor> ( )> pfnManufacture;

         /**
          * discover() -
          *
          * @param
          */
         static bool discover( const std::string & szVMType, hypv_config & local_config );

         /**
          * manufacture() - will manufacture an instance based on string
          */
         static boost::shared_ptr<hypervisor> manufacture (const std::string & szVMType);

        /**
         * initFactory() - initialize the factory
         *
         * TODO: add input params to allow config to "load" "plugins" which in
         * in turn register with factory and support the hypervisor interface
         */
         static bool init(/*const std::vector<std::string> & vPlugins*/);

    protected:
         /**
          * registerMfgFn() -
          */
         static bool registerMfgFn( const std::string& szVMType, const pfnManufacture & pfn );

    private:
        ///< data structure which registers functional callbacks to manufacture
         static std::map<std::string, pfnManufacture> m_SupportedHypervisors;

         ///< temporary, remove once "pluggable"
         bool m_bInitialized;

        ///< ensure that some fool doesn't try to create a factory
        hypervisor_factory(){;};

    };

    }/// end vmu

}/// end condor

#endif