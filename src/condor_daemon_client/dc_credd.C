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
#include "condor_classad.h"
#include "condor_commands.h"
#include "condor_attributes.h"
#include "daemon.h"
#include "dc_credd.h"
#include "../condor_c++_util/X509credential.h"


#include "../condor_c++_util/credential.h"
#include "classad_distribution.h"


// // // // //
// DCCredd
// // // // //

DCCredd::DCCredd( const char* name, const char* pool ) 
	: Daemon( DT_CREDD, name, pool )
{
}


DCCredd::~DCCredd( void )
{
}

bool 
DCCredd::storeCredential (Credential * cred,
						  CondorError & error) {
	this->locate();

	ReliSock rsock;

	rsock.timeout(20);   // years of research... :)

		// Connect
	if( ! rsock.connect(_addr) ) {
		error.pushf ( "DC_CREDD", 1, "Failed to connect to CredD %s", _addr);
		return false;
	}

		// Start command
	if( ! startCommand(CREDD_STORE_CRED, (Sock*)&rsock) ) {
		error.push ( "DC_CREDD", 2, "Failed to start command CREDD_STORE_CRED");
		return false;
	}

		// Force authentication
	if (!forceAuthentication( &rsock, &error )) {
		return false;
	}

		// Prepare to send
	rsock.encode();

		// Serialize metadata
		// We could just send the classad directly over the wire
		// but that has caused problems in the past for some reason
		// so we unparse it into a string
	classad::ClassAd * classad = cred->GetMetadata();
	classad::ClassAdUnParser unparser;
	std::string adbuffer;
	unparser.Unparse(adbuffer,classad);
	char * classad_str = strdup(adbuffer.c_str());
	delete classad;

		// Retrieve credential data
	void * data=NULL;
	int size=0;
	cred->GetData (data, size);	

		// Send the metadata and data
	if (!(rsock.code (classad_str) && 
		  rsock.code_bytes (data, size))) {
		error.push ("DC_CREDD", 3, "Communication error");

		if (classad_str) free (classad_str);
		if (data) free (data);
		return false;
	}
	
	if (classad_str) free (classad_str);
	if (data) free (data);
	
	rsock.eom();

		// Receive the return code
	int rc = 0;
	rsock.decode();
	rsock.code(rc);

	rsock.close();
	if (rc) {
		error.pushf ("DC_CREDD", 4, "Invalid CredD return code (%d)", rc);
	}
	return (rc==0);
}

bool 
DCCredd::getCredentialData (const char * cred_name,
							void *& cred_data,
							int & cred_size,
							CondorError & error) {
	locate();
	
	ReliSock rsock;

	rsock.timeout(20);   // years of research... :)

		// Connect
	if( ! rsock.connect(_addr) ) {
		error.pushf ( "DC_CREDD", 1, "Failed to connect to CredD %s", _addr);
		return false;
	}

		// Start command
	if( ! startCommand(CREDD_GET_CRED, (Sock*)&rsock) ) {
		error.push ( "DC_CREDD", 2, "Failed to start command CREDD_GET_CRED");
		return false;
	}

		// Force authentication
	if (!forceAuthentication( &rsock, &error )) {
		return false;
	}

		// Prepare to send request
	rsock.encode();

	char * _cred_name=strdup(cred_name); // de-const... fucking lame
	rsock.code (_cred_name);
	free (_cred_name);

	rsock.decode();

	if (!(rsock.code(cred_size) && cred_size > 0)) {
		error.push ("DC_CREDD", 3, "ERROR Receiving credential\n");
		return false;
	}

	cred_data = malloc (cred_size);
	if (!rsock.code_bytes (cred_data, cred_size)) {
		free (cred_data);
		cred_data = NULL;
		error.push ("DC_CREDD", 4, "ERROR Receiving credential\n");
		return false;
	}
	
	rsock.close();
	return true;
}

bool
DCCredd::listCredentials (SimpleList <Credential*> & result,  
						  int & size,
						  CondorError & error) {

	this->locate();

	ReliSock rsock;

	rsock.timeout(20);   // years of research... :)

		// Connect
	if( ! rsock.connect(_addr) ) {
		error.pushf ( "DC_CREDD", 1, "Failed to connect to CredD %s", _addr);
		return false;
	}

		// Start command
	if( ! startCommand(CREDD_QUERY_CRED, (Sock*)&rsock) ) {
		error.push ( "DC_CREDD", 2, "Failed to start command CREDD_QUERY_CRED");
		return false;
	}

		// Force authentication
	if (!forceAuthentication( &rsock, &error )) {
		return false;
	}

	rsock.encode();
	char * request = "_";
	rsock.code (request);
	rsock.eom();

		// Receive response
	rsock.decode();

	rsock.code (size);
	if (size == 0)
		return true;
	
	classad::ClassAdParser parser;


	int i;
	for (i=0; i<size; i++) {
		char * classad_str = NULL;
		if (!(rsock.code (classad_str))) {
			error.push ("DC_CREDD", 3, "Unable to receive credential data");
			return false;
		}

		classad::ClassAd * ad = parser.ParseClassAd (classad_str);
		if (!ad) {
			error.push ("DC_CREDD", 4, "Unable to parse credential data");
			return false;
		}

		Credential * cred = new X509Credential (*ad);
		delete ad;
		result.Append (cred);
	}

	return true;
}
						  
bool 
DCCredd::removeCredential (const char * cred_name,
						   CondorError & error) {

	locate();
	
	ReliSock rsock;

	rsock.timeout(20);   // years of research... :)

		// Connect
	if( ! rsock.connect(_addr) ) {
		error.pushf ( "DC_CREDD", 1, "Failed to connect to CredD %s", _addr);
		return false;
	}

		// Start command
	if( ! startCommand(CREDD_REMOVE_CRED, (Sock*)&rsock) ) {
		error.push ( "DC_CREDD", 2, "Failed to start command CREDD_REMOVE_CRED");
		return false;
	}

		// Force authentication
	if (!forceAuthentication( &rsock, &error )) {
		return false;
	}

	rsock.encode();

	char * _cred_name=strdup(cred_name); // de-const... fucking lame
	rsock.code (_cred_name);
	free (_cred_name);

	rsock.eom();
	rsock.decode();

	int rc=0;
	rsock.code (rc);
	if (rc) {
		error.push ( "DC_CREDD", 3, "Error removing credential");
	}

	return (rc==0);
}
