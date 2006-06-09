/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
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

#include "UrlParser.h"
using std::string;

#define NOT_FOUND	std::string::npos

// Default constructor.
UrlParser::UrlParser( void)
{
	initialize();
}

// Construct a UrlParser from a string.
UrlParser::UrlParser( const string& url )
{
	initialize();
	assign(url);
}

// destructor
UrlParser::~UrlParser( void)
{
	return;
}

// Initialize UrlParser
void
UrlParser::initialize( void )
{
	_url.clear();
	_scheme.clear();
	_user.clear();
	_password.clear();
	_host.clear();
	_port.clear();
	_directory.clear();
	_file.clear();
	_path.clear();
	_parsed = false;
	_error = true;
	_errorMsg = "unparsed";

	return;
}

void
UrlParser::assign( const string& url )
{
	initialize();
	_url = url;
}

// Assignment from a string
UrlParser&
UrlParser::operator=(const std::string& url)
{
	assign(url);
	return *this;
}

bool
UrlParser::parse( void )
{
	// field delimiters
	//std::string::size_type start = 0;
	std::string::size_type end = 0;
	std::string::size_type endpw, endusr;
	std::string::size_type endhost, endport;
	const char* delim;

	if ( _parsed ) goto EXIT;
	if ( _url.empty() ) {
		_errorMsg = "empty URL";
		goto EXIT;
	}

	// protocol (or scheme)
	delim = "://";
	end = _url.find(delim);
	if (end == NOT_FOUND) {
	// special case: handle deprecated file:/ protocol
		if (! _url.compare(0, strlen("file:/"), "file:/") ) {
			_scheme = "file";
			_url.erase(0, strlen("file:") );	// leave initial '/'
		} else {
			_errorMsg = "protocol not found";
			goto EXIT;
		}
	} else {
		_scheme = _url.substr(0, end);
		_url.erase(0, _scheme.length() + strlen(delim) );
	}

	// user name
	endpw = _url.find("@");
	if (endpw != NOT_FOUND) {
		endusr = _url.find(":");
		if (endusr != NOT_FOUND && endusr < endpw) {
			// password and user found
			_user = _url.substr(0, endusr);
			_password = _url.substr(endusr+1, endpw-endusr-1);
		} else {
			// only user found
			_user = _url.substr(0, endpw);
		}
		_url.erase(0, endpw+1);
	}

	// host, host
	endhost = _url.find("/");
	if (endhost != NOT_FOUND) {
		endport = _url.find(":");
		if (endport != NOT_FOUND) {
			_port = _url.substr(endport+1, endhost-endport-1);
			_host = _url.substr(0, endport);
		} else {
			_host = _url.substr(0, endhost);
		}
		//_url.erase(0, endhost+1);
		_url.erase(0, endhost);
	} else if (_scheme != "file") {
		_errorMsg = "host/port not found";
		goto EXIT;
	}

	// directory
	end = _url.rfind("/");
	if (end != NOT_FOUND) {
		_directory = _url.substr(0, end);
		_url.erase(0, end+1);
	} else if (_scheme != "http") {
		_errorMsg = "path not found";
		goto EXIT;
	}
	_file = _url;


	// Successfully parsed URL.
	_parsed = true;
	_error = false;
	_errorMsg.clear();

EXIT:
	return _parsed;
}

