/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/



#include "condor_common.h"
#include "sshd_manager.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_qmgr.h"         // need to talk to schedd's qmgr
#include "condor_attributes.h"   // for ATTR_ ClassAd stuff
#include "condor_email.h"        // for email.
#include "list.h"                // List class
#include "internet.h"            // sinful->hostname stuff
#include "daemon.h"
#include "env.h"
#include "condor_config.h"       // for 'param()'
#include "condor_uid.h"          // for PRIV_UNKNOWN

#include <sys/types.h>            // for stat
#include <sys/stat.h>
#include <unistd.h>



SshdManager::SshdManager(BaseShadow * shadow): hasKey(false),shadow(shadow){
  pubkeyFileName[0] = 0;
  keyFileName[0] = 0;
  contactFileName[0] = 0;
}


SshdManager::~SshdManager(){

  for (int i = 0; i <= sshdInfoList.getlast(); i++) {
	if (sshdInfoList[i] != NULL)
	  delete sshdInfoList[i];
  }

  dprintf(D_FULLDEBUG, "SshdManager::~SshdManager\n");
}

/*******************************************************************/
  
void 
SshdManager::cleanUp(){
  char knownHostFileName[_POSIX_PATH_MAX];

  if (keyFileName)
	sprintf(knownHostFileName, "%s.ssh_known_hosts", keyFileName);
  else
	knownHostFileName[0] = '\0';

  deleteFile(pubkeyFileName);
  deleteFile(keyFileName);
  deleteFile(contactFileName);
  deleteFile(knownHostFileName);
  hasKey = false;
}

extern void start_gdb();

// from sshd_proc 
//    return [1|0] (baseFilename, pub_length, pub_contents, pri_length, pri_contents)
int 
SshdManager::getKeys( int cmd, Stream * s){

  dprintf(D_ALWAYS, "SshdManager::getKeys\n");

  //  start_gdb();

  s->encode();

  int val;
  if (!hasKey){
	dprintf(D_FULLDEBUG, "keys seems not existing, create them\n");
	if (!createKeyFiles()){
	  // key create failed 
	  // inform them the failure.
	  val = 0;
	  s->code(val);
	  s->end_of_message();
	  return FALSE;
	}
	hasKey = true;
  }
  createContactFile();
  
  val = 1;
  char * nameBase = keyFileNameBase;
  if (!s->code(val)                ||
	  !s->code(nameBase)           ||
	  !s->end_of_message()         ||
	  !sendFile(s, pubkeyFileName) ||
	  !s->end_of_message()         ||
	  !sendFile(s, keyFileName)    ||
	  !s->end_of_message()){
	dprintf ( D_ALWAYS, "Failed to send key files!\n" );
	return FALSE;
	
  }
  return TRUE;
}


// from condor_sshd
//    get SshInfo.
int 
SshdManager::putInfo( int cmd, Stream* s ){
  /* 
	 here, we'll get information from the starter to supply 
	 fake ssh
	 1. read LamComdradeInfo from the stream
	 2. store them in an array incrementing the counter
	 3. if we got all the information, startup the master.
   */
  
  dprintf(D_ALWAYS, "SshdManager::sshdPutInfo\n");
  SshdInfo * sshd_info = new SshdInfo();

  if ( !sshd_info->code(*s) ||
	   !s->end_of_message() ){
	dprintf ( D_ALWAYS, "Failed to receive comrade info!\n" );
	return FALSE;
  }

  dprintf ( D_PROTOCOL, "Received ssh info from sneaky sshd\n" );
  dprintf ( D_FULLDEBUG, "sshd info received: hostname: %s\n", sshd_info->hostname );

  sshdInfoList[sshdInfoList.getlast() + 1] = sshd_info;

  return TRUE;
}


// from condor_ssh
//    get    hostname
//    return [1|0],  SshdInfo
int 
SshdManager::getInfo( int cmd, Stream * s){
  /*
	1. read hostname
	2. lookup the comradeinfo array
	3. return the info
   */
  dprintf ( D_FULLDEBUG, "SSH GET INFO: \n");

  char * hostname = NULL;

  if ( !s->code ( hostname ) ||
	   !s->end_of_message() ){
	dprintf ( D_ALWAYS, "Failed to receive target hostname!\n" );
	return FALSE;
  }
  dprintf ( D_FULLDEBUG, "SSHD GET INFO for host: %s\n", hostname);

  dprintf ( D_PROTOCOL, "Received target hostname from sneaky rsh\n" );
  dprintf ( D_FULLDEBUG, "target hostname: %s\n", hostname );

  /** now, lookup the array and get the information */

  SshdInfo * sshd_info = NULL;
  
  /** lookup */
  for (int i = 0; i <= sshdInfoList.getlast(); i++){
	if (strcmp(hostname, sshdInfoList[i]->hostname) == 0){
	  // found
	  sshd_info = sshdInfoList[i];
	  break;
	}
  }
  s->encode();

  int val;
  if (sshd_info == NULL){
	val = 0;
	dprintf ( D_ALWAYS, "Cannot find ssh info for target %s!\n", hostname );
	if (! s->code(val) ||
		! s->end_of_message() ){	  
	  dprintf ( D_ALWAYS, "Failed to inform error!\n" );
	}
	return FALSE;
  }

  val = 1;
  if (! s->code(val) ||
	  ! sshd_info->code(*s) ||
	  ! s->end_of_message() ){	  
	dprintf ( D_ALWAYS, "Failed to send ssh info!\n" );
	return FALSE;	
  }
  return TRUE;
}

// from condor_ssh
//    return [1|0],  SshdInfo
int 
SshdManager::getInfoAny( int cmd, Stream * s){
  /*
	1. lookup the comradeinfo array
	2. return the info
   */
  dprintf ( D_FULLDEBUG, "SSH GET INFO ANY: \n");

  if ( !s->end_of_message() ){
	dprintf ( D_ALWAYS, "Failed to receive eom!\n" );
	return FALSE;
  }

  /** now, pick the first one in the list */

  SshdInfo * sshd_info = NULL;
  
  if (sshdInfoList.getlast() >= 0)
	sshd_info = sshdInfoList[0];

  s->encode();

  int val;
  if (sshd_info == NULL){
	val = 0;
	dprintf ( D_ALWAYS, "Cannot find any ssh info in the list !\n");
	if (! s->code(val) ||
		! s->end_of_message() ){	  
	  dprintf ( D_ALWAYS, "Failed to inform error!\n" );
	}
	return FALSE;
  }

  val = 1;
  if (! s->code(val) ||
	  ! sshd_info->code(*s) ||
	  ! s->end_of_message() ){	  
	dprintf ( D_ALWAYS, "Failed to send ssh info!\n" );
	return FALSE;	
  }
  return TRUE;
}


// from parallel_master
//    return number of registered info
int 
SshdManager::getNumber( int cmd, Stream * s){
  dprintf ( D_FULLDEBUG, "SSH GET NUMBER: \n");

  s->encode();
  int val = sshdInfoList.getlast() + 1;
  if (! s->code(val) ||
	  ! s->end_of_message()) {
	dprintf ( D_ALWAYS, "Failed to send target sshd info!\n" );	
	return FALSE;
  }
  return TRUE;
}


/******************************************************************/

void 
SshdManager::deleteFile(char * fileName){
  if (fileName && fileName[0]){
    if( unlink( fileName ) == -1 ) {
	  if( errno != ENOENT ) {
		dprintf( D_ALWAYS, "Problem removing %s: errno %d.\n", 
				 fileName, errno );
	  }
    }
  }
}


/**   create key pair for ssh  */
int
SshdManager::createKeyFiles(){
  char * openssh_dir  = param( "CONDOR_PARALLEL_OPENSSH_DIR" );
  if (openssh_dir == NULL){
	dprintf( D_ALWAYS, "failed to get  CONDOR_PARALLEL_OPENSSH_DIR \n");
	return FALSE;
  }

  /** generate filenames */
  sprintf(keyFileNameBase, "key.%d.%d", shadow->getCluster(), shadow->getProc());
  sprintf(keyFileName,     "%s/%s",     shadow->getIwd(), keyFileNameBase);
  sprintf(pubkeyFileName,  "%s/%s.pub", shadow->getIwd(), keyFileNameBase);

  /** invoke ssh-keygen to have a pair of the keys */
  /** It should be more portable */

  char * args[10];
  int counter = 0;
  char command[_POSIX_ARG_MAX];
  sprintf(command, "%s/ssh-keygen", openssh_dir);
  args[counter++] = command;
  args[counter++] = "-t";
  args[counter++] = "rsa";
  args[counter++] = "-N";
  args[counter++] = "";
  args[counter++] = "-f";
  args[counter++] = keyFileName;
  args[counter++] = NULL;

  dprintf( D_FULLDEBUG, "about to fork-exec for : '%s'.\n", command );
  for (int i = 0; args[i] != NULL; i++)
	dprintf( D_FULLDEBUG, "                      : %s \n", args[i] );

  int pid;
  if ((pid = fork()) == 0){
	// i'm child
	execv(command, args);
	dprintf( D_ALWAYS, "WOW! failed to exec!: %s\n", strerror(sys_nerr) );
	exit(100);
  } else {
	// parent
	int status;
	waitpid(pid, &status, 0);
	if (WEXITSTATUS(status) == 100){
	  dprintf( D_ALWAYS, "failed to create key pair\n");
	  return FALSE;
	}
	struct stat keystat;
	if (stat(keyFileName, &keystat) == 1){
	  dprintf( D_ALWAYS, "seems to fail to create key pair, somehow\n");
	  return FALSE;
	}
  }

  free (openssh_dir);
  return TRUE;
}


// send the key file. it should be small, so I use the simplest way
int 
SshdManager::sendFile( Stream *s, char * filename){
  // get size of the file
  struct stat status;
  if (stat(filename, &status) == 1){
	dprintf( D_ALWAYS, "failed to get stat for file %s.\n", filename);
	return FALSE;
  }
  int size = status.st_size;

  // allocate a buffer
  char * buffer = new char[size];

  // read the file into the buffer
  int fd = open(filename, O_RDONLY);

  if (fd == 1){
	dprintf ( D_ALWAYS, "Failed to open %s\n", filename);
	delete buffer;
	return FALSE;
  }

  int total = 0;
  while (total < size) {
	int read_now = read(fd, buffer + total, size - total);
	total += read_now;
  }
  close(fd);

  // send size and contens
  dprintf ( D_FULLDEBUG, "length = %d\n", size);
  if (! s->code(size) ||
	  ! s->code_bytes(buffer, size)) {
	dprintf ( D_ALWAYS, "failed to send file %s'n", filename);
  }

  delete buffer;
  return TRUE;
}


int 
SshdManager::createContactFile(){

  sprintf(contactFileName, "contactFile.%d.%d", 
		  shadow->getCluster(), 
		  shadow->getProc());
  FILE * fp = fopen(contactFileName, "w");
  if (fp == NULL)
	return FALSE;

  char * openssh_dir  = param( "CONDOR_PARALLEL_OPENSSH_DIR" );
  if (openssh_dir == NULL){
	dprintf( D_ALWAYS, "failed to get  CONDOR_PARALLEL_OPENSSH_DIR \n");
	return FALSE;
  }

  char shadow_contact[128];

  if ( shadow->getJobAd()->LookupString( ATTR_MY_ADDRESS, shadow_contact ) < 1 ) {
	dprintf( D_ALWAYS, "%s not found in JobAd in SshdManager::createContactFile\n", 
			 ATTR_MY_ADDRESS );
	return FALSE;
  }
  
  fprintf(fp, "CONDOR_PARALLEL_OPENSSH_DIR='%s'\n", openssh_dir );

  fprintf(fp, "CONDOR_PARALLEL_SHADOW_CONTACT='%s'\n", shadow_contact );

  fprintf(fp, "CONDOR_PARALLEL_WORK_DIR=%s\n", shadow->getIwd());

  fprintf(fp, "CONDOR_PARALLEL_USER_KEY=%s\n", keyFileNameBase);

  free (openssh_dir);
  fclose(fp);
  return TRUE;
}
