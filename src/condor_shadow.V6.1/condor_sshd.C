#include "condor_common.h"
#include "reli_sock.h"
#include "condor_attributes.h"
#include "condor_commands.h"
#include "condor_distribution.h"
#include "condor_environ.h"
#include "sshd_info.h"
#include "time.h"

#ifndef _POSIX_PATH_MAX
#define _POSIX_PATH_MAX 255
#endif
#define MY_LINE_MAX 8000

#define SLEEP_SEC 1  /* I'm not sure 1 second is alway enough but, .. */

#define OPT_DEBUG_FLAGS "-De"

char    SSHD_CMD[_POSIX_PATH_MAX];

char    OPT_SSHD_CONF[_POSIX_PATH_MAX];
#define OPT_SSHD_CONF_F "-f%s"

char    OPT_PORT[20];
#define OPT_PORT_F     "-p%i"

#define OPT_ACCEPT_ENV "-oAcceptEnv=CONDOR_PARALLEL_*"

#define OPT_STRICT_MODES "-oStrictModes=no"

char    OPT_ALLOW_USER[_POSIX_PATH_MAX];
#define OPT_ALLOW_USER_F "-oAllowUsers=%s"

char    OPT_AUTHORIZED_KEYS_FILE[_POSIX_PATH_MAX];
#define OPT_AUTHORIZED_KEYS_FILE_F "-oAuthorizedKeysFile=%s.pub"

char    OPT_HOST_KEY[_POSIX_PATH_MAX];
#define OPT_HOST_KEY_F "-h%s"

char    hostkey[_POSIX_PATH_MAX];
#define HOST_KEY_F "%s/.ssh_host_rsa_key"
#define SSHD                "sshd"
#define SSH_KEYGEN          "ssh-keygen"
#define SSH_KEYGEN_OPTIONS  "-q -t rsa -N '' -f"

char  * current_working_dir   = NULL;
char  * opensshd_dir          = NULL;
char  * openssh_dir           = NULL;
char  * shadow_contact        = NULL;
char  * username              = NULL;
char  * userkey_filename      = NULL;
char  * hostname              = NULL;
char  * rsh_dir               = NULL;
char  * user_shell            = NULL;


#define SSHD_CONF_F           "%s/.tmp_sshd_conf"
char    sshd_conf[_POSIX_PATH_MAX];

#define USER_KEY_F            "%s/%s"
char    userkey[_POSIX_PATH_MAX];

int     port;


#define DEBUG 1
FILE * fp;

void log_header()
{
  time_t clock;
  char * pc;

  clock = time(NULL);
  pc = ctime(&clock);
  *(pc+24) = '\0';
  fprintf(fp, "%s (%d) ", pc, getpid());
}

int setup_envs(){
  if ((current_working_dir = getenv("CONDOR_PARALLEL_WORK_DIR")) == NULL){
	fprintf(fp, "failed to get CONDOR_PARALLEL_WORK_DIR\n");
	return 0;
  }
  if ((opensshd_dir = getenv("CONDOR_PARALLEL_OPENSSHD_DIR")) == NULL){
	fprintf(fp, "failed to get CONDOR_PARALLEL_OPENSSHD_DIR\n");
	return 0;
  }
  if ((openssh_dir = getenv("CONDOR_PARALLEL_OPENSSH_DIR")) == NULL){
	fprintf(fp, "failed to get CONDOR_PARALLEL_OPENSSH_DIR\n");
	return 0;
  }
  if ((shadow_contact = getenv("CONDOR_PARALLEL_SHADOW_CONTACT")) == NULL){
	fprintf(fp, "failed to get CONDOR_PARALLEL_SHADOW_CONTACT\n");
	return 0;
  }
  if ((userkey_filename = getenv("CONDOR_PARALLEL_USER_KEY")) == NULL){
	fprintf(fp, "failed to get CONDOR_PARALLEL_USER_KEY\n");
	return 0;
  }

  if ((rsh_dir = getenv("CONDOR_PARALLEL_RSH_DIR")) == NULL){
	fprintf(fp, "failed to get CONDOR_PARALLEL_RSH_DIR\n");
	return 0;
  }

  char * port_str;
  if ((port_str = getenv("CONDOR_PARALLEL_OPENSSH_PORT_START")) == NULL){
	fprintf(fp, "failed to get CONDOR_PARALLEL_OPENSSH_PORT_START\n");
	return 0;
  }
  if ((port = atoi(port_str)) == 0){
	fprintf(fp, "CONDOR_PARALLEL_OPENSSH_PORT_START is not proper val: %s\n", port_str);
	return 0;
  }	


  // get the user id
  struct passwd * pw = getpwuid(getuid());
  if (pw == NULL){
	fprintf(fp, "failed to get passwd info for this process\n");
	return 0;
  }
  username = strdup(pw->pw_name);

  sprintf(hostkey, HOST_KEY_F, current_working_dir);
  sprintf(userkey, USER_KEY_F, current_working_dir, userkey_filename);
  sprintf(sshd_conf, SSHD_CONF_F, current_working_dir);  

  /** prepair blank file for the sshd_conf */
  {
	FILE * fp = fopen(sshd_conf, "w");
	if (fp == NULL){
	  perror("fopen");
	  fprintf(fp, "failed to open sshd_conf\n");
	  return 0;
	}
	fclose(fp);
  }

  hostname = my_full_hostname();
  // get user shell from passwd file 
  uid_t id =  get_my_uid();
  struct passwd * my_passwd = getpwuid(id);

  // Do I have better way to make a copy ?
  user_shell = strdup(my_passwd->pw_shell);

  return 1;
}

int gen_hostkey(){
  char command[MY_LINE_MAX];
  
  sprintf(command, "%s/%s %s %s",
		  openssh_dir, SSH_KEYGEN, SSH_KEYGEN_OPTIONS, 
		  hostkey);
		  
  if (system(command) == 1){
	perror("system");
	fprintf(fp, "failed to system: %s\n", command);	
	return 0;
  }
  return 1;
}


void setupArgs(){
  sprintf(SSHD_CMD,       "%s/%s",          opensshd_dir, SSHD);
  sprintf(OPT_SSHD_CONF , OPT_SSHD_CONF_F,  sshd_conf);
  sprintf(OPT_ALLOW_USER, OPT_ALLOW_USER_F, username);
  sprintf(OPT_AUTHORIZED_KEYS_FILE, OPT_AUTHORIZED_KEYS_FILE_F, userkey);
  sprintf(OPT_HOST_KEY,   OPT_HOST_KEY_F, hostkey);
}


/* returns pid */
int invoke_sshd(){
  int pid;
  sprintf(OPT_PORT, OPT_PORT_F,  port);  
  pid = fork();

  if (pid == 1) { // failure
	perror("fork");
	exit(3);
  }
  if (pid == 0) { // I'm child 
	char * args[20];
	int i = 0;
	args[i++] = SSHD_CMD;
	args[i++] = OPT_DEBUG_FLAGS;
	args[i++] = OPT_SSHD_CONF;
	args[i++] = OPT_PORT;
	args[i++] = OPT_ACCEPT_ENV;
	args[i++] = OPT_STRICT_MODES;
	//	args[i++] = OPT_ALLOW_USER;
	args[i++] = OPT_AUTHORIZED_KEYS_FILE;
	args[i++] = OPT_HOST_KEY;
	args[i++] = NULL;	

	{
	  int j;
	  for (j=0;j < i; j++){
		fprintf(fp, "\t%s\n", args[j]);
		fflush(fp);
	  }
	}

	execv(SSHD_CMD, args);
	perror("execv");
	exit(3);
  }
  // I'm parent
  return pid;
}

int inform_shadow(){
  class SshdInfo info;

  info.hostname   = strdup(hostname);
  info.rshDir     = strdup(rsh_dir);
  info.userShell  = strdup(user_shell);
  info.workDir    = strdup(current_working_dir);
  info.opensshDir = strdup(openssh_dir);
  info.userName   = strdup(username);
  info.port       = port;

  log_header(); fprintf(fp, "try to inform shadow: port = %i\n", port); fflush(fp);

  ReliSock * s = new ReliSock;
  if ( !s->connect( shadow_contact ) ) {
	delete s;
	return 0;
  }
  
  s->encode();
  
  int cmd = SSHD_PUTINFO;
  if ( !s->code ( cmd ) ) {
	delete s;
	return 0;
  }
  
  if (!info.code( *s ) ||
	  !s->end_of_message() ) {
	delete s;
	return 0;
  }

  s->close();
  delete s;
  log_header(); fprintf(fp, "try to inform shadow: port = %i\n", port); fflush(fp);
  return 1;
}

int main(int argc, char ** argv){
  int i;
  int retry_limit = 10;
  
#ifdef DEBUG
  fp = fopen("/tmp/sshd.log", "a");
  if (fp == NULL)
	fp = stderr;
#else
  fp = stderr;
#endif

#ifdef DEBUG
log_header(); fprintf(fp, "try to setup var\n"); fflush(fp);
#endif

  /** try to setup variables with envirotments */
  if (!setup_envs()){
	fprintf(fp, "failed to get variables, exit\n");
	exit(3);
  }
#ifdef DEBUG
log_header(); fprintf(fp, "setup env done\n"); fflush(fp);
#endif

  /** try to generate keys */
  if (!gen_hostkey()){
	fprintf(fp, "failed to generate host keypair, exit\n");
	exit(3);
  }

#ifdef DEBUG
log_header(); fprintf(fp, "host keygen Done\n"); fflush(fp);
#endif

  setupArgs();

#ifdef DEBUG
log_header(); fprintf(fp, "setup Args Done\n");  fflush(fp);
#endif


  for (i = 0; i < retry_limit; i++){
	pid_t pid = invoke_sshd();
	int options = WNOHANG;
	/** wait for few seconds to make sure that it is working */
	sleep(SLEEP_SEC);
	signal(SIGALRM, SIG_IGN);
	if (waitpid(pid, NULL, options) == 0){
	  int status;

	  /** 
		The sshd is still living, suppose it is successfully listening 
		Send the ssh information
	   */
	  if (!inform_shadow()){
		/** failed to connect! exit now */
		exit(3);
	  }
	  fprintf(fp, "waiting \n");
	  wait(&status);
	  exit(status);
	  fprintf(fp, "waiting done \n");
	}
	port++;
  }
  /** OK, failed to invoke, Should I try to inform it to the shadow? **/
  exit(3);
}
