#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_version.h"
#include "condor_io.h"
#include "daemon.h"
#include "daemon_types.h"
#include "command_strings.h"
#include "condor_distribution.h"
#include "daemon_list.h"
#include "string_list.h"

// to take inherited sockets
#include "../condor_daemon_core.V6/condor_daemon_core.h"

// for copy_file
#include "util_lib_proto.h"

extern "C" int SetSyscalls(int val){return val;}
extern char* myName;

char *mySubSystem = "Transfer";  // for daemon core

// download
// parameters : file name to download, version to download , sock_inherit_list (or sender address)

// upload
// parameters : file name to upload, version to upload , sock_inherit_list ( or receiver address )

// executables names:
// condor_replica_down
// condor_replica_up

typedef enum {
    UPLOAD = 1,
    DOWNLOAD = 2,       
}CMD;

// Global variables
int cmd = 0; // UPLOAD or DOWNLOAD
char* filename;
char* replicafile;
char* version; // version to proceed
ReliSock* sock;

// functions definition
int download();
int doDownload(ReliSock *s, char* downfile);
int downloadFile(char* downfile);

int upload();
int doUpload(filesize_t *total_bytes, ReliSock *s, char* upfile);


int
main_init( int argc, char *argv[] )
{
    //freopen("TransferOut","w",stdout);
    dprintf(D_ALWAYS,"Transfer 1 %d\n",argc);
    dprintf(D_ALWAYS,"Transfer 2 \n");
    
    
    char *MyName = argv[0];
    char *cmd_str;

    if(argc != 4){ // name,filename,version file and version
         // print error msg.
        dprintf(D_ALWAYS,"Transfer  error : argc != 4\n"); 
         DC_Exit(1);
    }
    dprintf(D_ALWAYS,"Transfer 2a \n");
    MyName = strrchr( argv[0], DIR_DELIM_CHAR );
    if( !MyName ) {
        MyName = argv[0];
    } else {
        MyName++;
    }

    cmd_str = strchr( MyName, '_');
    if( !cmd_str ) {
        // print error msg.
         DC_Exit(1);
    }
    dprintf(D_ALWAYS,"Transfer 3 %s\n",cmd_str);
    // Figure out what kind of tool we are.
    // We use strncmp instead of strcmp because
    // we want to work on windows when invoked as
    // condor_reconfig.exe, not just condor_reconfig
    if( !strncmp( cmd_str, "_replica_down",strlen("_replica_down") ) ) {
        cmd = DOWNLOAD;
    } else if( !strncmp( cmd_str, "_replica_up",strlen("_replica_up") ) ) {
        cmd = UPLOAD;
    } else {
        // print error msg.
         DC_Exit(1);

    }
    dprintf(D_ALWAYS,"Transfer 4 %d filename %s replica %s version %s \n",cmd, argv[1], argv[2], argv[3]);
    filename = argv[1] ;
    replicafile = argv[2];
    version = argv[3];

    // try to take inherited socket
    
    Stream **socks = daemonCore->GetInheritedSocks();
    if (socks[0] == NULL ||
        socks[1] != NULL ||
        socks[0]->type() != Stream::reli_sock)
    {
        dprintf(D_ALWAYS, "Failed to inherit remote system call socket.\n");
        DC_Exit(1);
    }
    
    sock = (ReliSock *)socks[0];
       
    switch(cmd){
        case DOWNLOAD:
            DC_Exit(download());
        case UPLOAD:
            DC_Exit(upload());
        default:
            // print error msg.
            DC_Exit(1);  
    }
   
   DC_Exit(1);
} // end main


////////////////////////////////////////////////////////
//      download functions
////////////////////////////////////////////////////////
int
download()
{
    // download negotiator account file
    int res = downloadFile(filename);
    if(res == FALSE){
          ((ReliSock*)sock)->close();
          delete sock;
          return 1;
    }
    // download replica version file
    res = downloadFile(replicafile);
    ((ReliSock*)sock)->close();
    delete sock;
    return 0;  
}


int
downloadFile(char* downfile)
{
    // doDownload (to file <downfile>.<version> )
    int total_bytes = doDownload((ReliSock *)sock,downfile);

    // rotate file
    char vers_filename[_POSIX_PATH_MAX];
    sprintf(vers_filename,"%s.%s",downfile,version);

    int fd;
    if((fd = open(vers_filename,O_RDONLY)) < 0){
        dprintf(D_ALWAYS, "FileTransfer::DownloadThread file <%s> doesn't exist\n",vers_filename);
        ((ReliSock*)sock)->close();
        delete sock;
        return FALSE;
    }else{
        close(fd);
    }

    
    if(rotate_file(vers_filename,downfile) < 0){
        ((ReliSock*)sock)->close();
        delete sock;
        return FALSE;
    }
    
    return (total_bytes >= 0);

}


int
doDownload(ReliSock *s, char* downfile)
{
    int reply;
    filesize_t bytes;
    filesize_t total_bytes = 0;
    char vers_filename[_POSIX_PATH_MAX];

    sprintf(vers_filename,"%s.%s",downfile,version);

    // check if exist, in such case don't download
    int fd;
    if((fd = open(vers_filename,O_RDONLY)) >= 0){
        close(fd);
        return TRUE;
    }

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

        if( s->get_file( &bytes, vers_filename ) < 0 ) {
            return FALSE;
        }

        if( !s->end_of_message() ) {
            return FALSE;
        }
        total_bytes += bytes;
    }

    dprintf(D_ALWAYS, "FileTransfer::DoDownload received %d bytes\n",total_bytes);
    return TRUE;
}




////////////////////////////////////////////////////////
//       upload functions
////////////////////////////////////////////////////////
int
upload()
{
    dprintf(D_ALWAYS, "entering FileTransfer::UploadThread\n");

    filesize_t	total_bytes;
    // upload negotiator account file
    int status = doUpload( &total_bytes, (ReliSock *)sock, filename);

    // upload replica version file
    if(status != FALSE){
        status = doUpload( &total_bytes, (ReliSock *)sock, replicafile);
    }
    ((ReliSock*)sock)->close();
    delete sock;
    if(status == TRUE)
        return 0;
    return 1;
  
}

int
doUpload(filesize_t *total_bytes, ReliSock *s, char* upfile)
{

    filesize_t bytes;
    *total_bytes = 0;
    dprintf(D_ALWAYS, "entering FileTransfer::DoUpload\n");

    s->encode();

    char vers_filename[_POSIX_PATH_MAX];
    memset(vers_filename,0,_POSIX_PATH_MAX);
    sprintf(vers_filename,"%s%s",upfile,version);

    //dprintf(D_ALWAYS, "debug orig : <%s> filename : <%s>\n",upfile,vers_filename);

    // check if exist
    int fd;
    if((fd = open(vers_filename,O_RDONLY)) < 0){
        dprintf(D_ALWAYS, "debug orig : cannot open file_vers<%s>\n",vers_filename);

        if( copy_file(upfile, vers_filename) ){
               dprintf(D_ALWAYS, "debug orig : cannot copy yo file_vers\n") ;
        }
    }
    else{
       close(fd) ;
    }

    dprintf(D_ALWAYS, "DoUpload: send file %s\n",upfile);

    if( !s->snd_int(1,FALSE) ) {
        dprintf(D_ALWAYS, "DoUpload: exiting at %d\n",__LINE__);
        return FALSE;
    }
    if( !s->end_of_message() ) {
        dprintf(D_ALWAYS, "DoUpload: exiting at %d\n",__LINE__);
        return FALSE;
    }

    if( s->put_file( &bytes, vers_filename ) < 0 ) {
        dprintf(D_ALWAYS, "DoUpload: Failed to send file %s, exiting at %d\n",
            upfile,__LINE__);
    return FALSE;
    }

    if( !s->end_of_message() ) {
        dprintf(D_ALWAYS, "DoUpload: exiting at %d\n",__LINE__);
        return FALSE;
    }

    *total_bytes += bytes;


    // tell our peer we have nothing more to send
    s->snd_int(0,TRUE);

    dprintf(D_ALWAYS, "DoUpload: exiting at %d\n",__LINE__);

    return TRUE;
}




int
main_shutdown_graceful()
{

   // DC_Exit(0);
    return 0;
}


int
main_shutdown_fast()
{
   // DC_Exit(0);
    return 0;
}

int
main_config( bool is_full )
{

    return 1;
}


void
main_pre_dc_init( int argc, char* argv[] )
{
}


void
main_pre_command_sock_init( )
{
}
