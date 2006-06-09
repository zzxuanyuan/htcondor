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
#ifndef __URL_PARSER_H__
#define __URL_PARSER_H__

/* Class to parse URLs, per RFC 1738.  See the Globus GT4 globus-url-copy
 * manual for a syntax reference,
 * http://www.globus.org/toolkit/docs/4.0/data/gridftp/rn01re01.html
 * Generally, the URL syntax must be of the form  
 *   protocol://[host]:[port]/path
 * If the path ends with a trailing / (i.e. /path/to/directory/) it will be
 * considered to be a directory.  The file protocol is handled specially,
 * without specifying a host or port:
 *   file:///foo.dat
 * The following file protocol form is valid, but deprecated, and may not be
 * handled by other client software:
 *   file:/foo.dat
 * As some protocols (ftp, gsiftp, srb) allow for specification of the username
 * and password in the URL, these are supported for every protocol.
 *   protocol://myname:[mypassword]@myhost.mydomain.com/foo.dat  // INSECURE!
 * However, this format SPECIFIES THE PASSWORD IN THE CLEAR and is not
 * recommended.
 */

// Includes
#include "condor_common.h"

// Macros

// Typedefs

/// UrlParser class
class UrlParser
{
	public:

	/// Constructors
	UrlParser( void );
	UrlParser( const std::string& url );

	/// Destructor
	~UrlParser( void );

	/// Assignment from a string.
	void assign( const std::string& url);

	/// Parse URL
	bool parse( void );

	/// Assignment from a string
	UrlParser& operator=(const std::string& url);

	/// URL is a directory.
	bool isDirectory(void) { return _parsed && _file.empty(); }

	/// Parse error indicator.
	bool error(void) { return _error; }

	/// Parse error message.  Only valid if error==false.
	const char* errorMsg(void) { return _errorMsg.c_str(); }

	/// Protocol (or scheme)
	const char* protocol(void) { return _scheme.c_str(); }
	const char* scheme(void) { return _scheme.c_str(); }

	/// User name
	const char* user(void) { return _user.c_str(); }

	/// Password.  WARNING: can be seen in the clear!
	const char* password(void) { return _password.c_str(); }

	/// host
	const char* host(void) { return _host.c_str(); }

	/// port
	const char* port(void) { return _port.c_str(); }

	/// directory
	const char* directory(void) { return _directory.c_str(); }

	/// file
	const char* file(void) { return _file.c_str(); }

	/// path = /directory/file
	const char* path(void) {
		_path = _directory;
		if ( ! _file.empty() ) {
			_path += "/";
			_path += _file;
		}
		return _path.c_str();
	}

	private:

	// Private member data /////////////////////////////////////////////////////

	/// Parsed indicator.
	bool _parsed;

	/// Parse error indicator.
	bool _error;

	/// Parse error message.  Only valid if error==false.
	std::string _errorMsg;

	/// Initialize UrlParser
	void initialize( void );

	/// URL to parse
	std::string _url;

	/// Protocol (or scheme)
	std::string _scheme;

	/// User name.
	std::string _user;

	/// Password.
	std::string _password;

	/// Host
	std::string _host;

	/// Port.
	std::string _port;

	/// Directory.
	std::string _directory;

	/// File.
	std::string _file;

	/// Path.
	std::string _path;

}; // class UrlParser

#endif//__URL_PARSER_H__
