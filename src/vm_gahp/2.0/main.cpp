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
#include "subsystem_info.h"
#include "condor_daemon_core.h"

#include "vmgahp_common.h"
#include "vmgahp_controller.h"

#include <stdlib.h>
#include <iostream>
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

/// subsystem declaration used by daemoncore
DECL_SUBSYSTEM( "VM_GAHP", SUBSYSTEM_TYPE_GAHP );

/// namespace decls
namespace po = boost::program_options;
using namespace condor::vmu;
using namespace std;

/// it's a ptr to allow for possible virtualization, and ordered shutdown w/daemoncore
static boost::shared_ptr<vmgahp_controller> _p_vmcontroller (new vmgahp_controller());

/// common exit pattern
void vm_exit(const char * pszReason, unsigned int iCode)
{
    vmprintf(D_ALWAYS, "** EXITING (%d): %s", iCode, pszReason );
    _p_vmcontroller.reset();
    DC_Exit(iCode);
}

////////////////////////////////////////////////////////////////////////////////
/*The following are the main entry points which are called from  daemon-core. */
////////////////////////////////////////////////////////////////////////////////

/// called on config && reconfig.
void main_config()
{
    unsigned int iRet=0;
    if ( 0!= (iRet = _p_vmcontroller->config() ) )
    {
        vm_exit( "Failed to configure", 1);
    }
}

/// fast shutdown from daemon core
void main_shutdown_fast()
{ vm_exit( "Received Signal for shutdown fast", 0); }

/// graceful shutdown from daemon core
void main_shutdown_graceful()
{
    // shutdown and cleanup everything gracefully.
    vm_exit( "Received Signal for shutdown gracefully", _p_vmcontroller->fini() );
}

/// pre dc init from daemon core
void main_pre_dc_init(int, char*[])
{ ; }

/// main initialization.
void main_init(int argc, char *argv[])
{
    po::options_description opts( "vmgahp2 options" );
    po::variables_map in_opts;

    // default input options.
    opts.add_options()
    //("foreground,f", "Causes the daemon to start up in the foreground, instead of forking" )
    //("termlog,t", "Causes the daemon to print out its error message to stderr instead of its specified log file" ) // disabled VM_GAHP_LOG required!
    ("mode,M",  po::value< int >()->default_value(VMGAHP_TEST_MODE), "vmgahp mode: VMGAHP_TEST_MODE(0), VMGAHP_STANDALONE_MODE(1), VMGAHP_KILL_MODE(2)")
    ("vmtypes,y", po::value< vector< string > >()->multitoken(), "Different types, VMGAHP_TEST_MODE can scan for multiple types")
    ("matches,m", po::value< string >(), "KILL_MODE match string")
    ("help,h", "output help message");

    // parse the command line.
    po::store( po::parse_command_line( argc, argv, opts ), in_opts );

    // check the input options
    if ( in_opts.count("help") )
    {
        cout << opts << endl; // output help string
        vm_exit( "Exiting from input option parsing", 0 );
    }
    else
    {
        // one shot initializer for all things daemoncore
        static int iRet=_p_vmcontroller->init();

        if (0 == iRet)
        {
            vector<string> vmTypes;
            string szMatch;

            // obtain the input options.
            if ( in_opts.count( "vmtypes" ) )
                vmTypes = in_opts["vmtypes"].as< vector< string > >();

            if ( in_opts.count( "matches" ) )
                szMatch = in_opts["matches"].as< string >();

            // switch on the mode
            switch(in_opts["mode"].as<int>())
            {
                // discover hypervisor & config props
                case VMGAHP_TEST_MODE:
                    iRet = _p_vmcontroller->discover(vmTypes);
                    vm_exit( "end of VMGAHP_TEST_MODE", iRet );
                    break;
                // start up a vm
                case VMGAHP_STANDALONE_MODE:
                    if ((iRet = _p_vmcontroller->open_gahp( getenv("VMGAHP_VMTYPE"), getenv("VMGAHP_WORKING_DIR") )))
                        vm_exit( "failed to start VM_GAHP", iRet );
                    break;
                // kill the current running vm which matches
                case VMGAHP_KILL_MODE:
                    // how does it know what PID?
                    // type alone doesn't mean anything
                    //iRet = _p_vmcontroller->fini();
                    vm_exit( "end of VMGAHP_KILL_MODE", iRet );
                    break;
                default:
                    vm_exit( "Unknown mode", 1 );
            }

        }
        else
        {
            vm_exit( "Failed controller initialization", iRet );
        }
    }

}

/// main
void main_pre_command_sock_init()
{ daemonCore->WantSendChildAlive( false ); }