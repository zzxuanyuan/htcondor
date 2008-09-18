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


#ifndef __CONDOR_DC_MINICA_H__
#define __CONDOR_DC_MINICA_H__

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_io.h"
#include "enum_utils.h"
#include "daemon.h"
#include "MyString.h"
#include "simplelist.h"

class DCMinica : public Daemon {
 public:
	
	/** Constructor.  Same as a Daemon object.
		@param name The name (or sinful string) of the daemon, NULL
		if you want local  
		@param pool The name of the pool, NULL if you want local
	*/
	DCMinica( const char* const name = NULL, const char* pool = NULL );
	
	/// Destructor
	~DCMinica();
	
	// There's only one thing this daemon knows how to do.
	bool signRequest( CondorError &error );
	
    /*	bool removeCredential (const char * cred_name,
		CondorError & error);*/

	// Set members.  Which ones of these can go below?
	void setKeyFilename( MyString kfn ) { m_key_filename = kfn; }
	void setReqFilename( MyString rfn ) { m_request_filename = rfn; }
	void setCertFilename( MyString cfn ) { m_cert_filename = cfn; }
	void setCommonName( MyString cn ) { m_common_name = cn; }
	void setPassword(MyString pw ) { m_password = pw; }
	void setPassiveClient( bool pc ) { m_passive_client = pc; }
	void setUseServerOpensslCnf( bool us ) { m_use_server_openssl_cnf = us; }
	void setMinicaServer( MyString mca_s ) { m_minica_server = mca_s; }
	void setPathToOpenssl( MyString po ) { m_path_to_openssl = po; }
	void setPathToOpensslCnf( MyString poc ) { m_path_to_openssl_cnf = poc; }
	MyString getCommonName() { return m_common_name; }

 protected:
	//Aux functions for signRequest above.
	bool getOpensslCnf( void );
	bool maybeTransferOpensslCnf( void );

	/**
	 * When the client is passive, i.e. doesn't generate their own key or
	 * certificate request, this gets the server side to do that.
	 */
	bool handlePassiveClient( void );
// char *pass, char *common_name, 
//							  char **key_block, char **cert_request );

    /**
	 * When the client does generate the key and signing request.  Active
	 * => not passive.
	 */
	bool handleActiveClient( void );
//char *pass, char *common_name, char **key_block,
//							 char **cert_request );

//	bool getKeyCsr( int passive_client,
//					char *pass, char *common_name,
//					char **key_block, char **cert_request);
	bool getKeyCsr( void );

//    bool getCert( char *cert_request, char **signed_cert );
	bool getCert( void );

	bool checkVersionCompatibility( void );

	void initMembers();
	void setReliSock( ReliSock s ) { m_reli_sock = s; }
		
    MyString m_key_filename;
    MyString m_request_filename;
    MyString m_cert_filename;
    MyString m_common_name;
	MyString m_password;
    bool  m_passive_client;
    bool  m_use_server_openssl_cnf;
	MyString m_minica_server;
	MyString m_path_to_openssl;
	MyString m_path_to_openssl_cnf;
	ReliSock m_reli_sock;
	CondorError m_error_stack;
	MyString m_key_block;
	MyString m_cert_request;
	MyString m_signed_cert;

};
#endif /* _CONDOR_DC_MINCA_H */
