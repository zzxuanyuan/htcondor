/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_attributes.h"

#include "jic_local_config.h"


JICLocalConfig::JICLocalConfig( const char* keyword ) : JICLocal()
{
	if( keyword ) {
		key = strdup( keyword );
	} else {
		EXCEPT( "Can't instantiate a JICLocalConfig object without "
				"a keyword" );
	}
}


JICLocalConfig::~JICLocalConfig()
{
	if( key ) {
		free( key );
	}
}


bool
JICLocalConfig::getLocalJobAd( void )
{ 
	dprintf( D_ALWAYS, "Getting job ClassAd from config file "
			 "with keyword: \"%s\"\n", key );

	job_ad = new ClassAd();

		// first, things we absolutely need

	if( ! getConfigString(job_ad, key, 1, ATTR_JOB_CMD) ) { return false; }
	if( ! getConfigString(job_ad, key, 1, ATTR_JOB_IWD) ) { return false; }
	if( ! getConfigString(job_ad, key, 1, ATTR_OWNER) ) { return false; }
	if( ! getConfigInt(job_ad, key, 1, ATTR_JOB_UNIVERSE) ) { return false; }
	if( ! getConfigInt(job_ad, key, 1, ATTR_CLUSTER_ID) ) { return false; }
	if( ! getConfigInt(job_ad, key, 1, ATTR_PROC_ID) ) { return false; }

		// now, optional things

	getConfigString( job_ad, key, 0, ATTR_JOB_INPUT );
	getConfigString( job_ad, key, 0, ATTR_JOB_OUTPUT );
	getConfigString( job_ad, key, 0, ATTR_JOB_ERROR );
	getConfigString( job_ad, key, 0, ATTR_JOB_ARGUMENTS );
	getConfigString( job_ad, key, 0, ATTR_JOB_ENVIRONMENT );
	getConfigString( job_ad, key, 0, ATTR_JAR_FILES );
	getConfigInt( job_ad, key, 0, ATTR_KILL_SIG );
	getConfigBool( job_ad, key, 0, ATTR_STARTER_WAIT_FOR_DEBUG );
	getConfigString( job_ad, key, 0, ATTR_STARTER_ULOG_FILE );
	getConfigBool( job_ad, key, 0, ATTR_STARTER_ULOG_USE_XML );

	return true;
}


bool
JICLocalConfig::getConfigString( ClassAd* ad, const char* key, 
								 bool warn, const char* attr )
{
	return getConfigAttr( ad, key, warn, attr, true );
}


bool
JICLocalConfig::getConfigInt( ClassAd* ad, const char* key, 
							  bool warn, const char* attr )
{
	return getConfigAttr( ad, key, warn, attr, false );
}


bool
JICLocalConfig::getConfigBool( ClassAd* ad, const char* key, 
							   bool warn, const char* attr )
{
	return getConfigAttr( ad, key, warn, attr, false );
}


bool
JICLocalConfig::getConfigAttr( ClassAd* ad, const char* key, bool warn, 
							   const char* attr, bool is_string )
{
	char* tmp;
	char param_name[256];
	MyString expr;
	sprintf( param_name, "%s_%s", key, attr );
	bool needs_quotes = false;
	
	tmp = param( param_name );
	if( ! tmp ) {
		if( warn ) {
			dprintf( D_ALWAYS, "\"%s\" not found in config file\n",
					 param_name );
		}
		return false;
	}

	if( is_string && tmp[0] != '"' ) {
		needs_quotes = true;
	}

	expr = attr;
	expr += " = ";
	if( needs_quotes ) {
		expr += "\"";
	}
	expr += tmp;
	if( needs_quotes ) {
		expr += "\"";
	}
	free( tmp );

	if( ad->Insert(expr.Value()) ) {
		return true;
	}
	dprintf( D_ALWAYS, "ERROR: Failed to insert into job ad: %s\n",
			 expr.Value() );
	return false;
}


