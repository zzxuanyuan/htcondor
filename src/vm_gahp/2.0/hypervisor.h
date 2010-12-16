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

#ifndef VMGAHP_HYPERVISOR
#define VMGAHP_HYPERVISOR

#include <boost/shared_ptr.hpp>
#include <boost/variant.hpp>
#include <string>
#include <map>

#include "vm_stats.h"

namespace condor
{

    namespace vmu
    {

    typedef std::map< std::string, boost::variant<int, std::string> > nvps;
    //typedef std::map< std::string, nvps > vm_caps;

    /**
     * The following is an abstract base class for a hypervisor
     * It's meant to abstract away as much of condor and direct knowledge
     * of a hypervisor as possible such that
     *
     * The idea is you could take this piece out of condor and
     * verify it independently
     *
     * @author Timothy St. Clair
     */
    class hypervisor
    {
    public:

         hypervisor();
         virtual ~hypervisor();

         ///////////////////////////////////////////////////////////////
         // static functions to enable factory-esk approach
         ///////////////////////////////////////////////////////////////

         /**
          * discover() -
          *
          * @param check - a map containing the vms to check with nvps partially
          * filled, based on config options
          */
         //static bool discover( vm_caps & check );

         /**
          * manufacture() - will manufacture an instance based on std::string
          */
         //static boost::shared_ptr<hypervisor> manufacture (const std::string & vm_type);

         ///////////////////////////////////////////////////////////////
         // virtual interface functions
         // ?'s
         // can you reconfig a vm once it is running?
         // what is the difference is a soft suspend?
         ///////////////////////////////////////////////////////////////

         /**
          * start() - will start a vm using the mundged input file.
          */
         virtual bool start(std::string & szConfigFile)=0;

         /**
          */
         virtual bool suspend( bool bSoft=false )=0;

         /**
          */
         virtual bool resume()=0;

         /**
          */
         virtual bool checkpoint(/*name?*/)=0;

         /**
          */
         virtual bool shutdown(bool reboot=false, bool bforce=false)=0;

         /**
          */
         virtual bool getStats( vm_stats & stats )=0;

    private:

        /**
         */
        virtual bool check_caps(nvps & caps)=0;

        vm_stats m_state; ///< Current state of the vm;

    };


    }

}

#endif