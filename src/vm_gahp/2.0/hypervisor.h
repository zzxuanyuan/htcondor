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

#include "string_list.h" // I need to kill this
#include "vm_stats.h"

namespace condor
{

    namespace vmu
    {

     /**
     * The following ...
     */
    class hypv_config
    {
    public:
        hypv_config();
        virtual ~hypv_config();

        virtual bool InsertAddAttr(ClassAd & ad);
        virtual bool read_config();

        std::string     m_VM_TYPE;
        int             m_VM_MEMROY;     // this should be unsigned but there is a classad failure
        bool            m_VM_NETWORKING;
        std::string     m_VM_NETWORKING_DEFAULT_TYPE;
        StringList      m_VM_NETWORKING_TYPE;
    };

    /**
     * The following is an abstract base class for a hypervisor
     * It's meant to abstract away as much of condor and direct knowledge
     * of a hypervisor as possible, such that the controller knows nothing about
     * the specifics of a hypervisor and the interface is clear of condor
     * deps.
     *
     * If folks wanted to add another hypervisor say vmware, or virtbox, or
     * whatever, they could derive from *this class.  It's also important to
     * keep hypv_config generic enough for
     *
     * @author Timothy St. Clair
     */
    class hypervisor
    {
        friend class hypervisor_factory;

    public:

         hypervisor(){;};
         virtual ~hypervisor(){;};

        /**
         *
         */
        virtual bool config(const boost::shared_ptr<hypv_config> & local_config)=0;

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

         /**
         */
        virtual bool check_caps( boost::shared_ptr<hypv_config> & local_config )=0;

    protected:

        vm_stats m_state;   ///< Current state of the vm;

    };


    }

}

#endif