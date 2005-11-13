#include "dap_constants.h"
#include "condor_common.h"
#include "dap_client_interface.h"
#include "dap_classad_reader.h"
#include "dap_utility.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon.h"
#include "internet.h"
#include "condor_config.h"
#include "condor_config.h"
#include "globus_utils.h"

#ifndef WANT_NAMESPACES
#define WANT_NAMESPACES
#endif
#include "classad_distribution.h"

#define USAGE \
"[stork_server] submit_file\n\
stork_server\t\t\tspecify explicit stork server (deprecated)\n\
submit_file\t\t\tstork submit file\n\
\t-lognotes \"notes\"\tadd lognote to submit file before processing\n\
\t-stdin\t\t\tread submission from stdin instead of a file"

struct stork_global_opts global_opts;

int check_dap_format(classad::ClassAd *currentAd)
{
  char dap_type[MAXSTR];
  std::string adbuffer = "";
  classad::ExprTree *attrexpr = NULL;
  classad::ClassAdUnParser unparser;

  //should be context insensitive..
  //just check the format, not the content!
  //except dap_type ??
  
  if ( !(attrexpr = currentAd->Lookup("dap_type")) ) return 0;
  else{
    unparser.Unparse(adbuffer,attrexpr);

    char tmp_adbuffer[MAXSTR];
    strncpy(tmp_adbuffer, adbuffer.c_str(), MAXSTR);
    strcpy(dap_type,strip_str(tmp_adbuffer));

    if ( !strcmp(dap_type, "transfer") ){
      if ( !currentAd->Lookup("src_url") )  return 0;
      if ( !currentAd->Lookup("dest_url") ) return 0;
    }
    
    else if ( !strcmp(dap_type, "reserve") ){
      if ( !currentAd->Lookup("dest_host") )    return 0;
      //if ( !currentAd->Lookup("reserve_size") ) return 0;
      //if ( !currentAd->Lookup("duration") )     return 0;
      //if ( !currentAd->Lookup("reserve_id") )   return 0;
    }

    else if ( !strcmp(dap_type, "release") ){
      if ( !currentAd->Lookup("dest_host") ) return 0;
      if ( !currentAd->Lookup("reserve_id") ) return 0;
    }
    
    else return 0;
  }
  
  
  return 1;
}
//============================================================================
void MissingArgument(char *argv0,char *arg)
{
  fprintf(stderr,"Missing argument: %s\n",arg);
  stork_print_usage(stderr, argv0, USAGE, true);
  exit(1);
}

void IllegalOption(char *argv0,char *arg)
{
  fprintf(stderr,"Illegal option: %s\n",arg);
  stork_print_usage(stderr, argv0, USAGE, true);
  exit(1);
}

// Read a file into a null-terminated character string.  Return the address of
// the dyanmically allocated string.  Return NULL upon error.  It is the
// caller's responsibility to free() a dynamically allocated string.
const char
*readFile(const char *filename)
{
    char *buf = NULL;
    bool success = false;
    int fd = -1;
    struct stat stat_info;
    ssize_t bytes_read;
    size_t size;
    if ( stat(filename, &stat_info) < 0) {
        fprintf(stderr, "stat %s: %s\n", filename, strerror(errno) );
        goto ERROR;
    }

    size = stat_info.st_size;
    buf = (char *)malloc(size + 1);
    assert(buf);
    buf[size] = '\0';   // null terminated string
    fd = open(filename, O_RDONLY|O_CREAT, 0);
    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", filename, strerror(errno));
        goto ERROR;
    }
    bytes_read = full_read(fd, buf, size);

    if (bytes_read != (ssize_t)size) {
        fprintf(stderr, "file %s short read: %d out of %d: %s\n",
                filename, bytes_read, size, strerror(errno));
        goto ERROR;
    }

    success = true;

ERROR:
    if (fd >= 0) close(fd);
    if (! success) {
        if (buf) free(buf);
        return NULL;
    }
    return (const char *)buf;
}


int
submit_ad(
	Sock * sock,
	classad::ClassAd *currentAd,
  char *lognotes,
  bool spool_proxy
)
{

    //check the validity of the request
    if (currentAd == NULL) {
      fprintf(stderr, "Invalid input format! Not a valid classad!\n");
      return 1;
    }

    //add lognotes to the submit classad 
	// FIXME the lognotes apply to every job, not just a single job.  This will
	// break DAGMan.
    if (lognotes){
        if (! currentAd->InsertAttr("LogNotes", lognotes) ) {
            fprintf(stderr, "error inserting lognotes '%s' into job ad\n",
                    lognotes);
        }
    }
    
    //check format of the submit classad
    if ( !check_dap_format(currentAd)){
      fprintf(stderr, "========================\n");
      fprintf(stderr, "Not a valid DaP request!\nPlease check your submit file and then resubmit...\n");
      fprintf(stderr, "========================\n");
      return 1;
    }

	bool this_job_spool_proxy = spool_proxy;
	if( !currentAd->EvaluateAttrBool("spool_proxy",this_job_spool_proxy) ) {
		this_job_spool_proxy = spool_proxy;
	}

    std::string proxy_file_name;
    char * proxy = NULL;
    int proxy_size = 0;
    if (currentAd->EvaluateAttrString ("x509proxy",proxy_file_name )) {

        if ( proxy_file_name == "default" ) {
            char *defproxy = get_x509_proxy_filename();
            if (defproxy) {
                printf("using default proxy: %s\n", defproxy);
                proxy_file_name = defproxy;
                free(defproxy);
            } else {
                fprintf(stderr, "ERROR: %s\n", x509_error_string() );
                return 1;
            }
        }

		if( this_job_spool_proxy ) {

			struct stat stat_buff;
			if (stat (proxy_file_name.c_str(), &stat_buff) == 0) {
				proxy_size = stat_buff.st_size;
			} else {
                fprintf(stderr, "ERROR: proxy %s: %s\n",
                        proxy_file_name.c_str(),
                        strerror(errno) );
                return 1;
			}

			// Do a quick check on the proxy.
			if ( x509_proxy_try_import( proxy_file_name.c_str() ) != 0 ) {
				fprintf(stderr, "ERROR: check credential %s: %s\n",
						proxy_file_name.c_str(),
						x509_error_string() );
				return 1;
			}
			int remaining =
				x509_proxy_seconds_until_expire( proxy_file_name.c_str() );
			if (remaining < 0) {
				fprintf(stderr, "ERROR: check credential %s expiration: %s\n",
						proxy_file_name.c_str(),
						x509_error_string() );
				return 1;
			}
			if (remaining == 0) {
				fprintf(stderr, "ERROR: credential %s has expired\n",
						proxy_file_name.c_str() );
				return 1;
			}

			FILE * fp = fopen (proxy_file_name.c_str(), "r");
			if (fp) {
				proxy = (char*)malloc ((proxy_size+1)*sizeof(char));
				ASSERT(proxy);
				if (fread (proxy, proxy_size, 1, fp) != 1) {
					fprintf(stderr, "ERROR: Unable to read proxy %s: %s\n",
							proxy_file_name.c_str(),
							strerror(errno) );
					if (proxy) free(proxy);
					return 1;
				}
				fclose (fp);
			} else {
				fprintf(stderr, "ERROR: Unable to open proxy %s: %s\n",
						proxy_file_name.c_str(),
						strerror(errno) );
				if (proxy) free(proxy);
				return 1;
			}
		}
    }

    //if input is valid, then send the request:
    classad::PrettyPrint unparser;
	std::string adbuffer = "";
    unparser.Unparse(adbuffer, currentAd);
    fprintf(stdout, "================\n");
    fprintf(stdout, "Sending request:");
    fprintf(stdout, "%s\n", adbuffer.c_str());


	char *submit_error_reason = NULL;
	char * job_id = NULL;
	int rc = stork_submit (sock,
						currentAd,
						 global_opts.server,
						 proxy, 
						 proxy_size, 
						 job_id,
						 submit_error_reason);
    fprintf(stdout, "================\n");

	if (rc) {
		 fprintf (stdout, "\nRequest assigned id: %s\n", job_id);	
	} else {
		fprintf (stderr, "\nERROR submitting request (%s)!\n",
				submit_error_reason);
	}
	if (proxy) free(proxy);

	return 0;
}

/* ============================================================================
 * main body of dap_submit
 * ==========================================================================*/
int main(int argc, char **argv)
{
  int i;
  std::string adstr="";
  classad::ClassAdParser parser;
#if 0
  classad::ClassAd *currentAd = NULL;
  int leftparan = 0;
  FILE *adfile;
#endif
  char fname[_POSIX_PATH_MAX];
  char *lognotes = NULL;
  int read_from_stdin = 0;

  config();	// read config file

  bool spool_proxy = param_boolean("STORK_SPOOL_PROXY",true);

  // Parse out stork global options.
  stork_parse_global_opts(argc, argv, USAGE, &global_opts, true);

  for(i=1;i<argc;i++) {
    char *arg = argv[i];
    if(arg[0] != '-') {
      break; //this must be a positional argument
    }
#define OPT_LOGNOTES	"-l"	// -lognotes
    else if(!strncmp(arg,OPT_LOGNOTES,strlen(OPT_LOGNOTES) ) ) {
      if(i+1 >= argc) MissingArgument(argv[0],arg);
      lognotes = argv[++i];
    }
#define OPT_STDIN	"-s"	// -stdin
    else if(!strncmp(arg,OPT_STDIN,strlen(OPT_STDIN) ) ) {
      read_from_stdin = 1;
    }
    else if(!strcmp(arg,"--")) {
      //This causes the following arguments to be positional, even if they
      //begin with "--".  This is the standard getopt convention, even
      //though we are using non-standard long-argument notation for
      //consistency with other Condor tools.
      i++;
      break;
    }
    else {
      IllegalOption(argv[0],arg);
    }
  }


	int num_positional_args = argc - i;
	switch (num_positional_args) {
		case 0:
			if(! read_from_stdin) {
			  stork_print_usage(stderr, argv[0], USAGE, true);
			  exit(1);
			}
		case 1:
			if(read_from_stdin) {
				fprintf(stderr, "stdin not support in this version\n");
					return 1;	// FIXME
				strcpy(fname, "stdin");
				global_opts.server = argv[i];
			} else {
				strcpy(fname, argv[i]);
			}
		  break;
	  case 2:
			global_opts.server = argv[i++];
			strcpy(fname, argv[i]);
		  break;
	  default:
		  stork_print_usage(stderr, argv[0], USAGE, true);
		  exit(1);
  }

#if 0
  //open the submit file
  if(read_from_stdin) adfile = stdin;
  else {
    adfile = fopen(fname,"r");
    if (adfile == NULL) {
      fprintf(stderr, "Cannot open submit file %s: %s\n",fname, strerror(errno) );
      exit(1);
    }
  }
  

  int nrequests = 0;


    i =0;
    while (1){
      c = fgetc(adfile);
      if (c == ']'){ 
	leftparan --; 
	if (leftparan == 0) break;
      }
      if (c == '[') leftparan ++; 
      
      if (c == EOF) {
	fprintf (stderr, "Invalid Stork submit file %s\n", fname);
	return 1;
      }
      
      adstr += c;
      i++;
    }
    adstr += c;

    nrequests ++;
    if (nrequests > 1) {
      fprintf (stderr, "Multiple requests currently not supported!\n");
      return 1;
    } 
#endif

	const char *adBuf = readFile(fname);
	if (! adBuf) return 1;

	MyString sock_error_reason;

	Sock * sock = 
		start_stork_command_and_authenticate (
											global_opts.server,
											  STORK_SUBMIT,
											  sock_error_reason);

	const char *host = global_opts.server ? global_opts.server : "unknown";
	if (!sock) {
		fprintf(stderr, "ERROR: connect to server %s: %s\n",
				host, sock_error_reason.Value() );
		return 1;
	}

	// read all classads out of the input file
	int offset = 0;
	classad::ClassAd ad;
	std::string adBufStr = adBuf; //work around a bug in ParseClassAd(char*,...)
    while (parser.ParseClassAd(adBufStr, ad, offset) ) {
		// TODO: Add transaction processing, so that either all of, or none of
		// the submit ads are added to the job queue.  The current
		// implementation can fail after a partial submit, and not inform the
		// user.
        if (submit_ad(sock, &ad, lognotes, spool_proxy) != 0) {
			break;
		}
		while(adBufStr.size() > (unsigned)offset &&
				isspace(adBufStr[offset])) offset++;
    }

	sock->encode();
	char *goodbye = "";
	sock->code(goodbye);
	sock->eom();

	sock->close();

	delete sock;

	return 0;
}

