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

DECL_SUBSYSTEM( "VM_GAHP", SUBSYSTEM_TYPE_GAHP );

int main_init(int argc, char ** argv)
{
	dprintf(D_ALWAYS, "The HFC gahp server is starting!!!\n");	
	DC_Exit(0);
}

int main_config(bool is_full)
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

void main_pre_dc_init(int argc, char ** argv)
{

}

void main_pre_command_sock_init()
{

}
