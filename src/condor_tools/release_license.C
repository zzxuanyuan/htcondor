/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include <math.h>
#include <float.h>
#include "condor_state.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_api.h"

#include "condor_network.h"
#include "condor_classad.h"
#include "condor_commands.h"
#include "condor_io.h"

#include "MyString.h"

//------------------------------------------------------------------------

static void Usage(char* name) 
{
  fprintf(stderr, "Usage: %s job_id | -constraint expr\n",name);
  exit(1);
}

//------------------------------------------------------------------------

main(int argc, char* argv[])
{
  char constraint[512];
  *constraint='\0';

  int i;
  for(i=1; i<argc; i++) {

    // Analyze date specifiers

    if (strcmp(argv[i],"-help")==0) {
      Usage(argv[0]);
    }
    else if (strcmp(argv[i],"-constraint")==0) {
      if (i+1==argc || *constraint) Usage(argv[0]);
      sprintf (constraint, "(%s)", argv[i+1]);
      i++;
    }
    else {
      if (*constraint) Usage(argv[0]);
      sprintf (constraint, "(%s == \"%s\")", ATTR_JOB_ID, argv[i]);
    }
  }
  if (i<argc || *constraint=='\0') Usage(argv[0]);

  config( 0 );
  CondorQuery licenseQuery   (LICENSE_AD);
  QueryResult result;
  ClassAdList licenseAds;
  if  (((result = licenseQuery.addConstraint(constraint))    != Q_OK) ||
      ((result = licenseQuery.fetchAds(licenseAds))!= Q_OK))
  {
      dprintf (D_ALWAYS,
          "Error %s:  failed to fetch license ads ... aborting\n",
          getStrQueryResult(result));
      exit(1);
  }

  ClassAd* ad;
  char ipAddr[32];
  char key[64];
  int command=RELEASE_LICENSE;

  licenseAds.Open();
  while ((ad=licenseAds.Next())) {
    ReliSock sock;
    ad->LookupString(ATTR_NAME,key);
    ad->LookupString (ATTR_STARTD_IP_ADDR, ipAddr);
//printf("Name=%s , IpAddr=%s\n",key,ipAddr);
    if (!sock.connect (ipAddr, 0)) {
      dprintf(D_ALWAYS,"Could not connect to %s\n", ipAddr);
      continue;
    }

    sock.encode();
    if (!sock.code(command) ||
        !sock.code(key) ||
        !sock.end_of_message()) {
      fprintf(stderr, "failed to send command to the License server\n");
      continue;
    }

  }
  licenseAds.Close();

  return 0;
}
