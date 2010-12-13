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

#include "vmgahp_controller.h"
#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>

namespace po = boost::program_options;
using namespace condor::vmu;
using namespace std;

/// it's a ptr to allow for possible virtualization, and ordered shutdown
const static boost::shared_ptr<vmgahp_controller> _p_vmcontroller (new vmgahp_controller());

/// common exit pattern
void vm_exit(const char * pszReason, unsigned int iCode)
{
    // why is this special?
    vmprintf(D_ALWAYS, "** EXITING (%d): %s",iCode, pszReason );


    _p_vmcontroller.reset();
    DC_Exit(iCode);
}

/// parse the input options used for initialization
unsigned int parse_input_options(const po::variables_map & in_opts /*, */)
{
    // begin the input option parsing.
    unsigned int iRet=0;

    return (iRet);
}

/**
 * The following are the main entry points which are called from
 * daemon-core.
 */

/// called on config && reconfig.
void main_config()
{
    unsigned int iRet=0;
    if ( 0!= (iRet = _p_vmcontroller->config() ) )
    {
        vm_exit( "Failed to configure", 1);
    }
}

///
void main_shutdown_fast()
{ vm_exit( "Received Signal for shutdown fast", 0); }

///
void main_shutdown_graceful()
{ vm_exit( "Received Signal for shutdown gracefully", 0); }

///
void main_pre_dc_init(int, char*[])
{ ; }

/// open question
void main_init(int argc, char *argv[])
{
    po::options_description opts( "vmgahp2 options" );
    po::variables_map in_opts;
    unsigned int iRet=0;
    // some struct

    opts.add_options()
    ("fuzthewha,f", po::value<bool>(), "??" )
    ("t_fuzthewha,t", po::value<bool>(), "??" )
    ("mode,M",  po::value< std::string >()->default_value(0), "vmgahp mode, ITERATE MODES HERE");
    ("help,h", "out help message");

    po::store( po::parse_command_line( argc, argv, opts ), in_opts );

    // check the input options
    if ( vm.count("help") ||  0 != (iRet = parse_input_options(in_opts/*, */) )
    {
        cout<<opts<<endl; // output help string
        vm_exit ("Exiting from input option parsing", iRet);
    }
    else
    {
        // initialize the controller from the input options
        if ( 0 != ( iRet = _p_vmcontroller->init(/**/) ) )
        {

        }
    }

}

///
void main_pre_command_sock_init()
{ daemonCore->WantSendChildAlive( false ); }