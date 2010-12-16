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
#include "vmgahp_controller.h"
#include "condor_daemon_core.h"
#include "vmgahp_common.h"

using namespace std;
using namespace condor::vmu;

const char *vmgahp_version = "$VMGahpVersion 2.0 Jan 2011 Condor\\ VMGAHP $";

vmgahp_controller::vmgahp_controller()
{
    m_stdout_pipe=0;
}

vmgahp_controller::~vmgahp_controller()
{
    // nothing to do auto-cleanup
}

int vmgahp_controller::discover( const std::vector< std::string >& vTypes  )
{
    int iRet;

    if ( !( iRet = this->init() ) )
    {
        //iRet = hypervisor::discover

        /*
         * write_to_daemoncore_pipe("VM_GAHP_VERSION = \"%s\"\n", CONDOR_VMGAHP_VERSION);
        write_to_daemoncore_pipe("%s = \"%s\"\n", ATTR_VM_TYPE,
                gahpconfig->m_vm_type.Value());
        write_to_daemoncore_pipe("%s = %d\n", ATTR_VM_MEMORY,
                gahpconfig->m_vm_max_memory);
        write_to_daemoncore_pipe("%s = %s\n", ATTR_VM_NETWORKING,
                gahpconfig->m_vm_networking? "TRUE":"FALSE");
        if( gahpconfig->m_vm_networking ) {
            write_to_daemoncore_pipe("%s = \"%s\"\n", ATTR_VM_NETWORKING_TYPES,
                gahpconfig->m_vm_networking_types.print_to_string());
        }
        if( gahpconfig->m_vm_hardware_vt ) {
            write_to_daemoncore_pipe("%s = TRUE\n", ATTR_VM_HARDWARE_VT);
        }
         */

        //daemonCore->Write_Pipe( m_stdout_pipe, str, len);
    }
    else
        vmprintf(D_ALWAYS, "Failed to initialize" );

    return iRet;
}

int vmgahp_controller::init( )
{
    int iRet = 1;

    if (0 == init_uids() )
    {
        m_stdout_pipe = daemonCore->Inherit_Pipe(fileno(stdout),
                        true,       /*write pipe*/
                        false,      //nonregisterable
                        false);     //blocking

        if (!m_stdout_pipe)
        {
            vmprintf(D_ALWAYS, "Failed to daemonCore->Inherit_Pipe(stdout)" );
        }
        else
        {
            iRet = 0; // successful path
        }
    }
    else
    {
        vmprintf(D_ALWAYS, "Failed to initialize" );
    }

    return iRet;
}

int vmgahp_controller::config( )
{
    int iRet=0;

    // grabs the input options.

    return iRet;
}

int vmgahp_controller::init_uids()
{
    int iRet =0;

    #if 0 //!defined(WIN32)
        bool SwitchUid = can_switch_ids() || privsep_enabled();

        caller_uid = getuid();
        caller_gid = getgid();

        // Set user uid/gid
        string user_uid;
        string user_gid;
        user_uid = getenv("VMGAHP_USER_UID");
        if( user_uid.IsEmpty() == false )
        {
            int env_uid = (int)strtol(user_uid.Value(), (char **)NULL, 10);
            if( env_uid > 0 )
            {
                job_user_uid = env_uid;

                // Try to read user_gid
                user_gid = getenv("VMGAHP_USER_GID");
                if( user_gid.IsEmpty() == false )
                {
                    int env_gid = (int)strtol(user_gid.Value(), (char **)NULL, 10);
                    if( env_gid > 0 )
                    {
                        job_user_gid = env_gid;
                    }
                }
                if( job_user_gid == ROOT_UID )
                {
                    job_user_gid = job_user_uid;
                }
            }
        }

        if( !SwitchUid )
        {
            // We cannot switch uids
            // a job user uid is set to caller uid
            job_user_uid = caller_uid;
            job_user_gid = caller_gid;
        }
        else
        {
            // We can switch uids
            if( job_user_uid == ROOT_UID )
            {
                // a job user uid is not set yet.
                if( caller_uid != ROOT_UID )
                {
                    job_user_uid = caller_uid;
                    if( caller_gid != ROOT_UID )
                    {
                        job_user_gid = caller_gid;
                    }
                    else
                    {
                        job_user_gid = caller_uid;
                    }
                }
                else
                {
                    fprintf(stderr, "\nERROR: Please set environment variable "
                                    "'VMGAHP_USER_UID=<uid>'\n");
                                    exit(1);
                }
            }
        }

        // find the user name calling this program
        char *user_name = NULL;
        passwd_cache* p_cache = pcache();
        if( p_cache->get_user_name(caller_uid, user_name) == true )
        {
            caller_name = user_name;
            free(user_name);
        }

        if( job_user_uid == caller_uid )
        {
            job_user_name = caller_name;
        }

        if( SwitchUid )
        {
            set_user_ids(job_user_uid, job_user_gid);
            set_user_priv();

            // Try to get the name of a job user
            // If failed, it is harmless.
            if( job_user_uid != caller_uid )
            {
                if( p_cache->get_user_name(job_user_uid, user_name) == true )
                {
                    job_user_name = user_name;
                    free(user_name);
                }
            }
        }

    #endif

    return iRet;
}