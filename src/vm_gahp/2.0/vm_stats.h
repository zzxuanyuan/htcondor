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

// I need to rid myself of this dumb data structure.
#include "string_list.h"

#include <string>

namespace condor
{

    namespace vmu
    {

     /**
     * The following ...
     */
    typedef struct config
    {
        unsigned int    m_VM_MEMROY;
        bool            m_VM_NETWORKING;
        std::string     m_VM_NETWORKING_DEFAULT_TYPE;
        StringList      m_VM_NETWORKING_TYPE;

        // The following are the other params

    }hypv_config;

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
    typedef struct stats
    {
        //enum type eVmType;
        vm_state        m_eState; ///<
        unsigned int    m_iPid;   ///< pid of currently running vm 0

        hypv_config     m_config; ///<

        // insert all the vm stats here.
        //// running stats
        //// list of checkpoints, or just last checkpoint?
    }vm_stats;


    } // namespace vmu

}// namespace condor

#endif
