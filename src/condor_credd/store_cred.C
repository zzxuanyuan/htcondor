#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "daemon.h"
#include "X509credential.h"
#include "my_username.h"
#include "condor_config.h"

int main(int argc, char **argv)
{
  char * credd_sin = argv[1];
  char * cred_filename = argv[2];

  config();

  Daemon my_credd(DT_CREDD, NULL, NULL);
  Sock * sock = my_credd.startCommand (CREDD_STORE_CRED, Stream::reli_sock, 0);

  if (!sock) {
    fprintf (stderr, "Unable to start CREDD_STORE_CRED command to host %s\n", credd_sin);
    return 1;
  }

  sock->encode();

  X509Credential * cred = new X509Credential();
  cred->SetStorageName(cred_filename);
  cred->LoadData();
  cred->SetName("MyCred");

  char * username = my_username(0);
  cred->SetOwner (username);
  

  ClassAd classad(*(cred->GetClassAd()));
  classad.put (*sock);

  int size = cred->GetDataSize();

  void * data;
  cred->GetData (data, size);
  sock->code_bytes (data, size);
  printf ("sent data\n");

  sock->eom();
  sock->decode();
  
  int rc;
  sock->code(rc);

  printf ("Server returned %d \n", rc);

  sock->close();
  delete sock;
  return 0;
}

  
