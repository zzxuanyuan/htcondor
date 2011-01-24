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
#ifndef __GAHP_REQUEST__
#define __GAHP_REQUEST__

#include <boost/shared_ptr.hpp>
#include "IVmGahp/types.h"
#include "IVmGahp/errors.h"
#include "gahp_common.h"


 /* Operating gahp protocol */
#define VMGAHP_COMMAND_ASYNC_MODE_ON    "ASYNC_MODE_ON"
#define VMGAHP_COMMAND_ASYNC_MODE_OFF   "ASYNC_MODE_OFF"
#define VMGAHP_COMMAND_VERSION          "VERSION"
#define VMGAHP_COMMAND_COMMANDS         "COMMANDS"
#define VMGAHP_COMMAND_QUIT             "QUIT"
#define VMGAHP_COMMAND_RESULTS          "RESULTS"

#define VMGAHP_COMMAND_SUPPORT_VMS      "SUPPORT_VMS"
#define VMGAHP_COMMAND_CLASSAD          "CLASSAD"
#define VMGAHP_COMMAND_CLASSAD_END      "CLASSAD_END"
#define VMGAHP_COMMAND_VM_START         "CONDOR_VM_START"
#define VMGAHP_COMMAND_VM_STOP          "CONDOR_VM_STOP"
#define VMGAHP_COMMAND_VM_SUSPEND       "CONDOR_VM_SUSPEND"
#define VMGAHP_COMMAND_VM_SOFT_SUSPEND  "CONDOR_VM_SOFT_SUSPEND"
#define VMGAHP_COMMAND_VM_RESUME        "CONDOR_VM_RESUME"
#define VMGAHP_COMMAND_VM_CHECKPOINT    "CONDOR_VM_CHECKPOINT"
#define VMGAHP_COMMAND_VM_STATUS        "CONDOR_VM_STATUS"
#define VMGAHP_COMMAND_VM_GETPID        "CONDOR_VM_GETPID"

#define VMGAHP_RESULT_SUCCESS       "S"
#define VMGAHP_RESULT_ERROR         "E"
#define VMGAHP_RESULT_FAILURE       "F"

#define VMGAHP_TERMINATOR           "\r\n"


namespace condor
{
    /**
     *
     */
    class GahpRequest
    {
    public:
        GahpRequest (char * pszCommand);
        GahpRequest();
        int m_reqid;
        bool m_has_result;
        bool m_is_success;

        std::string m_raw_cmd;
        Gahp_Args m_args;
        std::string m_result;

        virtual bool execute()=0;
        virtual bool serialize(std::string & szBuff)=0;
    };

    /**
     *
     */
    class GahpFactory
    {
       friend class GahpRequest;
    public:
        static boost::shared_ptr<GahpRequest> manufacture (const char * pszCommand);
    };


}

#endif