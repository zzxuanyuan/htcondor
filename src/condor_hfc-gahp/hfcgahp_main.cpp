/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
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
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "condor_string.h"
#include "condor_attributes.h"
#include "condor_uid.h"
#include "subsystem_info.h"

#include "hfcgahp_io.h"
#include "hfcgahp_command.h"
#include "hfcgahp_user_commands.h"

DECL_SUBSYSTEM( "HFC_GAHP", SUBSYSTEM_TYPE_GAHP );

int main_init(int , char ** )
{
	dprintf(D_ALWAYS, "The HFC gahp server is starting!!!\n");
	
	int returnValue;

	if(!initCommandSystem())
	{
		dprintf(D_ALWAYS, "Command system init failed\n");
		DC_Exit(-1);		
	}
	
	if((returnValue = registerPipes()) != 0)
	{
		dprintf(D_ALWAYS, "Failed to register pipes [%d]\n", returnValue);
		DC_Exit(-1);
	}

	// call the user init function for user stuff
	if(!userInit())
	{
		dprintf(D_ALWAYS, "User init function failed\n");
		DC_Exit(-1);
	}

	// write the banner
	std::string version;
	getVersionString(version);
	so_printf("%s\n", version.c_str());

	return TRUE;
}

int main_config(bool)
{
	return 0;
}

int main_shutdown_fast()
{
	return 0;
}

int main_shutdown_graceful()
{
	return 0;
}

void main_pre_dc_init(int, char **)
{

	
}

void main_pre_command_sock_init()
{
	// taken from the VM gahp
	daemonCore->WantSendChildAlive(false);
}
