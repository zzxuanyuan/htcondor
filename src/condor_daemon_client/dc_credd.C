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
#include "condor_classad.h"
#include "condor_commands.h"
#include "condor_attributes.h"
#include "daemon.h"
#include "dc_credd.h"
#include "../condor_c++_util/X509credential.h"
#include "simplelist.h"
#include "../condor_credd/credd_constants.h"


#include "../condor_c++_util/credential.h"


// // // // //
// DCCredd
// // // // //

DCCredd::DCCredd( const char* tName, const char* tPool ) 
	: Daemon( DT_CREDD, tName, tPool )
{
}


DCCredd::~DCCredd( void )
{
}

bool
DCCredd::storeSharedSecret (const char *data,
							MyString &ssn,
							CondorError & condor_error) {
	ReliSock *rsock = NULL;
	int rc = 1;
	char *tmp; // for de consting.
	char *ss_name = NULL;

	rsock = (ReliSock *)startCommand(CREDD_STORE_SS,
									 Stream::reli_sock, 20,
									 &condor_error);
	if(!rsock) {
		goto EXIT;
	}
	if(!forceAuthentication( rsock, &condor_error )) {
		goto EXIT;
	}

	rsock->encode();
	tmp = strdup(data);
	if(!tmp) {
		condor_error.pushf("DC_CREDD", 3,
						   "Malloc error: %s", strerror(errno));
		goto EXIT;
	}
	if(!rsock->code(tmp)) {
		condor_error.pushf("DC_CREDD", 4,
						   "Communication error, send shared secret: %s",
						   strerror(errno));
		free(tmp);
		goto EXIT;
	}
	free(tmp);

	rsock->eom();
	rsock->decode();
	rsock->code(rc);
	rsock->code(ss_name);
	rsock->eom();
	rsock->close();
	ssn = ss_name;
	dprintf(D_SECURITY, "Got name '%s'\n", ssn.Value());
	if(rc) {
		condor_error.pushf("DC_CREDD", 7, "Invalid CredD return code (%d)", rc);
	}
 EXIT:
	if(rsock) delete rsock;
	if(ss_name) free(ss_name);
	return (rc == 0) ? true : false;
}

bool 
DCCredd::getSharedSecret(MyString &ss,
						 CondorError & condor_error) {
	locate();
	ReliSock rsock;
	rsock.timeout(20);
	int rc = 1;
	char * secret_str = NULL;
	bool rv = false;
	if(!rsock.connect(_addr)) {
		condor_error.pushf("DC_CREDD", 1, "Failed connect to CredD %s", _addr);
		return false;
	}

	if(!startCommand(CREDD_GET_SS, (Sock*)&rsock)) {
		condor_error.pushf("DC_CREDD", 2, "Failed to start command CREDD_GET_SS");
		return false;
	}

	if(!forceAuthentication(&rsock, &condor_error)) {
		return false;
	}

	rsock.decode();

	if(!rsock.code(rc)) {
		condor_error.pushf("DC_CREDD", 3, "Failed to receive response code");
		return false;
	}
	if(rc != CREDD_SUCCESS) {
		condor_error.pushf("DC_CREDD", 4, "CredD Response code %d"
						   " - read cred_constants.h (sorry)", rc);
	} else {
		rv = true;
	}

	// This gets malloc'd by the called function since it's NULL.
	if(!rsock.code(secret_str)) {
		condor_error.pushf("DC_CREDD", 5, "Failed to receive shared secret");
		return false;
	}
	rsock.eom();
	ss = secret_str;
	rsock.close();
	return rv;
}					 

bool 
DCCredd::storeCredential (Credential * cred,
						  CondorError & condor_error) {

	bool rtnVal = false;
	ReliSock *rsock = NULL;
	void *data = NULL;
	char * classad_str = NULL;
	classad::ClassAd * classad = NULL;
	int rc = 0;
	int size=0;
	std::string adbuffer;
	classad::ClassAdUnParser unparser;

	rsock = (ReliSock *)startCommand(
			CREDD_STORE_CRED, Stream::reli_sock, 20, &condor_error);
	if ( ! rsock ) {
		goto EXIT;
	}

		// Force authentication
	if (!forceAuthentication( rsock, &condor_error )) {
		goto EXIT;
	}

		// Prepare to send
	rsock->encode();

		// Serialize metadata
		// We could just send the classad directly over the wire
		// but that has caused problems in the past for some reason
		// so we unparse it into a string
	classad = cred->GetMetadata();
	unparser.Unparse(adbuffer,classad);
	classad_str = strdup(adbuffer.c_str());

		// Retrieve credential data
	cred->GetData (data, size);	

		// Send the metadata and data
	if (! rsock->code (classad_str) ) {
		condor_error.pushf ("DC_CREDD", 3,
				"Communication error, send credential metadata: %s",
				strerror(errno) );
		goto EXIT;
	}

	if (! rsock->code_bytes (data, size) ) {
		condor_error.pushf ("DC_CREDD", 4,
				"Communication error, send credential data: %s",
				strerror(errno) );
		goto EXIT;
	}

	rsock->eom();

		// Receive the return code
	rsock->decode();
	rsock->code(rc);

	rsock->close();
	if (rc) {
		condor_error.pushf ("DC_CREDD", 4, "Invalid CredD return code (%d)", rc);
	}
	rtnVal =  (rc==0) ? true : false;
EXIT:
	if ( rsock ) delete rsock;
	if ( data ) free (data);
	if ( classad_str ) free (classad_str);
	if ( classad ) delete classad;
	return rtnVal;
}

bool 
DCCredd::getCredentialData (const char * cred_name,
							void *& cred_data,
							int & cred_size,
							CondorError & condor_error) {
	locate();
	
	ReliSock rsock;

	rsock.timeout(20);   // years of research... :)

		// Connect
	if( ! rsock.connect(_addr) ) {
		condor_error.pushf ( "DC_CREDD", 1, "Failed to connect to CredD %s", _addr);
		return false;
	}

		// Start command
	if( ! startCommand(CREDD_GET_CRED, (Sock*)&rsock) ) {
		condor_error.push ( "DC_CREDD", 2, "Failed to start command CREDD_GET_CRED");
		return false;
	}

		// Force authentication
	if (!forceAuthentication( &rsock, &condor_error )) {
		return false;
	}

		// Prepare to send request
	rsock.encode();

	char * _cred_name=strdup(cred_name); // de-const... fucking lame
	rsock.code (_cred_name);
	free (_cred_name);

	rsock.decode();

	if (!(rsock.code(cred_size) && cred_size > 0)) {
		condor_error.push ("DC_CREDD", 3, "ERROR Receiving credential\n");
		return false;
	}

	cred_data = malloc (cred_size);
	if (!rsock.code_bytes (cred_data, cred_size)) {
		free (cred_data);
		cred_data = NULL;
		condor_error.push ("DC_CREDD", 4, "ERROR Receiving credential\n");
		return false;
	}
	
	rsock.close();
	return true;
}

bool
DCCredd::listCredentials (SimpleList <Credential*> & result,  
						  int & size,
						  CondorError & condor_error) {

	bool rtnVal = false;	// default
	ReliSock *rsock = NULL;
	Credential * cred = NULL;
	classad::ClassAdParser parser;
	classad::ClassAd * ad = NULL;
	char * request = "_";

	rsock = (ReliSock *)startCommand(
			CREDD_QUERY_CRED, Stream::reli_sock, 20, &condor_error);
	if ( ! rsock ) {
		goto EXIT;
	}

		// Force authentication
	if (!forceAuthentication( rsock, &condor_error )) {
		goto EXIT;
	}

	rsock->encode();
	rsock->code (request);
	rsock->eom();

		// Receive response
	rsock->decode();

	rsock->code (size);
	if (size == 0) {
		rtnVal = true;
		goto EXIT;
	}

	int i;
	for (i=0; i<size; i++) {
		char * classad_str = NULL;
		if (!(rsock->code (classad_str))) {
			condor_error.push ("DC_CREDD", 3, "Unable to receive credential data");
			goto EXIT;
		}

		ad = parser.ParseClassAd (classad_str);
		if (!ad) {
			condor_error.push ("DC_CREDD", 4, "Unable to parse credential data");
			goto EXIT;
		}

		cred = new X509Credential (*ad);
		result.Append (cred);
	}

	rtnVal =  true;	// success

EXIT:
	if (ad) delete ad;
	if ( rsock ) delete rsock;
	return rtnVal;
}

bool 
DCCredd::removeCredential (const char * cred_name,
						   CondorError & condor_error) {

	bool rtnVal = false;
	ReliSock *rsock = NULL;
	char * _cred_name = NULL;
	int rc=0;

	rsock = (ReliSock *)startCommand(
			CREDD_REMOVE_CRED, Stream::reli_sock, 20, &condor_error);
	if ( ! rsock ) {
		goto EXIT;
	}

		// Force authentication
	if (!forceAuthentication( rsock, &condor_error )) {
		goto EXIT;
	}

	rsock->encode();

	_cred_name=strdup(cred_name); // de-const... fucking lame
	if ( ! rsock->code (_cred_name) ) {
		condor_error.pushf ( "DC_CREDD", 3, "Error sending credential name: %s",
				strerror(errno) );
		goto EXIT;
	}

	if ( ! rsock->eom() ) {
		condor_error.pushf ( "DC_CREDD", 3, "Error sending credential eom: %s",
				strerror(errno) );
		goto EXIT;
	}
	rsock->decode();

	if ( ! rsock->code (rc) ) {
		condor_error.pushf ( "DC_CREDD", 3, "Error rcving credential rc: %s",
				strerror(errno) );
		goto EXIT;
	}

	if (rc) {
		condor_error.push ( "DC_CREDD", 3, "Error removing credential");
		goto EXIT;
	}

	rtnVal = rc;
EXIT:
	if ( rsock ) delete rsock;
	if ( _cred_name ) free ( _cred_name );
	return rtnVal;
}
