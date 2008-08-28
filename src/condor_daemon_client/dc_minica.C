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
#include "dc_minica.h"
#include "../condor_minica/minica_common.h"

// // // // //
// DCMinica
// // // // //

DCMinica::DCMinica( const char* tName, const char* tPool ) 
	: Daemon( DT_MINICA, tName, tPool )
{
	initMembers();
	setKeyFilename(NULL);
	setReqFilename(NULL);
	setCertFilename(NULL);
	setCommonName(NULL);
	setPassiveClient(false);
	setUseServerOpensslCnf(false);
	setMinicaServer(NULL);
	setPathToOpenssl(NULL);
	setPathToOpensslCnf(NULL);
}


DCMinica::~DCMinica( void )
{
}

void
DCMinica::initMembers( void )
{
	m_key_filename = NULL;
}

bool
DCMinica::getOpensslCnf( void )
{
    char *openssl_cnf = NULL;
    char path[] = "/tmp/openssl.cnf.XXXXXX";
    int clen, cnf_fd;
    
    m_reli_sock.decode( );
    if( ! (m_reli_sock.code( openssl_cnf )) ) {
        m_error_stack.pushf("DC_MINICA",0,"error getting openssl.cnf file from server." );
        return false;
    }
    if( ! (m_reli_sock.end_of_message( )) ) {
        m_error_stack.pushf("DC_MINICA",0,"error communicating with server." );
		free(openssl_cnf);
        return false;
    }
    if( (cnf_fd = mkstemp( path )) == -1 ) {
        m_error_stack.pushf("DC_MINICA",0,"error making temp file for openssl.cnf." );
		free(openssl_cnf);
        return false;
    }
	m_path_to_openssl_cnf = path;
    clen = strlen( openssl_cnf );
    if( write( cnf_fd, openssl_cnf, clen ) != clen ) {
        m_error_stack.pushf("DC_MINICA",0,"Error writing openssl.cnf.\n" );
		free(openssl_cnf);
        return false;
    }
	free(openssl_cnf);
    if( close( cnf_fd ) != 0 ) {
        m_error_stack.pushf("DC_MINICA",0,"Error closing temp file." );
        return false;
    }
	return true;
}

bool
DCMinica::maybeTransferOpensslCnf( void )
{
	int xfer = m_use_server_openssl_cnf ? 1 : 0; // TODO: is this right?
    m_reli_sock.encode( );
    if( ! ( (m_reli_sock.code( xfer ))
            && (m_reli_sock.end_of_message( )) ) ) {
        m_error_stack.pushf("DC_MINICA",0,"Error sending config file status.\n" );
        return false;
    }
    if( xfer ) {
        //dprintf(D_SECURITY, "Getting openssl.cnf.\n" );
		getOpensslCnf();
		//dprintf(D_SECURITY, "Got path to openssl.cnf: '%s'\n", m_path_to_openssl_cnf );
    }
	return true;
}

bool
DCMinica::handlePassiveClient( void )
{
    int response_code;
    char *error_message = NULL;
	char *t1 = NULL; // TODO what if password is NULL?
	char *t2 = NULL;
	char *key_block = NULL;
	char *cert_request = NULL;

	t1 = strdup(m_password.Value());
	t2 = strdup(m_common_name.Value());
	if(!t1) {
		m_error_stack.pushf("DC_MINICA",0,"Memory.");
		return false;
	}
	if(!t2) {
		m_error_stack.pushf("DC_MINICA",0,"Memory.");
		free(t1);
		return false;
	}

    m_reli_sock.encode( ); /* 5 */
    if( ! ( (m_reli_sock.code( t1 ))
            && (m_reli_sock.code( t2 ))
            && (m_reli_sock.end_of_message( )) ) ) {
        m_error_stack.pushf("DC_MINICA",0,"Error sending data to server." );
		free(t1);
		free(t2);
        return false;
    }

    m_reli_sock.decode( ); /* 6 */
    if( ! ( (m_reli_sock.code( response_code ))
            && (m_reli_sock.code( error_message ))
            && (m_reli_sock.code( key_block ))
            && (m_reli_sock.code( cert_request ))
            && (m_reli_sock.end_of_message( )) ) ) {
        m_error_stack.pushf("DC_MINICA",0,"Error getting key from server." );
		if(key_block) free(key_block);
		if(cert_request) free(cert_request);
		return false;
    }

	m_key_block = key_block;
	m_cert_request = cert_request;
	if(key_block) free(key_block);
	if(cert_request) free(cert_request);

    if( response_code != MCA_RESPONSE_ALL_OK ) {
        m_error_stack.pushf("DC_MINICA",0,"Server generated error: %s.", error_message );
		return false;
    }
	return true;
}

bool
DCMinica::handleActiveClient( void )
{
    int error_code;
    char *key_block;
	char *cert_request;

    /** Fork to generate rsa key **/
    dprintf( D_SECURITY, "Generating key...\n" );

    key_block = repeat_gen_rsa_key( m_password.GetCStr(), MCA_MAX_KEYGEN_ATTEMPTS,
                                    MCA_KEYGEN_WAIT_TIME_SECS, 
									m_path_to_openssl.GetCStr(),
									&error_code );
    
    if( error_code > MCA_ERROR_NOERROR ) {
        m_error_stack.pushf("DC_MINICA",0,"Error generating key: %s",
                     get_mca_error_message( error_code ) );
		if(key_block) free(key_block);
		return false;
    }
    if( key_block == NULL ) {
        m_error_stack.pushf("DC_MINICA",0,"Key generation failed.  Can't continue." );
        return false;
    }
	m_key_block = key_block;
	free(key_block);

    // dprintf( D_SECURITY, "getting signing request.\n" );
    /** Fork to generate signing request. **/
	cert_request = gen_signing_request( m_key_block.Value(), 
										m_common_name.Value(), 
										m_password.GetCStr(),
										m_path_to_openssl.GetCStr(),
										m_path_to_openssl_cnf.GetCStr(),
										&error_code );
    
    if( error_code > MCA_ERROR_NOERROR ) {
        m_error_stack.pushf("DC_MINICA",0,"Error generating certificate request: %s",
							get_mca_error_message( error_code ) );
		if(cert_request) free(cert_request);
		return false;
							 
    }
	m_cert_request = cert_request;
    if( cert_request == NULL ) {
        m_error_stack.pushf("DC_MINICA",0,"Certificate request generation failed. Can't continue." );
        return false;
    }
	free(cert_request);
	return true;
}

/**
 * Get the key and certificate signing request, either passively or not.
 */
bool
DCMinica::getKeyCsr( void )
{
    int minica_server_status = 0;
    char *minica_server_response = NULL;
	int passive_client = m_passive_client ? 1 : 0;

    m_reli_sock.encode( ); /* 3 */
    if( ! ( (m_reli_sock.code( passive_client ))
            && (m_reli_sock.end_of_message( )) ) ) {
        m_error_stack.pushf("DC_MINICA",0,"Error sending data to server.\n" );
        return false;
    }

    m_reli_sock.decode( ); /* 4 */
    if( ! ( (m_reli_sock.code( minica_server_status ))
            && (m_reli_sock.code( minica_server_response ))
            && (m_reli_sock.end_of_message( )) ) ) {
        m_error_stack.pushf("DC_MINICA",0,"Error receiving response from server (4)." );
		if(minica_server_response) free(minica_server_response);
        return false;
    }
    switch( minica_server_status ) {
    case MCA_PROTOCOL_ABORT:
        m_error_stack.pushf("DC_MINICA",0,"Server sent protocol abort response.\n" );
        m_error_stack.pushf("DC_MINICA",0,"Server sent: '%s'\n", minica_server_response );
        if(minica_server_response) free(minica_server_response);
		return false;
        break;
    case MCA_PROTOCOL_PROCEED:
        //m_error_stack.pushf("DC_MINICA",0,"Server sent: '%s'\n", minica_server_response );
		if(minica_server_response) free(minica_server_response);
        break;
    default:
        m_error_stack.pushf("DC_MINICA",0,"Server sent unknown protocol message.\n" );
        if(minica_server_response) free(minica_server_response);
		return false;
		break;
    }

    if( passive_client ) {
        return handlePassiveClient( );
    }
	return handleActiveClient( ); 
}

/**
 * Get the signed certificate given the request.
 */
bool
DCMinica::getCert( void )
{
    int response_code = 0;
    char *error_message = NULL;
	char *cert_request = strdup(m_cert_request.Value());
	char *signed_cert = NULL;
    
    //dprintf( D_SECURITY, "Sending cert to server.\n" );
    m_reli_sock.encode( ); /* 7 */
    m_reli_sock.code( cert_request );
    m_reli_sock.end_of_message();
    
    m_reli_sock.decode( ); /* 8 */
    if( ! ( ( m_reli_sock.code( response_code )) &&
            ( m_reli_sock.code( error_message )) &&
            ( m_reli_sock.code( signed_cert )) &&
            ( m_reli_sock.end_of_message()))) {
        m_error_stack.pushf("DC_MINICA",0,"Got error receiving data from minica server.\n" );
		if(signed_cert) free(signed_cert);
		if(error_message) free(error_message);
		return false;
    }
    if( response_code != MCA_RESPONSE_ALL_OK ) {
        m_error_stack.pushf("DC_MINICA",0,"Server generated error (8): %s\n", error_message );
		if(signed_cert) free(signed_cert);
		if(error_message) free(error_message);
		return false;
    }
	if(error_message) free(error_message);

    if( (signed_cert == NULL) || (strlen( signed_cert ) == 0) ) {
        m_error_stack.pushf("DC_MINICA",0,"No certificate was returned.\n" );
		if(signed_cert) free(signed_cert);
		return false;
    }
	m_signed_cert = signed_cert;
	if(signed_cert) free(signed_cert);
	return true;
}

/**
 * The protocol preface involves a version comparision.
 */
bool
DCMinica::checkVersionCompatibility( void )
{
    char * minica_client_version = NULL;
    char * minica_server_version = NULL;
    char * minica_server_response = NULL;
    int minica_server_status = 0;
	MyString client;
	MyString server;
	MyString response;

    /* Protocol init. */
    m_reli_sock.encode( ); /* Numbers refer to protocol stages.  1 */
    minica_client_version = strdup( MCA_PROTOCOL_VERSION );
	if(!minica_client_version) {
		//perror("strdup");
		m_error_stack.pushf("DC_MINICA",0,"Memory.");
		return false;
	}
    if( ! ( (m_reli_sock.code( minica_client_version ))
            && (m_reli_sock.end_of_message( )) ) ) {
        m_error_stack.pushf("DC_MINICA",0,"Error sending version to server.\n" );
        if(minica_client_version) free(minica_client_version);
		return false;
    }
	client = minica_client_version;
	if(minica_client_version) free(minica_client_version);

    m_reli_sock.decode( ); /* 2 */
    if( ! ( (m_reli_sock.code( minica_server_version ))
            && (m_reli_sock.code( minica_server_status ))
            && (m_reli_sock.code( minica_server_response ))
            && (m_reli_sock.end_of_message( )) ) ) {
        m_error_stack.pushf("DC_MINICA",0,"Error receiving version from server.\n" );
        if(minica_server_version) free(minica_server_version);
		if(minica_server_response) free(minica_server_response);
		return false;
    }
	server = minica_server_version;
	response = minica_server_response;
	if(minica_server_version) free(minica_server_version);
	if(minica_server_response) free(minica_server_response);
    
    switch( minica_server_status ) {
    case MCA_PROTOCOL_ABORT:
        m_error_stack.pushf("DC_MINICA",0,"Server sent protocol abort response.\n" );
        m_error_stack.pushf("DC_MINICA",0,"Client version: '%s'\nServer version: '%s'\n",
							client.Value(), server.Value());
        m_error_stack.pushf("DC_MINICA",0,"Server sent: '%s'\n", response.Value() );
        return false;
        break;
    case MCA_PROTOCOL_PROCEED:
        m_error_stack.pushf("DC_MINICA",0,"Server sent: '%s'\n", response.Value() );
        break;
    default:
        m_error_stack.pushf("DC_MINICA",0,"Server sent unknown protocol message.\n" );
        return false;
		break;
    }
	return true;
}

bool 
DCMinica::signRequest( CondorError &error_stack )
{
	bool rv = false;

	m_error_stack = error_stack;
	m_reli_sock.timeout(20);
	if(!m_reli_sock.connect(_addr)) {
		m_error_stack.pushf("DC_MINICA",0, "Can't connect.");
		goto error;
	}
	if(!startCommand(DC_SIGN_CERT_REQUEST, (Sock*)&m_reli_sock)) {
		m_error_stack.pushf("DC_MINICA", 0, "Can't start command.");
		goto error;
	}

	if(!forceAuthentication( &m_reli_sock, &m_error_stack )) {
		goto error;
	}

    //dprintf( D_SECURITY, "Checking version compatability.\n" );
    if(!checkVersionCompatibility( )) {
		goto error;
	}

	//dprintf( D_SECURITY, "Sending config file request status.\n" );
    if(!maybeTransferOpensslCnf( )) {
		goto error;
	}

	if(!getKeyCsr( )) {
		goto error;
	}
	
	if(!str_to_file( m_key_block.Value(), m_key_filename.Value() ) ) {
        m_error_stack.pushf("DC_MINICA",0,"Key generated OK, but can't save to file '%s'."
							"Aborting.", m_key_filename.Value());
        goto error;
    }
    if(!str_to_file( m_cert_request.Value(), m_request_filename.Value() ) ) {
        m_error_stack.pushf("DC_MINICA",0,"Request generated OK, but can't save to file "
							 "'%s'. Aborting.\n", m_request_filename.Value());
        goto error;
    }

    if(!getCert( )) {
		goto error;
	}
    
    if(!str_to_file( m_signed_cert.Value(), m_cert_filename.Value() ) ) {
        m_error_stack.pushf("DC_MINICA",0,"Error writing signed certificate to file "
							 "'%s'.  Aborting.\n", m_cert_filename.Value());
        goto error;
    }

    dprintf( D_SECURITY, "Obtained certificate.\n" );
	rv = true;

 error:
	error_stack = m_error_stack;
    return rv;
}
