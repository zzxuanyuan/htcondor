#include "condor_common.h"
#include "reli_sock.h"
#include "condor_attributes.h"
#include "condor_commands.h"
#include "condor_distribution.h"
#include "condor_environ.h"
#include "sshd_info.h"

#ifndef POSIX_PATH_MAX
#define POSIX_PATH_MAX 255
#endif

#define MY_LINE_MAX 8000

#define OPT_STRICT_HOST_KEY_CHECK  "-oStrictHostKeyChecking=no"
#define OPT_SEND_ENV               "-oSendEnv=CONDOR_PARALLEL_*"
#define OPT_QUIET                  "-q"

char    OPT_USER_KNOWN_HOSTS_FILE[POSIX_PATH_MAX];
#define OPT_USER_KNOWN_HOSTS_FILE_F "-oUserKnownHostsFile=%s"

char    OPT_PORT[20];
#define OPT_PORT_F "-p%d"

char    OPT_USER_KEY[POSIX_PATH_MAX];
#define OPT_USER_KEY_F "-i%s/%s"

char    OPT_USER_NAME[20];
#define OPT_USER_NAME_F "-l%s"


char    wrapper[POSIX_PATH_MAX];

int check_space_inside(char * arg){
  if (arg[0] == '\"' || arg[0] == '\'')
	return 0;
  while (*arg != '\0'){
	if (*arg == ' ')
	  return 1;
	arg++;
  }
  return 0;

}

void concat_arg(char * arg_str, char * arg, int check){
  int flag = 0;
  if (check)
	flag = check_space_inside(arg);
  if (flag)
	strcat(arg_str, "\"");
  strcat(arg_str, arg);
  if (flag)
	strcat(arg_str, "\"");
  strcat(arg_str, " ");
}

#define DEBUG
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

char   openssh[POSIX_PATH_MAX];
char   known_hosts_file[POSIX_PATH_MAX];
char * openssh_dir;
char * shadow_contact;
char * work_dir;
char * userkey_filename;


char * target_user_shell;
char * target_work_dir;
char * target_rsh_dir;
char * target_openssh_dir;
int    target_port;
char * target_user_name;
char * hostname = NULL;

int get_envs(){
  if ((openssh_dir = getenv("CONDOR_PARALLEL_OPENSSH_DIR")) == NULL){
	log_header(); fprintf(fp, "failed to get CONDOR_PARALLEL_OPENSSH_DIR\n"); fflush(fp);
	return 0;
  }
  if ((shadow_contact = getenv("CONDOR_PARALLEL_SHADOW_CONTACT")) == NULL){
	log_header(); fprintf(fp, "failed to get CONDOR_PARALLEL_SHADOW_CONTACT\n"); fflush(fp);
	return 0;
  }
  if ((work_dir = getenv("CONDOR_PARALLEL_WORK_DIR")) == NULL){
	log_header(); fprintf(fp, "failed to get CONDOR_PARALLEL_WORK_DIR\n"); fflush(fp);
	return 0;
  }
  if ((userkey_filename = getenv("CONDOR_PARALLEL_USER_KEY")) == NULL){
	log_header(); fprintf(fp, "failed to get CONDOR_PARALLEL_USER_KEY\n"); fflush(fp);
	return 0;
  }

  sprintf(openssh, "%s/ssh", openssh_dir);
  sprintf(known_hosts_file, "%s/.ssh_known_hosts", work_dir);

  return 1;
}

int choose_any = 0;
class SshdInfo sshd_info;

int get_target_info(char * & hostname){
  ReliSock * s = new ReliSock;
  
  if ( !s->connect( shadow_contact ) ) {
	log_header(); fprintf(fp, "failed to connect to %s\n", shadow_contact); fflush(fp);
	delete s;
	exit(31);
  }
  
  s->encode();
  
  int cmd;

  if (!choose_any){   //  suply hostname
	cmd = SSHD_GETINFO;
	if ( !s->code ( cmd ) ) {
	  log_header(); fprintf(fp, "failed to send command to %s\n", shadow_contact); fflush(fp);
	  delete s;
	  exit(32);
	}
	
	if ( !s->code ( hostname ) ) {
	  log_header(); fprintf(fp, "failed to send hostname to %s\n", shadow_contact); fflush(fp);
	  delete s;
	  exit(32);
	}
  } else {
	// get the first one I can get

	cmd = SSHD_GETINFO_ANY;
	if ( !s->code ( cmd ) ) {
	  log_header(); fprintf(fp, "failed to send command to %s\n", shadow_contact); fflush(fp);
	  delete s;
	  exit(32);
	}
  }
  
  if (!s->end_of_message() ) {
	log_header(); fprintf(fp, "failed to send EOM to %s\n", shadow_contact); fflush(fp);
	delete s;
	exit(33);
  }
  
  s->decode();

  /** read info */
  int val;
  if (!s->code(val) || val == 0){
	log_header(); fprintf(fp, "failed to get info from %s\n", shadow_contact); 
	fflush(fp);
	return 0;
  }

  if (!sshd_info.code(*s)){
	log_header(); fprintf(fp, "failed to get info from %s\n", shadow_contact); 
	delete s;
	exit(33);
  }

  s->close();
  delete s;

  log_header(); fprintf(fp, "get info from %s\n", shadow_contact); fflush(fp);
  target_user_shell  = sshd_info.userShell;
  target_work_dir    = sshd_info.workDir;
  target_rsh_dir     = sshd_info.rshDir;
  target_openssh_dir = sshd_info.opensshDir;
  target_port        = sshd_info.port;
  target_user_name   = sshd_info.userName;
  hostname           = sshd_info.hostname;
  
  sprintf(wrapper, "%s/wrapper.sh", target_rsh_dir);

  return 1;
}

void setup_opt(){
  sprintf(OPT_USER_KNOWN_HOSTS_FILE, OPT_USER_KNOWN_HOSTS_FILE_F, known_hosts_file);
  sprintf(OPT_PORT, OPT_PORT_F, target_port);
  sprintf(OPT_USER_KEY, OPT_USER_KEY_F, work_dir, userkey_filename);
  sprintf(OPT_USER_NAME, OPT_USER_NAME_F, target_user_name);
}

/** to pass the enviroment variables to the remote executable, setup it */
void setup_envs(){
  setenv("CONDOR_PARALLEL_USER_SHELL",  target_user_shell , 1);
  setenv("CONDOR_PARALLEL_WORK_DIR",    target_work_dir,    1);
  setenv("CONDOR_PARALLEL_RSH_DIR",     target_rsh_dir,     1);
  setenv("CONDOR_PARALLEL_OPENSSH_DIR", target_openssh_dir, 1);
  /** 
	CONDOR_PARALLEL_USER_KEY and CONDOR_PARALLEL_SHADOW_CONTACT are
	already set up, and will be passed to the remote 
	*/
}

int main(int argc, char ** argv){
  int i, n_i = 0;
  char ** new_argv;

#ifdef DEBUG
  fp = fopen("/tmp/rsh.log", "a");
  if (fp == NULL)
	fp = fopen("/dev/null", "w");
#else
	fp = fopen("/dev/null", "w");
#endif


  /* allocate possible maximum size */
  new_argv = (char **)malloc(sizeof(char *) * argc + 40);
  if (new_argv == NULL){
	log_header(); fprintf(fp, "failed to alloc\n"); fflush(fp);
	exit(3);
  }

  char arg_str[MY_LINE_MAX];
  int got_command = 0;
  int no_wrapper = 0;
  
  if (!get_envs()){
	log_header(); fprintf(fp, "failed to get env: abort\n"); fflush(fp);
	exit(3);
  }
  new_argv[n_i++] = openssh;

  sprintf(arg_str, "'");
  
  for (i = 1; i < argc; i++){
	/* ignore options for rsh*/
	if (argv[i][0] == '-') {
	  if (got_command){
		concat_arg(arg_str, argv[i], 1);
	  } else {
		if (strcmp(argv[i], "-no_wrapper") == 0)
		  no_wrapper = 1;
		else if (strcmp(argv[i], "-choose_any") == 0)
		  choose_any = 1;
		else
		  /* set them as options for the ssh */
		  new_argv[n_i++] = argv[i];
	  }
	} else {
	  if (hostname == NULL) { 
		hostname = argv[i];
	  } else {
		if (!got_command){
		  got_command = 1;
		  concat_arg(arg_str, argv[i], 0);
		} else {
		  concat_arg(arg_str, argv[i], 1);
		}
	  }	
	}
  }
  strcat(arg_str, "'");

  if (!get_target_info(hostname)){
	log_header(); fprintf(fp, "failed to get information from shadow:  abort\n"); fflush(fp);
	exit(3);
  }

  setup_opt();
  setup_envs();

#ifdef DEBUG
  fprintf(fp, "------------------\n"); 
  for (i = 1; i < argc; i++)
	fprintf(fp, "%s\n", argv[i]);
  fprintf(fp, "------------------\n");
#endif

  new_argv[n_i++] = OPT_SEND_ENV;
  new_argv[n_i++] = OPT_QUIET;
  new_argv[n_i++] = OPT_STRICT_HOST_KEY_CHECK;
  new_argv[n_i++] = OPT_USER_KNOWN_HOSTS_FILE;
  new_argv[n_i++] = OPT_PORT;
  new_argv[n_i++] = OPT_USER_KEY;
  new_argv[n_i++] = OPT_USER_NAME;

  new_argv[n_i++] = hostname;
  if (no_wrapper){
	// cut off the heading and trailing quotes
	arg_str[strlen(arg_str) - 1] = '\0';
	new_argv[n_i++] = arg_str + 1;	
  } else {
	new_argv[n_i++] = wrapper;	
	new_argv[n_i++] = arg_str;
  }	

  new_argv[n_i] = NULL;


#ifdef DEBUG
  for (i = 0; i < n_i; i++)
	fprintf(fp, "%s\n", new_argv[i]);
  fprintf(fp, "------------------\n");
  fclose(fp);
#endif

  execv(openssh, new_argv);
  /** somehow, failed */
  perror("execv");
  return -1;
}
