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

#include "condor_common.h"
#include "stl_string_utils.h"
#include "vmgahp_common.h"

#include "condor_daemon_core.h"

using namespace std;
using namespace condor::vmu;

namespace condor {
    namespace vmu {

        void vmprintf( int flags, const char *fmt, ... )
        {
            static pid_t mypid = 0;

            if( !mypid ) {
                mypid = daemonCore->getpid();
            }

                if( fmt )
                {
                    std::string szString;
                    va_list args;
                    va_start(args, fmt);
                    sprintf(szString, fmt, args);
                    va_end(args);

                    dprintf(flags, "VMGAHP[%d]: %s\n", (int)mypid, szString.c_str());
                }


        }


    }

}
