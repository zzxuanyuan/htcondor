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
#include "condor_attributes.h"
#include "sshd_proc.h"
#include "env.h"
#include "condor_string.h"  // for strnewp
#include "my_hostname.h"
#include "starter.h"

extern CStarter *Starter;

SshdProc::SshdProc( ClassAd * jobAd ) : VanillaProc( jobAd ), baseFileName(NULL)
{

  shadow_contact[0] = 0;
  if ( JobAd->LookupString( ATTR_MY_ADDRESS, shadow_contact ) < 1 ) {
	dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
			 ATTR_MY_ADDRESS );
  }
  
  dprintf ( D_FULLDEBUG, "Constructor of SshdProc::SshdProc\n" );
}

SshdProc::~SshdProc()  {
  if (baseFileName != NULL) 
	delete baseFileName;
}


/** 
 * 1.) replace the executable
 * 2.) get keys 
 * 3.) set the environment variables
 *     - KEY
 *     - OPEN_SSHD, SSH location
 */

int 
SshdProc::StartJob()
{ 
	dprintf(D_FULLDEBUG,"in SshdProc::StartJob()\n");

	if ( !JobAd ) {
		dprintf ( D_ALWAYS, "No JobAd in SshdProc::StartJob()!\n" ); 
		return 0;
	}

	// replace executable and clear arguments
	if ( !alterExec() ) { 
		dprintf ( D_ALWAYS, "failed to replace exec in SshdProc::StartJob()!\n" ); 
		return 0;
	}

	priv_state priv = set_user_priv();
	if ( !getKeys() ) {
		dprintf ( D_ALWAYS, "failed to get key files in SshdProc::StartJob()!\n" ); 
		return 0;
	}
	set_priv(priv);
	
	// replace environment variables
	if ( !alterEnv() ) { 
		dprintf ( D_ALWAYS, "failed to change envs in SshdProc::StartJob()!\n" ); 
		return 0;
	}


    return VanillaProc::StartJob();
}


void 
SshdProc::Suspend() { 
  //  this code is stolen from mpi_comrade proc.
  //  I donot know what to do, actually.
  //    -hidemoto

	dprintf(D_FULLDEBUG,"in SshdProc::Suspend()\n");
	daemonCore->Send_Signal( daemonCore->getpid(), SIGQUIT );
}


void 
SshdProc::Continue() { 
	dprintf(D_FULLDEBUG,"in SshdProc::Continue() (!)\n");    
        // really should never get here, but just in case.....
    VanillaProc::Continue();
}


// send back the shadow contact string to the schedd
// another dirty hack.

bool 
SshdProc::PublishUpdateAd( ClassAd* ad ){
  char buf[200];

  sprintf( buf, "%s=%s", ATTR_MY_ADDRESS, shadow_contact );
  ad->InsertOrUpdate( buf );

  VanillaProc::PublishUpdateAd(ad);
}




/************************************************************************/

/************************************************************************/
/** 
  1. replace the job command in the JobAd
  2. clear 'args',  if exists
  */
int 
SshdProc::alterExec(){

  char *rsh_dir = param( "CONDOR_PARALLEL_RSH_DIR" );
  if (rsh_dir == NULL){
	dprintf(D_ALWAYS,"Connot find CONDOR_PARALLEL_RSH_DIR in config");
	return FALSE;
  }
  char executable[2048];
  sprintf( executable, "%s/condor_sshd", rsh_dir );
  free( rsh_dir );
 
  char tmp[2048];
  sprintf ( tmp, "%s = \"%s\"",
	    ATTR_JOB_CMD, executable );
  if (! JobAd->InsertOrUpdate( tmp )){
	dprintf ( D_ALWAYS, "Failed to insert Job args: %s\n", tmp);
	return FALSE;
  }
  
  // clear arg 
  sprintf( tmp, "%s = \"%s\"", 
		   ATTR_JOB_ARGUMENTS, "");
  if (! JobAd->InsertOrUpdate( tmp )){
	dprintf ( D_ALWAYS, "Failed to insert Job args: %s\n", tmp);
	return FALSE;
  }
  return TRUE;
}


/* 
   1. setup enviroment according to condor configuration file
   CONDOR_PARALLEL_OPENSSH_DIR
   CONDOR_PARALLEL_OPENSSHD_DIR
   CONDOR_PARALLEL_USER_SHELL
   CONDOR_PARALLEL_WORK_DIR

   2. add 
   CONDOR_PARALLEL_RSH_DIR
   to the path 
 */

int
SshdProc::alterEnv()
{

  dprintf ( D_FULLDEBUG, "SshdProc::alterPath()\n" );

  char *rsh_dir = param( "CONDOR_PARALLEL_RSH_DIR" );
  if (rsh_dir == NULL){
	dprintf(D_ALWAYS,"Connot find CONDOR_PARALLEL_RSH_DIR in config\n");
	return FALSE;
  }
  char * openssh_dir  = param( "CONDOR_PARALLEL_OPENSSH_DIR" );
  if (openssh_dir == NULL){
	free (rsh_dir);
	dprintf(D_ALWAYS,"Connot find CONDOR_OPENSSH_DIR in config\n");
	return FALSE;
  }
  char * opensshd_dir = param( "CONDOR_PARALLEL_OPENSSHD_DIR" );
  if (opensshd_dir == NULL){
	free (rsh_dir);
	free (openssh_dir);
	dprintf(D_ALWAYS,"Connot find CONDOR_OPENSSHD_DIR in config\n");
	return FALSE;
  }
  char * openssh_port_start = param( "CONDOR_PARALLEL_OPENSSH_PORT_START" );
  if (openssh_port_start == NULL){
	free (rsh_dir);
	free (openssh_dir);
	free (opensshd_dir);
	dprintf(D_ALWAYS,"Connot find CONDOR_OPENSSH_PORT_START in config\n");
	return FALSE;
  }
  char * user_shell   = "/bin/sh";
  const char * work_dir     = Starter->GetWorkingDir();


/* task:  First, see if there's a PATH var. in the JobAd->env.  
   If there is, alter it.  If there isn't, insert one. */
    
    char *tmp;
	char *env_str = NULL;
	Env envobject;
	if ( !JobAd->LookupString( ATTR_JOB_ENVIRONMENT, &env_str )) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting.\n", 
				 ATTR_JOB_ENVIRONMENT );
		return 0;
	}

	envobject.Merge(env_str);
	free(env_str);

	MyString path;
	MyString new_path;

	new_path = rsh_dir;
	new_path += ":";

	if(envobject.getenv("PATH",path)) {
        // The user gave us a path in env.  Find & alter:
        dprintf ( D_FULLDEBUG, "$PATH in ad:%s\n", path.Value() );

		new_path += path;
	}
	else {
        // User did not specify any env, or there is no 'PATH'
        // in env sent along.  We get $PATH and alter it.

        tmp = getenv( "PATH" );
        if ( tmp ) {
            dprintf ( D_FULLDEBUG, "No Path in ad, $PATH in env\n" );
            dprintf ( D_FULLDEBUG, "before: %s\n", tmp );
			new_path += tmp;
        }
        else {   // no PATH in env.  Make one.
            dprintf ( D_FULLDEBUG, "No Path in ad, no $PATH in env\n" );
			new_path = rsh_dir;
        }
    }
	envobject.Put("PATH",new_path.Value());
    
    envobject.Put( "CONDOR_PARALLEL_OPENSSH_DIR",  openssh_dir);
    envobject.Put( "CONDOR_PARALLEL_OPENSSH_PORT_START", openssh_port_start);
    envobject.Put( "CONDOR_PARALLEL_OPENSSHD_DIR", opensshd_dir);
    envobject.Put( "CONDOR_PARALLEL_USER_SHELL",   user_shell);
    envobject.Put( "CONDOR_PARALLEL_WORK_DIR",     work_dir);
    envobject.Put( "CONDOR_PARALLEL_RSH_DIR",      rsh_dir);
    envobject.Put( "CONDOR_PARALLEL_USER_KEY",     baseFileName);
    envobject.Put( "CONDOR_PARALLEL_SHADOW_CONTACT", shadow_contact);

        // now put the env back into the JobAd:
	env_str = envobject.getDelimitedString();
    dprintf ( D_FULLDEBUG, "New env: %s\n", env_str );

	free( openssh_dir );
	free( opensshd_dir );

	bool assigned = JobAd->Assign( ATTR_JOB_ENVIRONMENT,env_str );
	if(env_str) {
		delete[] env_str;
	}
	if(!assigned) {
		dprintf( D_ALWAYS, "Unable to update env! Aborting.\n" );
		return 0;
	}

    return 1;
}

// send req to the submission machine and get the key file
int
SshdProc :: getKeys() {
  // connect

  dprintf(D_FULLDEBUG, "getKeys\n");
  ReliSock * s = new ReliSock;
  if ( !s->connect( shadow_contact ) ) {
	delete s;
	dprintf(D_ALWAYS, "failed to connect to the shadow %s\n", shadow_contact);
	return FALSE;
  }
  dprintf(D_FULLDEBUG, "got connection to %s\n", shadow_contact);
  
  // send code
  s->encode();
  int cmd = SSHD_GETKEYS;
  if ( !s->code ( cmd )  ||
	   !s->end_of_message()) {

	delete s;
	return FALSE;
  }
  dprintf(D_FULLDEBUG, "sent command, red filename\n");

  s->decode();
  int val;
  if (!s->code(val)){
	dprintf(D_ALWAYS, "faild to get answer\n");
	return FALSE;	
  }

  if (val == 0){
	dprintf(D_ALWAYS, "looks like submit machine failed to create the keys\n");
	s->end_of_message();
	return FALSE;
  }

  if (!s->code( baseFileName ) ||
	  !s->end_of_message()){
	dprintf(D_ALWAYS, "faild to get filename\n");
	return FALSE;	
  }

  char pubKeyName[1000];
  sprintf(pubKeyName, "%s.pub", baseFileName);
  if ( !readStoreFile(s, pubKeyName, 0644)   ||
	   !s->end_of_message()            ||
	   !readStoreFile(s, baseFileName, 0600) ||	   
	   !s->end_of_message()){
	dprintf(D_ALWAYS, "faild to get files\n");
	return FALSE;	
  }

  s->close();
  delete s;

  return TRUE;
}


int 
SshdProc :: readStoreFile(Stream * s, char * filename, int mode){
  const char * work_dir = Starter->GetWorkingDir();

  char fullpath[1000];
  sprintf(fullpath, "%s/%s", work_dir, filename);

  int fd = open(fullpath, O_WRONLY | O_CREAT, mode);

  if (fd == 1) {
	dprintf(D_ALWAYS, "failed to create file %s\n", filename);
	return FALSE;
  }  

  int length;
  if ( !s->code(length) ) {
	dprintf(D_ALWAYS, "failed to read length of a file %s\n", filename);
	return FALSE;
  }  

  dprintf(D_FULLDEBUG, "length is %d, try to read them\n", length);

  char * buffer;
  buffer = new char[length];
  if (! s->code_bytes(buffer, length)) {
	dprintf(D_ALWAYS, "failed to read file contents %s\n", filename);
	return FALSE;
  }
  
  dprintf(D_FULLDEBUG, "write the contents to  %s\n", fullpath);
  write(fd, buffer, length);

  close(fd);
  delete buffer;
  return TRUE;
}
