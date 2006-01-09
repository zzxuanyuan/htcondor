#include "dap_constants.h"
#include "dap_utility.h"
#include "my_hostname.h"

char *strip_str(char *str)
{

  while ( str[0] == '"' || str[0] ==' ') {
    str++;
  }
  
  while ( str[strlen(str)-1] == '"' || str[strlen(str)-1] == ' ') {
    str[strlen(str)-1] = '\0';
  }
  
  return str;
}

void parse_url(char *url, char *protocol, char *host, char *filename)
{

  char temp_url[MAXSTR];
  char unstripped[MAXSTR];
  char *p;
  
  //initialize
  strcpy(protocol, "");
  strcpy(host, "");
  strcpy(filename, "");
  
  strcpy(unstripped, url);
  strcpy(temp_url, strip_str(unstripped));

  //get protocola
  if (strcmp(temp_url,"")) {
    strcpy(protocol, strtok(temp_url, "://") );   
  }
  else {
    strcpy(protocol, "");       
    printf("Error in parsing URL %s\n", url);
    return;
  }

  //if protocol == file
  if (!strcmp(protocol, "file")){
    strcpy(host, "localhost");
  }
  else { //get the hostname
    p = strtok(NULL,"/");
    if (p != NULL){
      strcpy(host, p);                 
    }
    else
      strcpy(host, "");   
  }

  //get rest of the filename
  //add "/" to filename, if (protocol != nest)
  p = strtok(NULL,"");

  if (p != NULL){
    if (strcmp(protocol, "nest")  && strcmp(protocol, "file"))
      strcat(strcpy(filename, "/"), p);   //get file name
    else 
      strcpy(filename, p);
  }
  else {
    strcpy(filename, "");
  }

  // printf("protocol:%s\n",protocol);
  //printf("host:%s\n",host);
  //printf("filename:%s\n",filename);

}

// Create a predictable unique path, given a directory, basename, job id, and
// pid.  The return value points to a statically allocated string.  This
// function is not reentrant.
const char *
job_filepath(
		const char *basename,
		const char *suffix,
		const char *dap_id,
		pid_t pid
)
{
	static char path[_POSIX_PATH_MAX];

	sprintf(path, "%s%s-job%s-pid%u",
			basename, suffix, dap_id, pid);
	return path;
}

