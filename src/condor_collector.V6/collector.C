#include "condor_common.h"
#include "condor_config.h"
#include "collector.h"

static char *_FileName_ = __FILE__;

CollectorDaemon::
CollectorDaemon( )
{
	condorDevelopersCollector 	= NULL;
	condorDevelopers 			= NULL;
	collectorName 				= NULL;
	condorAdmin 				= NULL;
	updateTimerID 				= -1;
	expiredAdsID 				= -1;
}

CollectorDaemon::
~CollectorDaemon( )
{
}


void CollectorDaemon::
Init( )
{
	StringSet	partitionAttr;

		// register command handlers
	daemonCore->Register_Command( UPDATE_PUBLIC_AD, "UPDATE_PUBLIC_AD", 
		(CommandHandlercpp)receive_modify, "receive_modify", this, READ );
	daemonCore->Register_Command( UPDATE_PRIVATE_AD, "UPDATE_PRIVATE_AD", 
		(CommandHandlercpp)receive_modify, "receive_modify", this, READ );
	daemonCore->Register_Command( MODIFY_PUBLIC_AD, "MODIFY_PUBLIC_AD", 
		(CommandHandlercpp)receive_modify, "receive_modify", this, READ );
	daemonCore->Register_Command( MODIFY_PRIVATE_AD, "MODIFY_PRIVATE_AD", 
		(CommandHandlercpp)receive_modify, "receive_modify", this, READ );

	daemonCore->Register_Command( QUERY_PUBLIC_ADS, "QUERY_PUBLIC_ADS", 
		(CommandHandlercpp)receive_query, "receive_query", this, READ );
	daemonCore->Register_Command( QUERY_PRIVATE_ADS, "QUERY_PRIVATE_ADS", 
		(CommandHandlercpp)receive_query, "receive_query", this, READ );

		// create views (sub-collections)
	partitionAttr.Insert( ATTR_TYPE );
	if((publicPartitionID=publicColl.CreatePartition(0,"0",partitionAttr))<0||
	  (privatePartitionID=privateColl.CreatePartition(0,"0",partitionAttr))<0){
		EXCEPT( "Error:  Could not instantiate partition\n" );
	}

	Config( );
}


void CollectorDaemon::
Config( )
{
	char	*tmp;

	#define PARAMINT(p,v,d) if((tmp=param(p))){v=atoi(tmp);free(tmp);}else{v=d;}

		// integer parameters
	PARAMINT( "CLIENT_TIMEOUT", clientTimeout, 30 );
	PARAMINT( "QUERY_TIMEOUT", queryTimeout, 60 );
	PARAMINT( "MASTER_CHECK_INTERVAL", masterCheckInterval, 0 );
	PARAMINT( "CLASSAD_LIFETIME", classadLifetime, 900 );

		// get email address of (local) condor admin
	if( condorAdmin ) free( condorAdmin );
	if( ( condorAdmin = param( "CONDOR_ADMIN" ) ) == NULL ) {
		EXCEPT( "Parameter 'CONDOR_ADMIN' not found in config file." );
	}

		// get email address of condor developers
	if( ( tmp = param( "CONDOR_DEVELOPERS" ) ) == NULL ) {
		tmp = strdup( "condor-admin@cs.wisc.edu" );
	} else if( strcasecmp( tmp, "NONE" ) == 0 ) {
		free( tmp );
		tmp = NULL;
	}
	if( condorDevelopers ) free( condorDevelopers );
	condorDevelopers = tmp;


		// get collector (pool) name
	if( collectorName ) free( collectorName );
	collectorName = param( "COLLECTOR_NAME" );


		// collector update information
	if( ( tmp = param( "CONDOR_DEVELOPERS_COLLECTOR" ) ) == NULL ) {
		tmp = strdup( "condor.cs.wisc.edu" );
	} else if( strcasecmp( tmp, "NONE" ) == 0 ) {
		free( tmp );
		tmp = NULL;
	}
	if( condorDevelopersCollector ) free( condorDevelopersCollector );
	condorDevelopersCollector = tmp;
	if( updateTimerID >= 0 ) {
		daemonCore->Cancel_Timer( updateTimerID );
		updateTimerID = -1;
	}
	updateSock.close( );
	PARAMINT( "COLLECTOR_UPDATE_INTERVAL", collectorUpdateInterval, 900 );
	if( condorDevelopersCollector && collectorUpdateInterval > 0 ) {
		if(updateSock.connect(condorDevelopersCollector,COLLECTOR_PORT)==TRUE){
			if( updateTimerID >= 0 ) {
				daemonCore->Cancel_Timer( updateTimerID );
			}
			updateTimerID = daemonCore->Register_Timer( 1, 
				collectorUpdateInterval, (TimerHandlercpp)sendCollectorAd,
				"sendCollectorAd", this );
		}
	}

		// classad expressions to remove stale classads
	char	buffer[1024];
	expireQuery.Clear( );
	sprintf( buffer, "Now - other.%s", ATTR_LAST_HEARD_FROM );
	if( !expireQuery.Insert( "ElapsedTime", buffer ) ) {
		EXCEPT( "Failed processing expression: %s", buffer );
	}
	sprintf(buffer,"ElapsedTime > MakeRelTime(other.%s)",ATTR_UPDATE_INTERVAL);
	if( !expireQuery.Insert( "Suicide", buffer ) ) {
		EXCEPT( "Failed processing expression: %s", buffer );
	}
	sprintf( buffer, "ElapsedTime > MakeRelTime(%d)", classadLifetime );
	if( !expireQuery.Insert( "Homicide", buffer ) ) {
		EXCEPT( "Failed processing expression: %s", buffer );
	}
	expireQuery.Insert(ATTR_REQUIREMENTS,"Isboolean(Suicide)?Suicide:Homicide");

		// timer to remove expired classads
	if( expiredAdsID >= 0 ) {
		daemonCore->Cancel_Timer( expiredAdsID );
	}
	expiredAdsID = daemonCore->Register_Timer( classadLifetime, classadLifetime,
		(TimerHandlercpp)removeExpiredAds, "removeExpiredAds", this );

}


void CollectorDaemon::
Exit( )
{
	return;
}


void CollectorDaemon::
Shutdown( )
{
	return;
}


int CollectorDaemon::
receive_query( int cmd, Stream *strm )
{
	ClassAdCollection	*coll;
	ClassAd 			*ad=NULL;
	ClassAd				*rep;
	Sink				snk;
	int					coID;
	CedarSource			cSrc;
	CedarSink			cSnk;
	Value				val;
	int					partitionID;

	if( cmd == QUERY_PUBLIC_ADS ) {
		coll = &publicColl;
		partitionID = publicPartitionID;
	} else if( cmd == QUERY_PRIVATE_ADS ) {
		coll = &privateColl;
		partitionID = privatePartitionID;
	} else {
		dprintf( D_ALWAYS, "Unknown command %d\n", cmd );
		return( FALSE );
	}

	strm->decode( );
	strm->timeout( queryTimeout );
	cSrc.Initialize( strm );
	src.SetSource( &cSrc );
	if( !src.ParseClassAd( ad ) || !strm->end_of_message( ) ) {
		dprintf( D_ALWAYS, "Failed parsing query classad\n" );
		return( FALSE );
	}
	if(!ad->EvaluateAttr(ATTR_COLLECTION_HINTS,val)||!val.IsClassAdValue(rep)){
		dprintf( D_ALWAYS, "Unable to determine collection to query\n" );
		delete ad;
		return( FALSE );
	}
	if( ( coID = coll->FindPartition( partitionID, rep ) ) < 0 ) {
		dprintf( D_ALWAYS, "Unable to find collection to query\n" );
		delete ad;
		return( FALSE );
	}

	strm->encode( );
	cSnk.Initialize( strm );
	snk.SetSink( &cSnk );
	coll->QueryCollection( coID, ad, snk );
	snk.FlushSink( );
	strm->end_of_message( );
	delete ad;
	return( TRUE );
}


int CollectorDaemon::
receive_modify( int cmd, Stream *strm )
{
	ClassAdCollection	*coll;
	ClassAd				*ad=NULL;
	CedarSource			cSrc;
	char				type[128], name[128];
	char				key[512];
	int					alen;

	if( cmd == MODIFY_PUBLIC_AD || cmd == UPDATE_PUBLIC_AD ) {
		coll = &publicColl;
	} else if( cmd == MODIFY_PRIVATE_AD || cmd == UPDATE_PRIVATE_AD ) {
		coll = &privateColl;
	} else {
		dprintf( D_ALWAYS, "Unknown command %d\n", cmd );
		return( FALSE );
	}

	strm->decode( );
	strm->timeout( clientTimeout );
	cSrc.Initialize( strm );
	src.SetSource( &cSrc );
	if( !src.ParseClassAd( ad ) || !strm->end_of_message( ) ) {
		dprintf( D_ALWAYS, "Failed parsing modify classad\n" );
		return( FALSE );
	}
	if( !ad->EvaluateAttrString( ATTR_TYPE, type, 128, alen ) ||
		!ad->EvaluateAttrString( ATTR_NAME, name, 128, alen ) ) {
		dprintf( D_ALWAYS, "Unable to construct key from classad\n" );
		delete ad;
		return( FALSE );
	}
	sprintf( key, "%s::%s", type, name );

	if( cmd == MODIFY_PUBLIC_AD || cmd == MODIFY_PRIVATE_AD ) {
		ad->DeepInsert( ATTR_UPDATES, ATTR_LAST_HEARD_FROM,
			Literal::MakeAbsTime( ) );
		coll->ModifyClassAd( key, ad );
	} else {
		ad->Insert( ATTR_LAST_HEARD_FROM, Literal::MakeAbsTime( ) );
		coll->UpdateClassAd( key, ad );
	}
	dprintf( D_ALWAYS, "Received classad from %s\n", key );

	return( TRUE );
}


int CollectorDaemon::
sendCollectorAd( )
{
	dprintf( D_ALWAYS, "Pretend that I sent the collector ad\n" );
	return( 1 );
}


int CollectorDaemon::
removeExpiredAds( )
{
	CollConstrContentItor	itor;
	char					key[512];
	ClassAd 				*query;
	
	dprintf( D_ALWAYS, "Removing stale public ads ...\n" );
	query = expireQuery.Copy( );
	query->Insert( "Now", Literal::MakeAbsTime( ) );

	itor.RegisterQuery( query );
	publicColl.InitializeIterator( 0, &itor );

	while( !itor.AtEnd( ) ) {
		if( itor.CurrentAdKey( key ) ) {
			dprintf( D_ALWAYS, " * Removing stale ad: %s\n", key );
			publicColl.DestroyClassAd( key );
			if( !itor.IteratorMoved( ) ) {
				itor.NextAd( );
			}
		}
	}


	dprintf( D_ALWAYS, "Removing stale private ads ...\n" );
	query = expireQuery.Copy( );
	query->Insert( "Now", Literal::MakeAbsTime( ) );

	itor.RegisterQuery( query );
	privateColl.InitializeIterator( 0, &itor );

	while( !itor.AtEnd( ) ) {
		if( itor.CurrentAdKey( key ) ) {
			dprintf( D_ALWAYS, " * Removing stale ad: %s\n", key );
			privateColl.DestroyClassAd( key );
			if( !itor.IteratorMoved( ) ) {
				itor.NextAd( );
			}
		}
	}

	return( TRUE );
}


void CollectorDaemon::
checkMaster( ClassAd *ad )
{
	char	key[512], machine[128];
	int		alen;
	ClassAd	*mad;

		// construct master's key
	if( !ad->EvaluateAttrString( ATTR_MACHINE, machine, 128, alen ) ) {
		dprintf( D_ALWAYS,"Could not evaluate attribute '%s'\n",ATTR_MACHINE );
		return;
	}
	sprintf( key, "%s::%s", MASTER_ADTYPE, machine );

		// check if master has a classad
	if( ( mad = publicColl.LookupClassAd( key ) ) == NULL ) {
		dprintf(D_ALWAYS,"Achtung! Master ad %s not found (orphaned daemons)\n",
			key );
	}
}
