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

#ifndef __VMGAHP_LIBVIRT_KVM__
#define __VMGAHP_LIBVIRT_KVM__

#include "libvirt.h"

namespace condor
{
    namespace vmu
    {
        /**
         *
         * @author Timothy St. Clair
         */
        class kvm : public libvirt
        {
        public:

            kvm();
            virtual ~kvm();

            static boost::shared_ptr<hypervisor> manufacture();

            //virtual bool check_caps(hypv_config & local_config);
        };

    } /// end namespace vmu

} /// end namespace condor


#endif