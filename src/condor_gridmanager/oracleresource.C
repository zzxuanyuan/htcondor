/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
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
#include "string_list.h"

#include "oracleresource.h"
#include "gridmanager.h"

// timer id value that indicates the timer is not registered
#define TIMER_UNSET		-1

#define XACT_COMMIT_DELAY	5

template class List<OracleJob>;
template class Item<OracleJob>;
template class List<OciSession>;
template class Item<OciSession>;
template class HashTable<HashKey, OciSession *>;
template class HashBucket<HashKey, OciSession *>;

#define HASH_TABLE_SIZE			500

template class HashTable<HashKey, OciServer *>;
template class HashBucket<HashKey, OciServer *>;

HashTable <HashKey, OciServer *> ServersByName( HASH_TABLE_SIZE,
												hashFunction );

OCIEnv *GlobalOciEnvHndl = NULL;
OCIError *GlobalOciErrHndl = NULL;
bool GlobalOciInitDone = false;

int InitGlobalOci()
{
	int rc;
	char *param_value = NULL;
	MyString buff;

	if ( GlobalOciInitDone ) {
		return OCI_SUCCESS;
	}

	param_value = param("ORACLE_HOME");
	if ( param_value == NULL ) {
		EXCEPT("ORACLE_HOME undefined!");
	}
	buff.sprintf( "ORACLE_HOME=%s", param_value );
	putenv( strdup( buff.Value() ) );
	free(param_value);

	if ( GlobalOciEnvHndl == NULL ) {
dprintf(D_ALWAYS,"*OCIEnvCreate( &GlobalOciEnvHndl, OCI_DEFAULT, NULL, NULL, NULL, NULL, 0, NULL )...\n");
		rc = OCIEnvCreate( &GlobalOciEnvHndl, OCI_DEFAULT, NULL, NULL, NULL,
						   NULL, 0, NULL );
		if ( rc != OCI_SUCCESS ) {
dprintf(D_ALWAYS,"***OCIEnvCreate failed\n");
			return rc;
		}
	}

	if ( GlobalOciErrHndl == NULL ) {
dprintf(D_ALWAYS,"*OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&GlobalOciErrHndl, OCI_HTYPE_ERROR, 0, NULL )...\n");
		rc = OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&GlobalOciErrHndl,
							 OCI_HTYPE_ERROR, 0, NULL );
		if ( rc != OCI_SUCCESS ) {
dprintf(D_ALWAYS,"***OCIHandleAlloc failed\n");
			return rc;
		}
	}

	GlobalOciInitDone = true;

	return OCI_SUCCESS;
}

OciSession *GetOciSession( const char *db_name, const char *db_username,
						   const char *db_password )
{
	int rc;
	OciServer *server;
	OciSession *session;

	if ( db_name == NULL || db_username == NULL || db_password == NULL ) {
		return NULL;
	}

	rc = ServersByName.lookup( HashKey( db_name ), server );

	if ( rc != 0 ) {
		server = new OciServer( db_name );
		ASSERT(server);
		ServersByName.insert( HashKey( db_name ), server );
	} else {
		ASSERT(server);
	}

	session = server->FindSession( db_username );
	if ( session == NULL ) {
		session = new OciSession( server, db_username, db_password );
		ASSERT(session);
	}

	return session;
}

OciSession::OciSession( OciServer *oci_server, const char *db_username,
						const char *db_password )
{
	initDone = false;
	server = oci_server;
	username = strdup( db_username );
	password = strdup( db_password );
	ociSvcCtxHndl = NULL;
	ociSessionHndl = NULL;
	ociTransHndl = NULL;
	ociErrorHndl = NULL;
	sessionOpen = false;
	registeredJobs = new List<OracleJob>();

	server->RegisterSession( this, username );
}

OciSession::~OciSession()
{
	if ( registeredJobs != NULL ) {
		delete registeredJobs;
	}
	if ( server != NULL ) {
		server->UnregisterSession( this, username );
	}
	if ( username != NULL ) {
		free( username );
	}
	if ( password != NULL ) {
		free( username );
	}
}

int OciSession::Initialize()
{
	int rc;

	if ( initDone ) {
		return OCI_SUCCESS;
	}

	if ( (rc = InitGlobalOci()) != OCI_SUCCESS ) {
		return rc;
	}

	if ( ociErrorHndl == NULL ) {
dprintf(D_ALWAYS,"*OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&ociErrorHndl, OCI_HTYPE_ERROR, 0, NULL )...\n");
		rc = OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&ociErrorHndl,
							 OCI_HTYPE_ERROR, 0, NULL );
		if ( rc != OCI_SUCCESS ) {
dprintf(D_ALWAYS,"***OCIHandleAlloc failed\n");
			return rc;
		}
	}

	if ( ociSessionHndl == NULL ) {
dprintf(D_ALWAYS,"*OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&ociSessionHndl, OCI_HTYPE_SESSION, 0, NULL )\n");
		rc = OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&ociSessionHndl,
							 OCI_HTYPE_SESSION, 0, NULL );
		if ( rc != OCI_SUCCESS ) {
dprintf(D_ALWAYS,"***OCIHandleAlloc failed\n");
			return rc;
		}
	}

	if ( ociSvcCtxHndl == NULL ) {
dprintf(D_ALWAYS,"*OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&ociSvcCtxHndl, OCI_HTYPE_SVCCTX, 0, NULL )\n");
		rc = OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&ociSvcCtxHndl,
							 OCI_HTYPE_SVCCTX, 0, NULL );
		if ( rc != OCI_SUCCESS ) {
dprintf(D_ALWAYS,"***OCIHandleAlloc failed\n");
			return rc;
		}
	}

	if ( ociTransHndl == NULL ) {
dprintf(D_ALWAYS,"*OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&ociTransHndl, OCI_HTYPE_TRANS, 0, NULL )\n");
		rc = OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&ociTransHndl,
							 OCI_HTYPE_TRANS, 0, NULL );
		if ( rc != OCI_SUCCESS ) {
dprintf(D_ALWAYS,"***OCIHandleAlloc failed\n");
			return rc;
		}
	}

dprintf(D_ALWAYS,"*OCIAttrSet( ociSessionHndl, OCI_HTYPE_SESSION, username, strlen(username), OCI_ATTR_USERNAME, ociErrorHndl )\n");
	rc = OCIAttrSet( ociSessionHndl, OCI_HTYPE_SESSION, username,
					 strlen(username), OCI_ATTR_USERNAME, ociErrorHndl );
	if ( rc != OCI_SUCCESS ) {
dprintf(D_ALWAYS,"***OCIAttrSet failed\n");
		return rc;
	}

dprintf(D_ALWAYS,"*OCIAttrSet( ociSessionHndl, OCI_HTYPE_SESSION, password, strlen(password), OCI_ATTR_PASSWORD, ociErrorHndl )\n");
	rc = OCIAttrSet( ociSessionHndl, OCI_HTYPE_SESSION, password,
					 strlen(password), OCI_ATTR_PASSWORD, ociErrorHndl );
	if ( rc != OCI_SUCCESS ) {
dprintf(D_ALWAYS,"***OCIAttrSet failed\n");
		return rc;
	}

dprintf(D_ALWAYS,"*rc = OCIAttrSet( ociSvcCtxHndl, OCI_HTYPE_SVCCTX, ociSessionHndl, 0, OCI_ATTR_SESSION, ociErrorHndl )\n");
	rc = OCIAttrSet( ociSvcCtxHndl, OCI_HTYPE_SVCCTX, ociSessionHndl, 0,
					 OCI_ATTR_SESSION, ociErrorHndl );
	if ( rc != OCI_SUCCESS ) {
dprintf(D_ALWAYS,"***OCIAttrSet failed\n");
		return rc;
	}

dprintf(D_ALWAYS,"*rc = OCIAttrSet( ociSvcCtxHndl, OCI_HTYPE_SVCCTX, ociTransHndl, 0, OCI_ATTR_TRANS, ociErrorHndl )\n");
	rc = OCIAttrSet( ociSvcCtxHndl, OCI_HTYPE_SVCCTX, ociTransHndl, 0,
					 OCI_ATTR_TRANS, ociErrorHndl );
	if ( rc != OCI_SUCCESS ) {
dprintf(D_ALWAYS,"***OCIAttrSet failed\n");
		return rc;
	}

	initDone = true;

	return OCI_SUCCESS;
}

void OciSession::ServerConnectionClosing()
{
	int rc;

		// TODO: what about any open transactions?

	rc = CloseSession();
	if ( rc != OCI_SUCCESS ) {
		EXCEPT( "OCISessionEnd() failed!\n" );
	}

	return;
}

void OciSession::RegisterJob( OracleJob *job )
{
	registeredJobs->Append( job );
}

void OciSession::UnregisterJob( OracleJob *job )
{

	registeredJobs->Delete( job );

		// TODO: if this is last job, arrange to close session and delete
		//   this object
}

int OciSession::AcquireSession( OracleJob *job, OCISvcCtx *&svc_hndl,
								OCIError *&err_hndl )
{
	int rc;

	if ( sessionOpen == false ) {
		rc = OpenSession( err_hndl );
		if ( rc != OCI_SUCCESS ) {
			return rc;
		}
	}

dprintf(D_ALWAYS,"***ociSvcCtxHndl=0x%x\n",ociSvcCtxHndl);
	svc_hndl = ociSvcCtxHndl;

	return OCI_SUCCESS;
}

int OciSession::ReleaseSession( OracleJob *job )
{
	return OCI_SUCCESS;
}

int OciSession::OpenSession( OCIError *&err_hndl )
{
	int rc;
	OCIServer *server_hndl;

	if ( sessionOpen ) {
		return OCI_SUCCESS;
	}

	rc = Initialize();
	if ( rc != OCI_SUCCESS ) {
		if ( rc == OCI_ERROR ) {
			err_hndl = ociErrorHndl;
		}
		return rc;
	}

	rc = server->SessionActive( this, server_hndl, err_hndl );
	if ( rc != OCI_SUCCESS ) {
		return rc;
	}

dprintf(D_ALWAYS,"*OCIAttrSet( ociSvcCtxHndl, OCI_HTYPE_SVCCTX, server_hndl, 0, OCI_ATTR_SERVER, ociErrorHndl )\n");
	rc = OCIAttrSet( ociSvcCtxHndl, OCI_HTYPE_SVCCTX, server_hndl, 0,
					 OCI_ATTR_SERVER, ociErrorHndl );
	if ( rc != OCI_SUCCESS ) {
dprintf(D_ALWAYS,"***OCIAttrSet failed\n");
		if ( rc == OCI_ERROR ) {
			err_hndl = ociErrorHndl;
		}
		server->SessionInactive( this );
		return rc;
	}

dprintf(D_ALWAYS,"*OCISessionBegin( ociSvcCtxHndl, ociErrorHndl, ociSessionHndl, OCI_CRED_RDBMS, OCI_DEFAULT )...\n");
	rc = OCISessionBegin( ociSvcCtxHndl, ociErrorHndl, ociSessionHndl,
						  OCI_CRED_RDBMS, OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
dprintf(D_ALWAYS,"***OCISessionBegin failed\n");
		if ( rc == OCI_ERROR ) {
			err_hndl = ociErrorHndl;
		}
		server->SessionInactive( this );	
		return rc;
	}
dprintf(D_ALWAYS,"***ociSvcCtxHndl=0x%x\n",ociSvcCtxHndl);

	sessionOpen = true;

	return OCI_SUCCESS;
}

int OciSession::CloseSession()
{
	int rc;

	if ( sessionOpen == false ) {
		return OCI_SUCCESS;
	}

		// TODO: what about any open transactions?

dprintf(D_ALWAYS,"*OCISessionEnd( ociSvcCtxHndl, ociErrorHndl, NULL, OCI_DEFAULT )...\n");
	rc = OCISessionEnd( ociSvcCtxHndl, ociErrorHndl, NULL, OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
dprintf(D_ALWAYS,"***OCISessionEnd failed\n");
		return rc;
	}

	sessionOpen = false;

	return OCI_SUCCESS;
}

OciServer::OciServer( const char *db_name )
	: sessionsByUsername( HASH_TABLE_SIZE, hashFunction )
{
	initDone = false;
	dbName = strdup( db_name );
	ociServerHndl = NULL;
	ociErrorHndl = NULL;
	connectionOpen = false;
	idleDisconnectTid = TIMER_UNSET;
	activeSessions = new List<OciSession>();
}

OciServer::~OciServer()
{
	if ( idleDisconnectTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( idleDisconnectTid );
	}
	if ( connectionOpen ) {
		ServerDisconnect();
	}
	if ( activeSessions != NULL ) {
		delete activeSessions;
	}
	if ( dbName ) {
		free( dbName );
	}
	if ( ociServerHndl != NULL ) {
dprintf(D_ALWAYS,"*OCIHandleFree( ociServerHndl, OCI_HTYPE_SERVER )\n");
		OCIHandleFree( ociServerHndl, OCI_HTYPE_SERVER );
	}
	if ( ociErrorHndl != NULL ) {
dprintf(D_ALWAYS,"*OCIHandleFree( ociErrorHndl, OCI_HTYPE_ERROR )\n");
		OCIHandleFree( ociErrorHndl, OCI_HTYPE_ERROR );
	}
}

int OciServer::Initialize()
{
	int rc;

	if ( initDone ) {
		return OCI_SUCCESS;
	}

	if ( (rc = InitGlobalOci()) != OCI_SUCCESS ) {
		return rc;
	}

	if ( ociErrorHndl == NULL ) {
dprintf(D_ALWAYS,"*OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&ociErrorHndl, OCI_HTYPE_ERROR, 0, NULL )\n");
		rc = OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&ociErrorHndl,
							 OCI_HTYPE_ERROR, 0, NULL );
		if ( rc != OCI_SUCCESS ) {
dprintf(D_ALWAYS,"***OCIHandleAlloc failed\n");
			return rc;
		}
	}

	if ( ociServerHndl == NULL ) {
dprintf(D_ALWAYS,"*OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&ociServerHndl, OCI_HTYPE_SERVER, 0, NULL )\n");
		rc = OCIHandleAlloc( GlobalOciEnvHndl, (dvoid**)&ociServerHndl,
							 OCI_HTYPE_SERVER, 0, NULL );
		if ( rc != OCI_SUCCESS ) {
dprintf(D_ALWAYS,"***OCIHandleAlloc failed\n");
			return rc;
		}
	}

	initDone = true;

	return OCI_SUCCESS;
}

void OciServer::RegisterSession( OciSession *session, const char *username )
{
	OciSession *old_session;

	ASSERT(sessionsByUsername.lookup( HashKey(username), old_session ) == -1);

	sessionsByUsername.insert( HashKey(username), session );
}

void OciServer::UnregisterSession( OciSession *session, const char *username )
{
	if ( activeSessions->Delete( session ) ) {
		SessionInactive( session );
	}
	sessionsByUsername.remove( username );
}

OciSession *OciServer::FindSession( const char *username )
{
	OciSession *session = NULL;

	sessionsByUsername.lookup( HashKey( username ), session );

	return session;
}

int OciServer::SessionActive( OciSession *session, OCIServer *&svr_hndl,
							  OCIError *&err_hndl )
{
	int rc;

	rc = ServerConnect();
	if ( rc != OCI_SUCCESS ) {
		if ( rc == OCI_ERROR ) {
			err_hndl = ociErrorHndl;
		}
		return rc;
	}

		// TODO check if session is already in list?
	activeSessions->Append( session );

	if ( idleDisconnectTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( idleDisconnectTid );
		idleDisconnectTid = TIMER_UNSET;
	}

	svr_hndl = ociServerHndl;

	return OCI_SUCCESS;
}

void OciServer::SessionInactive( OciSession *session )
{
	activeSessions->Delete( session );

	if ( activeSessions->IsEmpty() ) {
		idleDisconnectTid = daemonCore->Register_Timer( 5,
							(TimerHandlercpp)&OciServer::IdleDisconnectHandler,
							"OciServer::IdleDisconnectHandler", this );
	}
}

int OciServer::IdleDisconnectHandler()
{
	int rc;
	OciSession *session;

	idleDisconnectTid = TIMER_UNSET;

	if ( activeSessions->IsEmpty() == false ) {
		// There are active sesssions, abort the disconnect.
		return 0;
	}

	// Notify all of our sessions about we're about to close the connection
	// to the server.
	sessionsByUsername.startIterations();

	while ( sessionsByUsername.iterate( session ) != 0 ) {
		session->ServerConnectionClosing();
	}

	rc = ServerDisconnect();
	if ( rc != OCI_SUCCESS ) {
		// TODO can we do better?
		EXCEPT( "OCIServerDetach failed!\n" );
	}

	return 0;
}

int OciServer::ServerConnect()
{
	int rc;

	if ( connectionOpen ) {
		return OCI_SUCCESS;
	}

	rc = Initialize();
	if ( rc != OCI_SUCCESS ) {
		return rc;
	}

dprintf(D_ALWAYS,"*OCIServerAttach( ociServerHndl, ociErrorHndl, (OraText *)dbName, strlen(dbName), OCI_DEFAULT)...\n");
	rc = OCIServerAttach( ociServerHndl, ociErrorHndl, (OraText *)dbName, strlen(dbName),
						  OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
dprintf(D_ALWAYS,"***OCIServerAttach failed\n");
		return rc;
	}

	connectionOpen = true;

	return OCI_SUCCESS;
}

int OciServer::ServerDisconnect()
{
	int rc;

	if ( connectionOpen == false ) {
		return OCI_SUCCESS;
	}

dprintf(D_ALWAYS,"*OCIServerDetach( ociServerHndl, ociErrorHndl, OCI_DEFAULT )...\n");
	rc = OCIServerDetach( ociServerHndl, ociErrorHndl, OCI_DEFAULT );
	if ( rc != OCI_SUCCESS ) {
dprintf(D_ALWAYS,"***OCIServerDetach failed\n");
		return rc;
	}

	connectionOpen = false;

	return OCI_SUCCESS;
}
