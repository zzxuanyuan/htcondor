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

template class SimpleList<ReplicaVersion>;
char* processArgs;

/***********************************************************
  Function :
*/
HADReplication::HADReplication()
{
      wasInitiated = false;
      repVersion = NULL;
      versionFilePath = NULL;
      releaseDirPath = NULL;
      accountFilePath = NULL;
      reaperDownId = -1;
      reaperUpId = -1;
}

/***********************************************************
  Function :
*/
HADReplication::~HADReplication()
{
    finilize();
}

/***********************************************************
  Function :
*/
void
HADReplication::finilize(){
    if(repVersion != NULL){
        delete repVersion;
    }
    if ( versionFilePath != NULL){
      free( versionFilePath );
      versionFilePath = NULL;
    }
    if( releaseDirPath != NULL ){
      free(releaseDirPath);
      releaseDirPath = NULL;
    }
    if( accountFilePath != NULL ){
      free(accountFilePath);
      accountFilePath = NULL;
    }
}

/***********************************************************
  Function :
*/
void
HADReplication::initialize()
{
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
 
    finilize();
    
    char* tmp;
    tmp=param( "SPOOL" );
    if( tmp ){
        versionFilePath = (char*)malloc(strlen(tmp) + 1 + strlen("/AccountVers"));
        sprintf(versionFilePath,"%s/AccountVers",tmp);

        accountFilePath = (char*)malloc(strlen(tmp) + 1 + strlen("/aaa"));
        sprintf(accountFilePath,"%s/aaa",tmp);
        
        free( tmp );
    } else {
        return FALSE;
    }

    tmp=param( "RELEASE_DIR" );
    if( tmp ){
        releaseDirPath = (char*)malloc(strlen(tmp) + 1 + strlen("/bin"));
        sprintf(releaseDirPath,"%s/bin",tmp);
        free( tmp );
    } else {
        return FALSE;
    }
    
    repVersion = new ReplicaVersion(versionFilePath,TRUE);
 
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
            dprintf( D_ALWAYS, "id Replication - commandHandler4 addr <%s> version <%s>\n",subsys,versionReceived);
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
HADReplication::fileTransferDownloadReaper (Service* ser,int pid, int exit_status)
{

    dprintf( D_ALWAYS, "HADReplication::fileTransferDownloadReaper exit status : <%d>\n",exit_status);
    if(processArgs != NULL){
        free( processArgs );
    }
    dprintf( D_ALWAYS, "HADReplication::fileTransferDownloadReaper update version\n");
    ((HADStateMachine*)ser)->updateReplicaVersion();
    return exit_status;
}

/***********************************************************
  Function :
*/
int
HADReplication::fileTransferUploadReaper (Service* , int pid, int exit_status)
{
    if(processArgs != NULL){
        free( processArgs );
    }
    return exit_status;
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
        checkReceivedVersions();

}

/***********************************************************
  Function :
*/
void
HADReplication::updateReplicaVersion()
{
  repVersion->fetch();
}

/***********************************************************
  Function :
*/
int
HADReplication::sendFile(char* addr)
{
    dprintf( D_ALWAYS, "sendFile() to addr %s\n",addr);
    ReliSock* sock = new ReliSock();  // close and delete sock in the transfer process
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
    dprintf( D_ALWAYS, "sendFile() - after upload\n");
    return TRUE;
}

/***********************************************************
  Function :
*/
int
HADReplication::receiveFile(char* addr)
{
     dprintf( D_ALWAYS, "receiveFile() from addr %s\n",addr);
     ReliSock* s = sendTransferCommand(addr);  // close and delete sock in the transfer process

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
    sock.timeout( SEND_COMMAND_TIMEOUT );
    
    if(!sock.connect(addr,0,true)){
        dprintf(D_ALWAYS,"id %d , cannot connect to addr %s\n",daemonCore->getpid(),addr);
        sock.close();
        return FALSE;
    }

    // startCommand - max timeout is 1 sec
    if(! (d.startCommand(cmd,&sock,SEND_COMMAND_TIMEOUT))){
        dprintf(D_ALWAYS,"id %d , cannot start command %d to %s\n",daemonCore->getpid(),cmd,addr);
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

    subsys = strdup(repVersion->versionToSendString());

    if(!sock.code(subsys) || !sock.eom()){
        dprintf(D_ALWAYS,"id %d , sock.code false \n",daemonCore->getpid());
        sock.close();
        return FALSE;
    }else{
        dprintf(D_ALWAYS,"id %d , sock.code true \n",daemonCore->getpid());
    }
    sock.close();
    dprintf(D_ALWAYS,"Debug, sendVersionNumCommand vers is <%s>\n",subsys);
    free(subsys);
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

    // accept() do new()
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
}

/***********************************************************
  Function :
*/
void
HADReplication::checkReceivedVersions() 
{

    // check the versions list and get the better
    char* addr = new char[HAD_ADDRESS_LENGTH];
    
    ReplicaVersion vers = getBestVersion(addr);

    if(*repVersion >= vers){ 
        wasInitiated = true;
        delete [] addr ;
        return;
    }

    versionToDownLoad = strdup(vers.versionToSendString());

    // transfer file
    receiveFile(addr);
    delete [] addr ;

    wasInitiated = true;
}



/***********************************************************
  Function :
*/
ReplicaVersion
HADReplication::getBestVersion(char* addr)  
{
    ReplicaVersion otherVer;
    char* otherAddr = new char[HAD_ADDRESS_LENGTH];
    
    strcpy(addr,daemonCore->InfoCommandSinfulString());    
    ReplicaVersion maxVer(*repVersion);
   
    receivedVersionsList.Rewind();
    receivedAddrVersionsList.rewind();

    while(receivedVersionsList.Next(otherVer) && (otherAddr = receivedAddrVersionsList.next())) {
		if(otherVer > maxVer) {
              maxVer =  otherVer;
              strcpy(addr,otherAddr);
        }
	}
    
    delete [] otherAddr;
    return maxVer;

}



/***********************************************************
  Function :
*/
void
HADReplication::insertVersion(char* addr,char* version)
{

             
    ReplicaVersion otherVer;
    char* otherAddr = new char[HAD_ADDRESS_LENGTH];
 
    receivedVersionsList.Rewind();
    receivedAddrVersionsList.rewind();
    while(receivedVersionsList.Next(otherVer) && (otherAddr = receivedAddrVersionsList.next())) {
		if(strcmp(otherAddr,addr) == 0) {
                receivedVersionsList.DeleteCurrent();
                receivedAddrVersionsList.deleteCurrent();       
        }
	}
    delete [] otherAddr;
    ReplicaVersion alloc_ver(version);
    receivedVersionsList.Append(alloc_ver);

    receivedAddrVersionsList.append(addr);
           

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

    char* version_str = repVersion->versionToSendString();
    int len = strlen("condor_replica_down") + strlen(accountFilePath) +
                strlen(versionFilePath) +
                strlen(version_str) + 2*strlen(" ") +
                strlen(releaseDirPath) + strlen("/") + 1;
    if(processArgs != NULL){
      free( processArgs );
    }
    processArgs = NULL;
    processArgs = (char*)malloc(len);
    sprintf(processArgs,"%s/condor_replica_down %s %s %s",releaseDirPath,
            accountFilePath,versionFilePath,version_str);
    free( version_str );

    len = strlen("condor_replica_down") + strlen(releaseDirPath) + strlen("/") + 1;
    char executable[len];
    sprintf(executable,"%s/condor_replica_down",releaseDirPath);

   
    ReliSock* inherited_sockets[2];
     inherited_sockets[0] = s;
     inherited_sockets[1] = NULL;
     int ActiveTransferTid = daemonCore->Create_Process (
        executable,   // name
        processArgs,  // args
        PRIV_UNKNOWN,
        reaperUpId,
        TRUE,
        NULL,
        NULL,
        FALSE,
        (Stream**)inherited_sockets                       // socket to inherit
        );
   
    if (ActiveTransferTid == FALSE) {
        dprintf(D_ALWAYS, "Failed to create FileTransfer upload process!\n");
        return FALSE;
    }
    return TRUE;
}



/***********************************************************
  Function :
*/
int
HADReplication::Upload(ReliSock *s)
{
  

    char* version_str = repVersion->versionToSendString();
    int len = strlen("condor_replica_up") + strlen(accountFilePath) +
                strlen(versionFilePath) +
                strlen(version_str) + 2*strlen(" ") +
                strlen(releaseDirPath) + strlen("/") + 1;

    if(processArgs != NULL){
      free( processArgs );
    }          
    processArgs = NULL;            
    processArgs = (char*)malloc(len);
    sprintf(processArgs,"%s/condor_replica_up %s %s %s",releaseDirPath,
                accountFilePath,versionFilePath,version_str);
    free( version_str );

    len = strlen("condor_replica_up") + strlen(releaseDirPath) + strlen("/") + 1;
    char executable[len];
    sprintf(executable,"%s/condor_replica_up",releaseDirPath);

    ReliSock* inherited_sockets[2];
    inherited_sockets[0] = s;
    inherited_sockets[1] = NULL;
     
    int ActiveTransferTid = daemonCore->Create_Process (
        executable,   // name
        processArgs,  // args
        PRIV_UNKNOWN,
        reaperUpId,
        TRUE,
        NULL,
        NULL,
        FALSE,
        (Stream**)inherited_sockets                       // socket to inherit
        );
    

    if (ActiveTransferTid == FALSE) {
        dprintf(D_ALWAYS, "Failed to create FileTransfer upload process!\n");
        return FALSE;
    }
    return TRUE;
}


















