/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "boinc_mgr.h"



BOINC_BackfillVM::BOINC_BackfillVM( int vm_id )
	    : BackfillVM( vm_id )
{
		// TODO
}


BOINC_BackfillVM::~BOINC_BackfillVM()
{
		// TODO
}


bool
BOINC_BackfillVM::init()
{
		// TODO
	return true;
}


bool
BOINC_BackfillVM::start()
{
		// TODO
	return true;
}


bool
BOINC_BackfillVM::suspend()
{
		// TODO
	return true;
}


bool
BOINC_BackfillVM::resume()
{
		// TODO
	return true;
}


bool
BOINC_BackfillVM::softkill()
{
		// TODO
	return true;
}


bool
BOINC_BackfillVM::hardkill()
{
		// TODO
	return true;
}


BOINC_BackfillMgr::BOINC_BackfillMgr()
	: BackfillMgr()
{
	dprintf( D_ALWAYS, "Instantiating a BOINC_BackfillMgr\n" );
	m_boinc_dir = NULL;
	m_boinc_client_exe = NULL;
}


BOINC_BackfillMgr::~BOINC_BackfillMgr()
{
	dprintf( D_FULLDEBUG, "Destroying a BOINC_BackfillMgr\n" );
	if( m_boinc_dir ) {
		free( m_boinc_dir );
	}
	if( m_boinc_client_exe ) {
		free( m_boinc_client_exe );
	}

}


static bool
param_boinc( const char* attr_name, char** str_p )
{
	if( ! str_p ) {
		EXCEPT( "param_boinc() called with NULL string" );
	}
	if( ! attr_name ) {
		EXCEPT( "param_boinc() called with NULL attr_name" );
	}

	if( *str_p != NULL ) {
		free( *str_p );
	}
	*str_p = param( attr_name );
	if( ! *str_p ) {
		dprintf( D_ALWAYS, "Trying to initialize a BOINC backfill manager, "
				 "but %s not defined, failing\n", attr_name );
		return false;
	}
	return true;
}


bool
BOINC_BackfillMgr::init()
{
	return reconfig();
}


bool
BOINC_BackfillMgr::reconfig()
{
		// TODO be smart about if anything changes...

	if( ! param_boinc("BOINC_HOMEDIR", &m_boinc_dir) ) {
		return false;
	}
	if( ! param_boinc("BOINC_CLIENT_EXE", &m_boinc_client_exe) ) {
		return false;
	}
	
	dprintf( D_FULLDEBUG, "BOINC homedir: '%s'\n", m_boinc_dir );
	dprintf( D_FULLDEBUG, "BOINC client: '%s'\n", m_boinc_client_exe );
	return true;
}


bool
BOINC_BackfillMgr::addVM( BOINC_BackfillVM* boinc_vm )
{
		// TODO
	return true;
}


bool
BOINC_BackfillMgr::rmVM( int vm_id )
{
		// TODO
	return true;
}


bool
BOINC_BackfillMgr::start( int vm_id )
{
		// TODO
	return true;
}


bool
BOINC_BackfillMgr::suspend( int vm_id )
{
		// TODO
	return true;
}


bool
BOINC_BackfillMgr::resume( int vm_id )
{
		// TODO
	return true;
}


bool
BOINC_BackfillMgr::softkill( int vm_id )
{
		// TODO
	return true;
}


bool
BOINC_BackfillMgr::hardkill( int vm_id )
{
		// TODO
	return true;
}


