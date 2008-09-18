/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "daemon.h"
//#include "X509credential.h"
#include "condor_distribution.h"
#include "dc_credd.h"
#include "condor_config.h"
#include "globus_utils.h"

int main(int argc, char **argv)
{
  CondorError errorstack;
  MyString ss;

  myDistro->Init(argc, argv);
  config();
  Termlog = 1;
  dprintf_config("TOOL");

  //setenv("X509_USER_PROXY", getenv("IAN_PROXY"), 1);
  fprintf(stderr, "Proxy file: '%s'\n", get_x509_proxy_filename());
  
//  char * credd_sin = argv[1];
  
  DCCredd credd(NULL);
  if(!credd.locate()) {
	  fprintf(stderr, "Can't locate credd: %s\n", credd.error());
	  exit(1);
  }
//  sleep(120);
  if (credd.getSharedSecret(ss,	errorstack)) {
      printf ("Received '%s'\n", ss.Value());
	  return 0;
  } else {
	  fprintf (stderr, "ERROR (%d : %s)\n", 
			   errorstack.code(),
			   errorstack.message());
	  return 1;
  }
}

  
