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
#include "condor_config.h"
#include "vm_univ_utils.h"
#include "condor_daemon_core.h"
#include "stl_string_utils.h"
#include "condor_uid.h"
#include "condor_privsep/condor_privsep.h"

#include "hypervisor_factory.h"
#include "vmgahp_controller.h"
#include "vmgahp_common.h"
#include <boost/foreach.hpp>
#include <iostream>

#define ROOT_UID    0

using namespace std;
using namespace condor::vmu;

const char *vmgahp_version = "$VMGahpVersion 2.0 Jan 2011 Condor\\ VMGAHP $";

/////////////////////////////////////////
vmgahp_controller::vmgahp_controller()
{
    m_stdout_pipe=0;
}

/////////////////////////////////////////
vmgahp_controller::~vmgahp_controller()
{
    // nothing to do auto-cleanup
}

/////////////////////////////////////////
int vmgahp_controller::discover( const vector< string >& vTypes  )
{
    int iRet=1;
    string szCaps;
    string szTypesFound;

    priv_state priv = set_root_priv();
    BOOST_FOREACH( string szType, vTypes )
    {
        vmprintf(D_ALWAYS, "discover called for type: %s", szType.c_str());


        if ( hypervisor_factory::discover(szType, m_hyp_config_params) )
        {
            vmprintf(D_ALWAYS, "Discovered %s", szType.c_str() );

            /// TODO: this may just go into a serialization f(n) of m_hyp_config_params
            if ( szTypesFound.length() )
                sprintf_cat(szTypesFound, "%s,%s", szTypesFound.c_str(), szType.c_str());
            else
                szTypesFound = szType;

            iRet = 0;
        }
        else
        {
            vmprintf(D_ALWAYS, "ERROR: Discovery failed for type: %s", szType.c_str() );
        }
    }
    set_priv(priv);

    // only if discovery
    if (0 == iRet)
    {
        ///sprintf_cat(szCaps, "%s = TRUE\n", ATTR_VM_HARDWARE_VT); ?? Cap check?
        sprintf(szCaps, "VM_GAHP_VERSION = \"2.0\"\n");
        sprintf_cat(szCaps, "%s = \"%s\"\n", ATTR_VM_TYPE, szTypesFound.c_str());
        sprintf_cat(szCaps,"%s = %d\n", ATTR_VM_MEMORY, m_hyp_config_params.m_VM_MEMROY);
        sprintf_cat(szCaps, "%s = %s\n", ATTR_VM_NETWORKING, (m_hyp_config_params.m_VM_NETWORKING)?"TRUE":"FALSE");
        if (m_hyp_config_params.m_VM_NETWORKING)
        {
            sprintf_cat(szCaps, "%s = \"%s\"\n", ATTR_VM_NETWORKING_TYPES, m_hyp_config_params.m_VM_NETWORKING_TYPE.print_to_string());
        }

        daemonCore->Write_Pipe( m_stdout_pipe, szCaps.c_str(), szCaps.length());
    }

    return iRet;
}

/////////////////////////////////////////
int vmgahp_controller::init( )
{
    int iRet = 1;

    // validate that you can switch privs.
    if ( 0 == this->init_uids() )
    {
        m_stdout_pipe = daemonCore->Inherit_Pipe(fileno(stdout),
                        true,       /*write pipe*/
                        false,      //nonregisterable
                        false);     //blocking

        if (0 == m_stdout_pipe)
        {
            vmprintf(D_ALWAYS, "Failed to daemonCore->Inherit_Pipe(stdout)" );
        }
        else
        {
            // now check all the input params
            iRet = this->config();
        }
    }
    else
    {
        vmprintf(D_ALWAYS, "Failed to initialize" );
    }

    return iRet;
}

/////////////////////////////////////////
int vmgahp_controller::config( )
{
    int iRet=0;
    char * config_value=0;

    // Read VM_MEMORY
    m_hyp_config_params.m_VM_MEMROY =  param_integer("VM_MEMORY", 0);
    if( m_hyp_config_params.m_VM_MEMROY <= 0 )
    {
        vmprintf( D_ALWAYS,"ERROR: 'VM_MEMORY' is not defined in configuration ");
        iRet=1;
    }

    // Read VM_NETWORKING
    m_hyp_config_params.m_VM_NETWORKING = param_boolean("VM_NETWORKING", false);

    // Read VM_NETWORKING_TYPE
    if( m_hyp_config_params.m_VM_NETWORKING )
    {
        config_value = param("VM_NETWORKING_TYPE");
        if( !config_value )
        {
            vmprintf( D_ALWAYS,"WARNING: 'VM_NETWORKING' is true but 'VM_NETWORKING_TYPE' is not defined, so 'VM_NETWORKING' is disabled");
            m_hyp_config_params.m_VM_NETWORKING = false;
        }
        else
        {
            MyString networking_type = delete_quotation_marks(config_value);

            networking_type.trim();
            networking_type.lower_case();

            free(config_value);

            StringList networking_types(networking_type.Value(), ", ");
            m_hyp_config_params.m_VM_NETWORKING_TYPE.create_union(networking_types, false);

            if( m_hyp_config_params.m_VM_NETWORKING_TYPE.isEmpty() )
            {
                vmprintf( D_ALWAYS,"WARNING: 'VM_NETWORKING' is true but 'VM_NETWORKING_TYPE' is empty So 'VM_NETWORKING' is disabled\n");
                m_hyp_config_params.m_VM_NETWORKING = false;
            }
            else
            {
                config_value = param("VM_NETWORKING_DEFAULT_TYPE");
                if( config_value )
                {
                    m_hyp_config_params.m_VM_NETWORKING_DEFAULT_TYPE = (delete_quotation_marks(config_value)).Value();
                    free(config_value);
                }
            }
        }
     }

    return iRet;
}

/////////////////////////////////////////
int vmgahp_controller::init_uids()
{
    int iRet =0;

    #if !defined(WIN32)

        uid_t caller_uid = ROOT_UID;
        gid_t caller_gid = ROOT_UID;
        uid_t job_user_uid = ROOT_UID;
        uid_t job_user_gid = ROOT_UID;
        string caller_name;
        string job_user_name;

        bool SwitchUid = can_switch_ids() || privsep_enabled();

        caller_uid = getuid();
        caller_gid = getgid();

        // check env options
        char *user_uid, *user_gid;
        if( ( user_uid = getenv("VMGAHP_USER_UID") ) )
        {
            int env_uid = (int)strtol(user_uid, (char **)NULL, 10);
            if( env_uid > 0 )
            {
                job_user_uid = env_uid;

                // Try to read user_gid
                if( (user_gid = getenv("VMGAHP_USER_GID")) )
                {
                    int env_gid = (int)strtol(user_gid, (char **)NULL, 10);
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

        if( SwitchUid )
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
                    vmprintf( D_ALWAYS, "ERROR: Please set environment variable 'VMGAHP_USER_UID=<uid>'");
                    iRet=1;
                }
            }
        }
        else
        {
            vmprintf( D_ALWAYS, "ERROR: Can not can_switch_ids");
            iRet = 1;
        }

        if (!iRet)
        {
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
        }

    #endif

    return iRet;
}

int vmgahp_controller::fini()
{
    int iRet =0;

    if (m_hypervisor)
    {
        vmprintf( D_ALWAYS, "Gracefully shutting down hypervisor");
        iRet = m_hypervisor->shutdown(false, true);
    }

    return (iRet);
}

int vmgahp_controller::spawn( const std::string & szVMType, const std::string & szWorkingDir )
{
    int iRet = 1;
    m_hypervisor = hypervisor_factory::manufacture(szVMType);

    if (m_hypervisor)
    {
        if ( m_hypervisor->init( m_hyp_config_params ) )
        {
            /// TODO: call start?  with what?
            iRet = 0;
        }
        else
            vmprintf( D_ALWAYS, "ERROR: Failed to init %s (^^ CHECK LOG ^^)", szVMType.c_str() );
    }
    else
    {
        vmprintf( D_ALWAYS, "ERROR: Failed to manufacture %s (NOT REGISTERED)", szVMType.c_str() );
    }

    return (iRet);
}
