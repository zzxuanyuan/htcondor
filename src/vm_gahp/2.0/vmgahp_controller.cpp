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

#include "condor_daemon_core.h"
#include "condor_uid.h"
#include "condor_privsep/condor_privsep.h"

#include "hypervisor_factory.h"
#include "vmgahp_controller.h"
#include "vmgahp_common.h"
#include <boost/foreach.hpp>

#define ROOT_UID    0

using namespace std;
using namespace condor::vmu;

const char *vmgahp_version = "$VMGahpVersion 2.0 Jan 2011 Condor\\ VMGAHP $";

///////////////////////////////////////////////////
///////////////////////////////////////////////////

vmgahp_controller::vmgahp_controller()
{
    m_stdout_pipe=0;
    m_stdin_pipe=0;
    m_bAsync_mode=false;
}

///////////////////////////////////////////////////
///////////////////////////////////////////////////

vmgahp_controller::~vmgahp_controller()
{
    // nothing to do auto-cleanup
}

///////////////////////////////////////////////////
///////////////////////////////////////////////////

int vmgahp_controller::discover( const vector< string >& vTypes  )
{
    int iRet=0;
    ClassAd ad;
    boost::shared_ptr<hypv_config> pTempConfig;

    ad.Clear();

    priv_state priv = set_root_priv();
    BOOST_FOREACH( string szType, vTypes )
    {
        vmprintf(D_ALWAYS, "discover called for type: %s", szType.c_str());
        if ( hypervisor_factory::discover(szType, pTempConfig) )
        {
            pTempConfig->InsertAddAttr( ad );
            vmprintf(D_ALWAYS, "Discovered %s", szType.c_str() );
        }
        else
        {
            vmprintf(D_ALWAYS, "ERROR: Discovery failed for type: %s", szType.c_str() );
        }
    }
    set_priv(priv);

    if ( ad.size() )
    {
        classad::PrettyPrint pp;
        string szCaps;

        pp.Unparse(szCaps, &ad);

        // now send the classad to the parent process which spawned this.
        vmprintf( D_ALWAYS, "Discovered:\n%s", szCaps.c_str() );
        daemonCore->Write_Pipe( m_stdout_pipe, szCaps.c_str(), szCaps.length() );
    }
    else
    {
        vmprintf( D_ALWAYS, "ERROR: Empty Discovery classad" );
        iRet = 1;
    }

    return iRet;
}

///////////////////////////////////////////////////
///////////////////////////////////////////////////

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

///////////////////////////////////////////////////
///////////////////////////////////////////////////

int vmgahp_controller::config( )
{
    bool bRet = true;

    if (m_hyp_config_params)
        bRet = m_hyp_config_params->read_config();

    if (m_hypervisor)
    {
        priv_state priv = set_root_priv();

        bRet = m_hypervisor->check_caps(m_hyp_config_params);

        if (bRet)
            bRet = m_hypervisor->config(m_hyp_config_params);

        set_priv(priv);
    }

    return ( (int) !bRet );
}

///////////////////////////////////////////////////
///////////////////////////////////////////////////

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

///////////////////////////////////////////////////
///////////////////////////////////////////////////

int vmgahp_controller::fini()
{
    int iRet =0;

    if (m_hypervisor)
    {
        vmprintf( D_ALWAYS, "Gracefully shutting down hypervisor");

        priv_state priv = set_root_priv();
        iRet = m_hypervisor->shutdown(false, true);
        set_priv(priv);
    }

    return (iRet);
}

///////////////////////////////////////////////////
///////////////////////////////////////////////////

int vmgahp_controller::start(  const char * pszVMType, const char * pszWorkingDir )
{
    int iRet = 1;

    // check for valid input params
    if (pszVMType && pszWorkingDir)
    {
        // attempt to manufacture based on input
        m_hypervisor = hypervisor_factory::manufacture(string(pszVMType), m_hyp_config_params);

        if (m_hypervisor)
        {
            // load the config
            if ( 0 == this->config() )
            {
                // bind command handlers


                // setup the daemon core GAHP communications via stdin
                m_stdin_pipe = daemonCore->Inherit_Pipe(fileno(stdin), false, true, false);

                if (-1 == m_stdin_pipe)
                {
                    daemonCore->Register_Pipe(m_stdin_pipe,
                                              "stdin_pipe",
                                              (PipeHandlercpp)&VMGahp::waitForCommand,
                                              "vmgahp_controller::waitForCommand",
                                              this)
                    iRet = 0;
                }
            }
            else
                vmprintf( D_ALWAYS, "ERROR: Failed to config %s (^^ CHECK LOG ^^)", pszVMType );
        }
        else
        {
            vmprintf( D_ALWAYS, "ERROR: Failed to manufacture %s (NOT REGISTERED)", pszVMType );
        }
    }
    else
    {
        vmprintf( D_ALWAYS, "ERROR: Type or Working Directory not defined" );
    }

    return (iRet);
}

///////////////////////////////////////////////////
///////////////////////////////////////////////////

int vmgahp_controller::waitForCommand()
{
    int iRead_Bytes=0;
    int iBuffBlockSize=4096;
    int iTotalBytes=0;
    int iLoopCnt=0;
    int iCommands=0;
    vector<char> vBuff;

    do
    {
        iTotalBytes+=iRead_Bytes;
        iLoopCnt++;

        // resize the buffer accordingly
        vBuff.resize(iBuffBlockSize*iLoopCnt);

        // read in from daemoncore pipe
        iRead_Bytes = daemonCore->Read_Pipe(m_stdin_pipe, (void *) &vBuff[iTotalBytes], iBuffBlockSize);

    // drain the daemoncore pipe every time this is called.
    }while (iRead_Bytes > 0);

    if (iRead_Bytes < 0)
        vmprintf(D_ALWAYS, "error reading from DaemonCore pipe %d", iRead_Bytes);

    // now our buffer is full we can parse the commands
    if (iTotalBytes)
    {
        vBuff[iTotalBytes] = '\0';

        iCommands = unmarshall( (char *) &vBuff[0] );
        vmprintf(D_FULLDEBUG, "Parsed (%d) commands", iRead_Bytes);
    }

    return 0; // default return to daemon core.
}

///////////////////////////////////////////////////
///////////////////////////////////////////////////

int vmgahp_controller::unmarshall(char * pszBuffer)
{
    int iCommmands=0;
    shared_ptr<GahpRequest> pReq;
    string szMessage;

    // each command is \r\n terminated so look for that
    char * pch = strtok (pszBuffer,"\r\n");
    while (pch != NULL)
    {
        // see if it is a valid command
        if ( ( pReq = GahpFactory::manufacure(pch) ) )
        {
            // execute the command
            if ( dispatch(pReq) )
                iCommands++;
        }
        else
        {
            // unrecognized command
            // daemonCore->Write_Pipe( m_stdout_pipe, szMessage.c_str(), szMessage.length() );
        }

        // grab the next command
        pch = strtok (NULL, "\r\n");
    }

    return (iCommmands);

}

///////////////////////////////////////////////////
///////////////////////////////////////////////////

bool vmgahp_controller::dispatch( shared_ptr<GahpRequest> & pReq )
{
    string szMessage;

    // elevate privs for execution.
    priv_state priv = set_root_priv();
    if ( pReq->execute() )
    {
        if ( m_bAsync_mode )
        {
            m_vAsyncResults.push_back(pReq);
            //szMessage = ;
        }
        else
        {
            //szMessage = ;
        }
    }
    else
    {
        // send error in execution.
        //szMessage = ;
        vmprintf( D_ALWAYS, "Failed to execute %s", pch );
    }
    set_priv(priv);

    // send the response message
    daemonCore->Write_Pipe( m_stdout_pipe, szMessage.c_str(), szMessage.length() );
}