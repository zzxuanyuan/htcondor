#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon.h"
#include "X509credential.h"

int main(int argc, char **argv)
{
  char * credd_sin = argv[1];
  char * cred_name = argv[2];

  Daemon my_credd(DT_CREDD, credd_sin, NULL);
  Sock * sock = my_credd.startCommand (CREDD_REMOVE_CRED, Stream::reli_sock, 0);

  if (!sock) {
    fprintf (stderr, "Unable to start CREDD_REMOVE_CRED command to host %s\n", credd_sin);
    return 1;
  }

  sock->encode();

  sock->code (cred_name);

  sock->eom();

  sock->decode();

  int rc=0;
  sock->code (rc);
  
  if (rc) {
    fprintf (stderr, "Unable to remove credential %s\n", cred_name);
  }

  sock->close();
  delete sock;
  return rc;
}

  
