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

#ifndef VMGAHP_LIBVIRT_HYPERVISOR
#define VMGAHP_LIBVIRT_HYPERVISOR

#include "hypervisor"

namespace condor
{
    namespace vmu
    {

    /**
     * libvirt is the
     *
     * @author Timothy St. Clair
     */
    class libvirt: public hypervisor
    {
    public:
        libvirt(){;};
        virtual ~libvirt(){;};

         /**
         */
        virtual bool init(const hypv_config & local_config);

         /**
          * start() - will start a vm using the mundged input file.
          */
         virtual bool start(std::string & szConfigFile);

         /**
          */
         virtual bool suspend( bool bSoft=false );

         /**
          */
         virtual bool resume();

         /**
          */
         virtual bool checkpoint(/*name?*/);

         /**
          */
         virtual bool shutdown(bool reboot=false, bool bforce=false);

         /**
          */
         virtual bool getStats( vm_stats & stats );

         /**
         */
        virtual bool check_caps(hypv_config & local_config);

    };

    }/// end namespace vmu

}/// end namespace condor

#endif