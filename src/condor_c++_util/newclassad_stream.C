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
#include "condor_debug.h"
#include "newclassad_stream.h"

int
StreamPut( Stream *stream, const classad::ClassAd &ad )
{
	classad::ClassAdUnParser	unparser;
	string						str;
	unparser.Unparse( str, &ad );
	return stream->put( (char*)str.c_str() );
}

int
StreamPut( Stream *stream, list<const classad::ClassAd *> &ad_list )
{
	if ( !stream->put( (int) ad_list.size() ) ) {
		return 0;
	}
	list< const classad::ClassAd *>::iterator iter;
	for ( iter = ad_list.begin();  iter != ad_list.end();  iter++ )
	{
		const	classad::ClassAd	*ad = *iter;
		if ( !StreamPut( stream, *ad ) ) {
			return 0;
		}
	}
	return 1;
}

int StreamGet( Stream *stream, list<classad::ClassAd *> &ad_list )
{
	int		num_ads;
	if ( !stream->get( num_ads ) ) {
		return 0;
	}
	if ( num_ads < 0 ) {
		return 0;
	}
	for( int ad_num = 0;  ad_num < num_ads;  ad_num++ ) {
		classad::ClassAd	*ad = new classad::ClassAd;
		if ( !StreamGet( stream, *ad ) ) {
			return 0;
		}
		ad_list.push_back( ad );
	}
	return num_ads;
}

int StreamGet( Stream *stream, classad::ClassAd &ad )
{
	char		*cstr = NULL;
	if ( !stream->get( cstr ) ) {
		dprintf( D_FULLDEBUG, "get( %p ) failed\n", cstr );
		return 0;
	}
	classad::ClassAdParser	parser;
	if ( ! parser.ParseClassAd( cstr, ad, true ) ) {
		free( cstr );
		return 0;
	}
	free( cstr );
	return 1;
}
