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
char* version; // version to proceed
ReliSock* sock;

// functions definition
int download();
int doDownload(ReliSock *s);
int upload();
int doUpload(filesize_t *total_bytes, ReliSock *s);

int
main_init( int argc, char *argv[] )
{
    char *MyName = argv[0];
    char *cmd_str;

    if(argc != 3){ // name,filename and version
         // print error msg.
        return 1;
    }
    MyName = strrchr( argv[0], DIR_DELIM_CHAR );
    if( !MyName ) {
        MyName = argv[0];
    } else {
        MyName++;
    }

    cmd_str = strchr( MyName, '_');
    if( !cmd_str ) {
        // print error msg.
        return 1;
    }
    // Figure out what kind of tool we are.
    // We use strncmp instead of strcmp because
    // we want to work on windows when invoked as
    // condor_reconfig.exe, not just condor_reconfig
    if( !strncmp( cmd_str, "_down",strlen("_down") ) ) {
        cmd = DOWNLOAD;
    } else if( !strncmp( cmd_str, "_up",strlen("_up") ) ) {
        cmd = UPLOAD;
    } else {
        // print error msg.
        return 1;
    }

    filename = argv[1] ;
    version = argv[2];

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
            return download();
        case UPLOAD:
            return upload();
        default:
            // print error msg.
            return 1;  
    }
    return 1;
    
} // end main


////////////////////////////////////////////////////////
//      download functions
////////////////////////////////////////////////////////
int
download()
{
    int total_bytes = doDownload((ReliSock *)sock);

    // rotate file
    char vers_filename[_POSIX_PATH_MAX];
    sprintf(vers_filename,"%s.%s",filename,version);

    // for debug - change negot.filename
    char negfilename[_POSIX_PATH_MAX];
    sprintf(negfilename,"%s_down",filename);

    int fd;
    if((fd = open(vers_filename,O_RDONLY)) < 0){
        printf("FileTransfer::DownloadThread file <%s> doesn't exist\n",vers_filename);
        return FALSE;
    }else{
        close(fd);
    }

    if(rotate_file(vers_filename,negfilename) < 0){
        return FALSE;
    }

    ((ReliSock*)sock)->close();
    delete sock;
    return (total_bytes >= 0);
  
}


int
doDownload(ReliSock *s)
{
    int reply;
    filesize_t bytes;
    filesize_t total_bytes = 0;
    char vers_filename[_POSIX_PATH_MAX];

    sprintf(vers_filename,"%s.%s",filename,version);

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

    printf("FileTransfer::DoDownload received %d bytes\n",total_bytes);
    return TRUE;
}




////////////////////////////////////////////////////////
//       upload functions
////////////////////////////////////////////////////////
int
upload()
{
    printf("entering FileTransfer::UploadThread\n");

    filesize_t	total_bytes;
    int status = doUpload( &total_bytes, (ReliSock *)sock);
    ((ReliSock*)sock)->close();
    delete sock;
    return status;
  
}

int
doUpload(filesize_t *total_bytes, ReliSock *s)
{

    filesize_t bytes;
    *total_bytes = 0;
    printf("entering FileTransfer::DoUpload\n");

    s->encode();

    char vers_filename[_POSIX_PATH_MAX];
    memset(vers_filename,0,_POSIX_PATH_MAX);
    sprintf(vers_filename,"%s%s",filename,version);

    printf("debug orig : <%s> filename : <%s>\n",filename,vers_filename);

    // check if exist
    int fd;
    if((fd = open(vers_filename,O_RDONLY)) < 0){
        printf("debug orig : cannot open file_vers<%s>\n",vers_filename);

        if( copy_file(filename, vers_filename) ){
               printf("debug orig : cannot copy yo file_vers\n") ;
        }
    }
    else{
       close(fd) ;
    }

    printf("DoUpload: send file %s\n",filename);

    if( !s->snd_int(1,FALSE) ) {
        printf("DoUpload: exiting at %d\n",__LINE__);
        return FALSE;
    }
    if( !s->end_of_message() ) {
        printf("DoUpload: exiting at %d\n",__LINE__);
        return FALSE;
    }

    if( s->put_file( &bytes, filename ) < 0 ) {
        printf("DoUpload: Failed to send file %s, exiting at %d\n",
            filename,__LINE__);
    return FALSE;
    }

    if( !s->end_of_message() ) {
        printf("DoUpload: exiting at %d\n",__LINE__);
        return FALSE;
    }

    *total_bytes += bytes;


    // tell our peer we have nothing more to send
    s->snd_int(0,TRUE);

    printf("DoUpload: exiting at %d\n",__LINE__);

    return FALSE;
}




int
main_shutdown_graceful()
{

    DC_Exit(0);
    return 0;
}


int
main_shutdown_fast()
{
    DC_Exit(0);
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
