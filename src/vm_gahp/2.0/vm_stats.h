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

#ifndef VMGAHP_STATS
#define VMGAHP_STATS

#include "classad/classad.h"
#include <boost/shared_ptr.hpp>
#include <vector>
#include <string>

namespace condor
{

    namespace vmu
    {

    class hypv_config;

    /// The state of the current running vm
    typedef enum
    {
        VM_INACTIVE=0,
        VM_RUNNING,
        VM_STOPPED,
        VM_SUSPENDED
        // may be more...
    }vm_state;

    /// vm_stats are all the current known information about a vm for
    class vm_stats
    {
    public:
        //enum type eVmType;
        vm_state        m_eState; ///<
        unsigned int    m_iPid;   ///< pid of currently running vm 0
        boost::shared_ptr<hypv_config>     m_pconfig; ///<

        //virtual bool InsertAddAttr(ClassAd & ad);

        // insert all the vm stats here.
        //// running stats
        //// list of checkpoints, or just last checkpoint?
    };


    } // namespace vmu

}// namespace condor

#endif
