#ifndef __X509CREDENTIAL_H__
#define __X509CREDENTIAL_H__
#include "credential.h"
#include "condor_classad.h"

class X509Credential : public Credential {
public:

  X509Credential ();
  X509Credential (const ClassAd&);
  X509Credential (const char * metadata);
  X509Credential (const char * metadata, const void * buff, int size);
  virtual ~X509Credential();

  virtual time_t GetRealExpirationTime();

  int StoreData ();
  int LoadData ();

  const char * GetMyProxyServerDN();
  const char * GetRefreshPassword();
  const char * GetMyProxyServerHost();
  const char * GetCredentialName();

  void SetStorageName (const char *);
  const char * GetStorageName();

 protected:
  MyString storage_name;
  MyString myproxy_server_host;
  MyString myproxy_server_dn;
  MyString myproxy_server_password;
  MyString myproxy_credential_name;

};

#endif
