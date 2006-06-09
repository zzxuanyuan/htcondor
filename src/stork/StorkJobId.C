/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#include "StorkJobId.h"
#include "stork_job_ad.h"
#include "std_string_utils.h"

using std::string;

#define KEYFORMAT			"job (%d)"
#define RAWFORMAT			"%d"

// Default constructor.
StorkJobId::StorkJobId( void)
{
	initialize();
}

// destructor
StorkJobId::~StorkJobId( void)
{
	return;
}

// Initialize StorkJobId
void
StorkJobId::initialize( void )
{
	_id = 0;
	_error = true;
	_errorMsg.clear();
	_key.clear();
	_raw.clear();

	return;
}

void
StorkJobId::assign( const std::string& key )
{
	initialize();
	int converted = sscanf(key.c_str(), KEYFORMAT, &this->_id) ;
	if (converted != 1) {
		string_printf(_errorMsg, "error reading jobId from key \"%s\"",
				key.c_str() );
	} else {
		_error = false;
	}
}

StorkJobId::StorkJobId( const std::string& key )
{
	initialize();
	assign( key );
}

StorkJobId&
StorkJobId::operator=(const std::string& key)
{
	assign(key);
	return *this;
}

void
StorkJobId::assign( const classad::ClassAd& ad)
{
	initialize();
	if ( ! ad.EvaluateAttrInt(STORK_JOB_ATTR_ID, this->_id) ) {
		string_printf( _errorMsg, "error reading %s attribute from job ad",
				STORK_JOB_ATTR_ID );
	} else {
		_error = false;
	}
}

StorkJobId::StorkJobId( const classad::ClassAd& ad)
{
	initialize();
	assign( ad );
}

StorkJobId&
StorkJobId::operator=(const classad::ClassAd& ad)
{
	assign( ad );
	return *this;
}

void
StorkJobId::updateClassAd( classad::ClassAd& ad ) const
{
	ad.InsertAttr( STORK_JOB_ATTR_ID, _id );
}

std::string
StorkJobId::key( void )
{
	string_printf( _key, KEYFORMAT, _id );
	return _key;
}

const char*
StorkJobId::fmt( void )
{
	key();
	return _key.c_str();
}

const char*
StorkJobId::c_str( void )
{
	string_printf( _raw, RAWFORMAT, _id );
	return _raw.c_str();
}

