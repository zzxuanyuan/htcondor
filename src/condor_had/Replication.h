// Listener.h: interface for the HADReplication class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(HAD_REPLICATION_H__)
#define HAD_REPLICATION_H__

#include "../condor_daemon_core.V6/condor_daemon_core.h"



//#define REPLICATION_CYCLE 300 //5 min (5*60)


class ReplicaVersion;

    /**
    * class   HADReplication
    */
class HADReplication  
{
public:

    /**
    *   Const'r
    */
	HADReplication();

    /**
    *   Destructor
    */
	~HADReplication();

    void finilize();
	void initialize();
	int reinitialize();

    void  initializeFile(StringList*,int);
    bool isInitializedFile(){return wasInitiated;};

    void commandHandler(int cmd,Stream *strm) ;
   
    void  replicate();  // called each replication cycle

    void  checkReceivedVersions() ;

    void updateReplicaVersion();
private:
    // param in condor_config, default value is 5 min.
    int replicationInterval;

    // buffers for getting received version ids
    SimpleList<ReplicaVersion> receivedVersionsList;
    StringList receivedAddrVersionsList;

    // reapers
    static int fileTransferDownloadReaper (Service*, int pid, int exit_status);
    static int fileTransferUploadReaper (Service*, int pid, int exit_status);
    int reaperDownId;
    int reaperUpId;

    // version functions
    ReplicaVersion* repVersion;
    
    void  insertVersion(char* addr,char* version);
    ReplicaVersion   getBestVersion(char* addr) ;

    StringList* otherHADIPs;
    bool wasInitiated;
    
    char* versionFilePath;
    char* releaseDirPath;
    char* accountFilePath;

    // commands
    int   sendCommandToOthers(int cmd);
    int   sendJoinCommand(int cmd,char* addr);
    int   sendVersionNumCommand(int cmd,char* addr);
    ReliSock*   sendTransferCommand(char* addr);
    

    // transfer functions
    //char* versionToDownLoad;

    int  sendFile(char* );
    int  receiveFile(char* );

    int Download(ReliSock *s);
    int Upload(ReliSock *s);

    struct paramThread{
        HADReplication* myObj;
        char* version;
    };
	
};

#endif // !HAD_REPLICATION_H__

