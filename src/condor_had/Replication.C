/*
 Replication.cpp: implementation of the HADReplication class.
*/


#include "condor_common.h"
#include "condor_state.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_api.h"
#include "condor_classad_lookup.h"
#include "condor_query.h"
#include "daemon.h"
#include "daemon_types.h"
#include "internet.h"
#include "list.h"

#include "Replication.h"
#include "StateMachine.h"
#include "ReplicaVersion.h"

// for copy_file
#include "util_lib_proto.h"


#define HAD_ADDRESS_LENGTH 28
#define NEGOTIATOR_FILE "/tmp/svekant/aaa"
#define VERSION_FILE "/tmp/svekant/vers"

/***********************************************************
  Function :
*/
HADReplication::HADReplication()
{
   //   receivedAddrVersionsList = new StringList();
      wasInitiated = false;
#if 1 // debug
      repVersion = new ReplicaVersion(VERSION_FILE,TRUE);
#endif
//      printf("HADReplication const'r\n");
}

/***********************************************************
  Function :
*/
HADReplication::~HADReplication()
{
//    if(receivedAddrVersionsList != NULL)
//        delete receivedAddrVersionsList;
    if(repVersion != NULL){
        delete repVersion;
    }

}

/***********************************************************
  Function :
*/
void
HADReplication::initialize()
{
   reinitialize();

    // Register the download reaper for the child process
    reaperDownId = daemonCore->Register_Reaper("fileTransferDownloadReaper",
            (ReaperHandler) &HADReplication::fileTransferDownloadReaper,
            "fileTransferDownloadReaper");
    
    // Register the upload reaper for the child process
    reaperUpId = daemonCore->Register_Reaper("fileTransferUploadReaper",
            (ReaperHandler) &HADReplication::fileTransferUploadReaper,
            "fileTransferUploadReaper");
}
/***********************************************************
  Function :
*/
int
HADReplication::reinitialize()
{

/*    char* tmp;
    tmp=param("HAD_REPLICATION_INTERVAL");
    if( tmp )
    {
        replicationInterval = atoi(tmp);
        free( tmp );
    } else {
        replicationInterval = REPLICATION_CYCLE;
    }
   
    if ( replicaTimerID >= 0 ) {
            daemonCore->Cancel_Timer(replicaTimerID);
    }
    // Set a timer to replication routine.
    replicaTimerID = daemonCore->Register_Timer (replicationInterval ,replicationInterval,(TimerHandlercpp) &HADReplication::replicate,
                                    "Time to replicate file", this);
    */
    if(repVersion != NULL){
        delete repVersion;
    }
    repVersion = new ReplicaVersion(VERSION_FILE);

    return TRUE;
}

/***********************************************************
  Function :       commandHandler
   REPL_FILE_VERSION  command format:   (REPL_FILE_VERSION,addr,version)
   REPL_TRANSFER_FILE command format:   (REPL_TRANSFER_FILE,addr)
   REPL_JOIN command format:   (REPL_JOIN,addr)
*/
void
HADReplication::commandHandler(int cmd,Stream *strm)
{

    char* subsys = NULL;

    strm->decode();
    if( ! strm->code(subsys) ) {
        dprintf( D_ALWAYS, "id Replication - commandHandler - Can't read subsystem name\n" );
        return;
    }
    if( ! strm->end_of_message() ) {
        dprintf( D_ALWAYS, "id Replication - commandHandler - Can't read end_of_message\n" );
        free( subsys );
        return;
    }

    dprintf( D_ALWAYS, "id %d Replication - commandHandler received cmd <%d> str : <%s>\n",daemonCore->getpid(),cmd,subsys);

    switch(cmd){
        case HAD_REPL_FILE_VERSION:
        {
             dprintf( D_ALWAYS, "id Replication - REPL_FILE_VERSION received\n");
            // in subsys is remote address ,receive version id
            char* subsysVer = NULL;

            strm->decode();
            if( ! strm->code(subsysVer) ) {
                dprintf( D_ALWAYS, "id Replication - commandHandler - Can't read subsystem name\n" );
                free( subsys );
                return;
            }
            if( ! strm->end_of_message() ) {
                dprintf( D_ALWAYS, "id Replication - commandHandler - Can't read end_of_message\n" );
                free( subsysVer );
                free( subsys );
                return;
            }

            char* versionReceived = strdup(subsysVer);
            dprintf( D_ALWAYS, "id Replication - commandHandler4 addr <%s> version <%d>\n",subsys,versionReceived);
            insertVersion(subsys,versionReceived);

            free( subsysVer );
            break;
        }
        case HAD_REPL_TRANSFER_FILE:
        {
            dprintf( D_ALWAYS, "id %d Replication - commandHandler received REPL_TRANSFER_FILE\n",daemonCore->getpid());

            sendFile(subsys);
            break;
        }
        case HAD_REPL_JOIN:
        {
            dprintf( D_ALWAYS, "id %d Replication - commandHandler received REPL_JOIN\n",daemonCore->getpid());

            // send my version
            sendVersionNumCommand(HAD_REPL_FILE_VERSION,subsys);
            break;
        }
    }

    free( subsys );
}

/***********************************************************
  Function :
*/
int
HADReplication::fileTransferDownloadReaper (Service* , int pid, int exit_status)
{

    dprintf( D_ALWAYS, "HADReplication::fileTransferDownloadReaper exit status : <%d>\n",exit_status);
    // rotate file
    //char filename[_POSIX_PATH_MAX];
    //sprintf(filename,"%s_%s",NEGOTIATOR_FILE,versionToString(versionToDownLoad));
    //if(rotate_file(filename,NEGOTIATOR_FILE) < 0)
    //    return FALSE;

    return TRUE;
}

/***********************************************************
  Function :
*/
int
HADReplication::fileTransferUploadReaper (Service* , int pid, int exit_status)
{
    dprintf( D_ALWAYS, "HADReplication::fileTransferUploadReaper exit status : <%d>\n",exit_status);
    return 1;
}

/***********************************************************
  Function :
*/
void
HADReplication::replicate()
{
        // send my version to all
         sendCommandToOthers(HAD_REPL_FILE_VERSION);

        // check buffers and pick the best version
        // if there is the better version than mine , get the file
        checkReceivedVersions();

}



/***********************************************************
  Function :
*/ /*
int  HADReplication::calculateOwnVersion(){

         return (int)(100000 - daemonCore->getpid());
} */
/***********************************************************
  Function :
*/
int
HADReplication::sendFile(char* addr)
{
    ReliSock* sock = new ReliSock();
    if(sock == NULL){
        return FALSE;
    }

    if(!sock->connect(addr,0,true)){
        dprintf(D_ALWAYS,"id %d , cannot connect to addr\n",daemonCore->getpid(),addr);
        sock->close();
        return FALSE;
    }

    // send file
    Upload(sock);
    //sock->close();
    return TRUE;
}

/***********************************************************
  Function :
*/
int
HADReplication::receiveFile(char* addr)
{
     ReliSock* s = sendTransferCommand(addr);

     if(s == NULL) {
        return FALSE;
     }
     return Download(s);
}


/***********************************************************
  Function :
*/
int
HADReplication::sendCommandToOthers(int cmd)
{

    dprintf( D_ALWAYS, "id %d , send %d command  \n",daemonCore->getpid(),cmd);

    char* addr;

    otherHADIPs->rewind();
    while( (addr = otherHADIPs->next()) ) {
      
        if(cmd == HAD_REPL_TRANSFER_FILE){
                   sendTransferCommand(addr);
        }else if(cmd == HAD_REPL_JOIN){
                   sendJoinCommand(cmd,addr);
        }else if(cmd == HAD_REPL_FILE_VERSION){
                   sendVersionNumCommand(cmd,addr);
        }

    }
    return TRUE;
}


/***********************************************************
  Function :
*/
int
HADReplication::sendJoinCommand(int cmd,char* addr)
{
    dprintf( D_ALWAYS, "id %d , send %d (HAD_REPL_JOIN)command to %s\n",daemonCore->getpid(),cmd,addr);

    Daemon d( DT_ANY, addr );
    ReliSock sock;

    if(!sock.connect(addr,0,true)){
        dprintf(D_ALWAYS,"id %d , cannot connect to addr %s\n",daemonCore->getpid(),addr);
        sock.close();
        return FALSE;
    }

    // startCommand - max timeout is 1 sec
    if(! (d.startCommand(cmd,&sock,1))){
        dprintf(D_ALWAYS,"id %d , cannot start command %s\n",daemonCore->getpid(),addr);
        sock.close();
        return FALSE;
    }

    char stringId[HAD_ADDRESS_LENGTH];
    sprintf(stringId,"%s",daemonCore->InfoCommandSinfulString());
    char* subsys = (char*)stringId;

    if(!sock.code(subsys) || !sock.eom()){
        dprintf(D_ALWAYS,"id %d , sock.code false \n",daemonCore->getpid());
        sock.close();
        return FALSE;
    } else {
        dprintf(D_ALWAYS,"id %d , sock.code true \n",daemonCore->getpid());
        sock.close();
        return TRUE;
    }


}


/***********************************************************
  Function :
*/
int
HADReplication::sendVersionNumCommand(int cmd,char* addr)
{
    dprintf( D_ALWAYS, "id %d , send %d command to %s\n",daemonCore->getpid(),cmd,addr);

    Daemon d( DT_ANY, addr );
    ReliSock sock;

    if(!sock.connect(addr,0,true)){
        dprintf(D_ALWAYS,"id %d , cannot connect to addr %s\n",daemonCore->getpid(),addr);
        sock.close();
        return FALSE;
    }

    // startCommand - max timeout is 1 sec
    if(!d.startCommand(cmd,&sock,1)){
        dprintf(D_ALWAYS,"id %d , cannot start command addr %s\n",daemonCore->getpid(),addr);
        sock.close();
        return FALSE;
    }


    // send addr
    char stringAddr[HAD_ADDRESS_LENGTH];
    sprintf(stringAddr,"%s",daemonCore->InfoCommandSinfulString());
    char* subsys = (char*)stringAddr;

    if(!sock.code(subsys) || !sock.eom()){
        dprintf(D_ALWAYS,"id %d , sock.code false \n",daemonCore->getpid());
        sock.close();
        return FALSE;
    }else{
        dprintf(D_ALWAYS,"id %d , sock.code true \n",daemonCore->getpid());
    }


    // send versionId
    //char stringVersion[10];
    //sprintf(stringVersion,"%d",calculateOwnVersion());

    subsys = strdup(repVersion->versionToSendString());

    if(!sock.code(subsys) || !sock.eom()){
        dprintf(D_ALWAYS,"id %d , sock.code false \n",daemonCore->getpid());
        sock.close();
        return FALSE;
    }else{
        dprintf(D_ALWAYS,"id %d , sock.code true \n",daemonCore->getpid());
    }
    sock.close();
    return TRUE;
}

/***********************************************************
  Function :
    my ipaddr+port : daemonCore->InfoCommandSinfulString()
*/
ReliSock*
HADReplication::sendTransferCommand(char* addr){
    dprintf( D_ALWAYS, "id %d , send %d command(transfer) to %s\n",daemonCore->getpid(),HAD_REPL_TRANSFER_FILE,addr);

    Daemon d( DT_ANY, addr );
    ReliSock sock;

    if(!sock.connect(addr,0,true)){
        dprintf(D_ALWAYS,"id %d , cannot connect to addr %s\n",daemonCore->getpid(),addr);
        sock.close();
        return NULL;
    }

    // startCommand - max timeout is 1 sec
    if(!d.startCommand(HAD_REPL_TRANSFER_FILE,&sock,1)){
        dprintf(D_ALWAYS,"id %d , cannot start command to addr %s\n",daemonCore->getpid(),addr);
        sock.close();
        return NULL;
    }

    char stringId[HAD_ADDRESS_LENGTH];

    // find and bind port
    ReliSock transfer_sock;

    if(!transfer_sock.bind()){
        sock.close();
        return NULL;
    }

    if(!transfer_sock.listen()){
        sock.close();
        return NULL;
    }

    sprintf(stringId,"<%s:%d>",getHostFromAddr(daemonCore->InfoCommandSinfulString()),transfer_sock.get_port());

    char* subsys = (char*)stringId;
    if(!sock.code(subsys) || !sock.eom()){
        dprintf(D_ALWAYS,"id %d , sock.code false \n",daemonCore->getpid());
        sock.close();
        return NULL;
    } else {
        dprintf(D_ALWAYS,"id %d , sock.code true \n",daemonCore->getpid());
    }

    sock.close();

    ReliSock *s = transfer_sock.accept();
    return s;
}

/***********************************************************
  Function :
*/
void
HADReplication::initializeFile(StringList* hadsList,int hadInterval)
{
    // send to all "join" cmd

    otherHADIPs = hadsList;
    sendCommandToOthers(HAD_REPL_JOIN);

    //waitingVersionsTimerID = daemonCore->Register_Timer ( 2*hadInterval,0,(TimerHandlercpp) &HADReplication::checkReceivedVersions,
    //                                "Time to check received versions", this);

}

/***********************************************************
  Function :
*/
void
HADReplication::checkReceivedVersions()
{

    daemonCore->Cancel_Timer (waitingVersionsTimerID) ;

    // check the versions list and get the better
    char* addr = new char[HAD_ADDRESS_LENGTH];
    ReplicaVersion vers = getBestVersion(addr);

    if(vers <= *repVersion){
        wasInitiated = true;
        free( addr );
        return;
    }

    versionToDownLoad = strdup(vers.versionToSendString());

    // transfer file
    receiveFile(addr);
    free( addr );

    wasInitiated = true;
}



/***********************************************************
  Function :
*/
ReplicaVersion
HADReplication::getBestVersion(char* addr)
{

/*    int myVer = calculateOwnVersion();

    int otherVer;
    char* otherAddr = new char[HAD_ADDRESS_LENGTH];

    int maxVer = myVer;
    strcpy(addr,daemonCore->InfoCommandSinfulString());

    receivedVersionsList.Rewind();
    receivedAddrVersionsList->rewind();

    while(receivedVersionsList.Next(otherVer) && (otherAddr = receivedAddrVersionsList->next())) {
		if(otherVer > maxVer) {
              maxVer =  otherVer;
              strcpy(addr,otherAddr);
        }
	}

    free( otherAddr );
    return  maxVer;
*/

//    ReplicaVersion otherVer;
//    char* otherAddr = new char[HAD_ADDRESS_LENGTH];

    ReplicaVersion maxVer;
   /*
    receivedVersionsList.Rewind();
    receivedAddrVersionsList.rewind();

    while(receivedVersionsList.Next(otherVer) && (otherAddr = receivedAddrVersionsList.next())) {
		if(otherVer > maxVer) {
              maxVer =  otherVer;
              strcpy(addr,otherAddr);
        }
	}
    free( otherAddr );
*/
    return maxVer;

}



/***********************************************************
  Function :
*/
void
HADReplication::insertVersion(char* addr,char* version)
{

             /*
    ReplicaVersion* otherVer;
    char* otherAddr = new char[HAD_ADDRESS_LENGTH];

    receivedVersionsList.Rewind();
    receivedAddrVersionsList.rewind();
    while(receivedVersionsList.Next(otherVer) && (otherAddr = receivedAddrVersionsList.next())) {
		if(strcmp(otherAddr,addr) == 0) {
                receivedVersionsList.DeleteCurrent();
                receivedAddrVersionsList.deleteCurrent();
                delete otherAddr;
                delete otherVer;
        }
	}

    ReplicaVersion* alloc_ver = new ReplicaVersion(version);
    receivedVersionsList.Append(alloc_ver);

    char* alloc_addr = new char[HAD_ADDRESS_LENGTH];
    strcpy(alloc_addr,addr);
    receivedAddrVersionsList.append(alloc_addr);

    free(otherAddr);

          */

}



/////////////////////////////////////////////////////////////////////////////////
//
//
/////////////////////////////////////////////////////////////////////////////////

/***********************************************************
  Function :
*/
int
HADReplication::Download(ReliSock *s)
{
    dprintf(D_FULLDEBUG,"entering FileTransfer::Download\n");
/*
    struct paramThread *params = (struct paramThread *)malloc(sizeof(struct paramThread));
    params->myObj = this;
    params->version = versionToDownLoad;

    int activeTransferTid = daemonCore->
        Create_Thread((ThreadStartFunc)&HADReplication::DownloadThread,
        (void *)params, s, reaperDownId);

    if(activeTransferTid == FALSE) {
        dprintf(D_ALWAYS,"Failed to create FileTransfer DownloadThread!\n");
        return FALSE;
    }
*/
    char* version_str = repVersion->versionToSendString();
    int len = strlen("condor_replica_down") + strlen(NEGOTIATOR_FILE) + strlen(version_str) + 2*strlen(" ");
    char args[len];
    sprintf(args,"condor_replica_down %s %s",NEGOTIATOR_FILE,version_str);
    free( version_str );
    
    int ActiveTransferTid = daemonCore->Create_Process (
        "condor_replica_down",   // name
        args,  // args
        PRIV_UNKNOWN,
        reaperDownId,
        TRUE,
        NULL,
        NULL,
        FALSE,
        (Stream**)&s                       // socket to inherit
        );
    if (ActiveTransferTid == FALSE) {
        dprintf(D_ALWAYS, "Failed to create FileTransfer upload process!\n");
        return FALSE;
    }
    return TRUE;
}

/***********************************************************
  Function :
*/ /*
int
HADReplication::DownloadThread(void *arg, Stream *s)
{
    dprintf(D_FULLDEBUG,"entering FileTransfer::DownloadThread\n");
    struct paramThread* params = (struct paramThread*)arg;

    int total_bytes = params->myObj->DoDownload((ReliSock *)s);

    // rotate file
    char filename[_POSIX_PATH_MAX];
    //char* version_str = versionToString(params->version);
    sprintf(filename,"%s.%s",NEGOTIATOR_FILE,params->version);
    free( params->version );

    // for debug - change negot.filename
    char negfilename[_POSIX_PATH_MAX];
    sprintf(negfilename,"%s_down",NEGOTIATOR_FILE);

    int fd;
    if((fd = open(filename,O_RDONLY)) < 0){
        dprintf(D_FULLDEBUG,"FileTransfer::DownloadThread file <%s> doesn't exist\n",filename);
        return FALSE;
    }else{
        close(fd);
    }

    if(rotate_file(filename,negfilename) < 0)
    //if(rotate_file(filename,NEGOTIATOR_FILE) < 0)
        return FALSE;

    ((ReliSock*)s)->close();
    delete s;
    delete params;
    return (total_bytes >= 0);
}
   */

/*
  Define a macro to restore our priv state (if needed) and return.  We
  do this so we don't leak priv states in functions where we need to
  be in our desired state.
 */

/***********************************************************
  Function :
*/ /*
int
HADReplication::DoDownload(ReliSock *s)
{
    int reply;
    filesize_t bytes, total_bytes = 0;
    char filename[_POSIX_PATH_MAX];

    //char* vresion_str = versionToString(versionToDownLoad);
    sprintf(filename,"%s.%s",NEGOTIATOR_FILE,versionToDownLoad);
    //free( vresion_str );

    // check if exist, in such case don't download
    int fd;
    if((fd = open(filename,O_RDONLY)) >= 0){
        close(fd);
        return TRUE;
    }


    dprintf(D_FULLDEBUG,"entering FileTransfer::DoDownload sync=\n");
    s->decode();

    for (;;) {
        if( !s->code(reply) ) {
            return FALSE;
        }
        if( !s->end_of_message() ) {
            return FALSE;
        }
        if( !reply ) {
            break;
        }

        if( s->get_file( &bytes, filename ) < 0 ) {
            return FALSE;
        }

        if( !s->end_of_message() ) {
            return FALSE;
        }
        total_bytes += bytes;
    }

    dprintf(D_ALWAYS,"FileTransfer::DoDownload received %d bytes\n",total_bytes);
    return TRUE;
}

 */

/***********************************************************
  Function :
*/
int
HADReplication::Upload(ReliSock *s)
{
    dprintf(D_FULLDEBUG,"entering FileTransfer::Upload\n");
/*
    int ActiveTransferTid = daemonCore->
        Create_Thread((ThreadStartFunc)&HADReplication::UploadThread,
            (void *)this, s, reaperUpId);
    if (ActiveTransferTid == FALSE) {
        dprintf(D_ALWAYS, "Failed to create FileTransfer UploadThread!\n");
        return FALSE;
    }
*/
    char* version_str = repVersion->versionToSendString();
    int len = strlen("condor_replica_up") + strlen(NEGOTIATOR_FILE) + strlen(version_str) + 2*strlen(" ");
    char args[len];
    sprintf(args,"condor_replica_up %s %s",NEGOTIATOR_FILE,version_str);
    free( version_str );
    
    int ActiveTransferTid = daemonCore->Create_Process (
        "condor_replica_up",   // name
        args,  // args
        PRIV_UNKNOWN,
        reaperUpId,
        TRUE,
        NULL,
        NULL,
        FALSE,
        (Stream**)&s                       // socket to inherit
        );
    if (ActiveTransferTid == FALSE) {
        dprintf(D_ALWAYS, "Failed to create FileTransfer upload process!\n");
        return FALSE;
    }
    return TRUE;
}

/***********************************************************
  Function :
*/ /*
int
HADReplication::UploadThread(void *arg, Stream *s)
{
    dprintf(D_FULLDEBUG,"entering FileTransfer::UploadThread\n");

    filesize_t	total_bytes;
    HADReplication* myobj = (HADReplication*)arg;
    int status = myobj->DoUpload( &total_bytes, (ReliSock *)s);	

    ((ReliSock*)s)->close();
    delete s;
    return status;
}

/***********************************************************
  Function :
*/  /*
int
HADReplication::DoUpload(filesize_t *total_bytes, ReliSock *s)
{

    filesize_t bytes;
    *total_bytes = 0;
    dprintf(D_FULLDEBUG,"entering FileTransfer::DoUpload\n");

    s->encode();

    char filename[_POSIX_PATH_MAX];
    memset(filename,0,_POSIX_PATH_MAX);

    char *version_str = repVersion->versionToSendString();
    sprintf(filename,"%s%s",NEGOTIATOR_FILE,version_str);
    free( version_str );

    dprintf(D_FULLDEBUG,"debug orig : <%s> filename : <%s>\n",NEGOTIATOR_FILE,filename);

    // check if exist
    int fd;
    if((fd = open(filename,O_RDONLY)) < 0){
        dprintf(D_FULLDEBUG,"debug orig : cannot open file_vers<%s>\n",filename);

        if( copy_file(NEGOTIATOR_FILE, filename) ){
               dprintf(D_FULLDEBUG,"debug orig : cannot copy yo file_vers\n") ;
        }
    }
    else{
       close(fd) ;
    }

    dprintf(D_FULLDEBUG,"DoUpload: send file %s\n",filename);

    if( !s->snd_int(1,FALSE) ) {
        dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);
        return FALSE;
    }
    if( !s->end_of_message() ) {
        dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);
        return FALSE;
    }

    if( s->put_file( &bytes, filename ) < 0 ) {
        EXCEPT("DoUpload: Failed to send file %s, exiting at %d\n",
            filename,__LINE__);
        return FALSE;
    }

    if( !s->end_of_message() ) {
        dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);
        return FALSE;
    }

    *total_bytes += bytes;


    // tell our peer we have nothing more to send
    s->snd_int(0,TRUE);

    dprintf(D_FULLDEBUG,"DoUpload: exiting at %d\n",__LINE__);


    return FALSE;
}
   */
















