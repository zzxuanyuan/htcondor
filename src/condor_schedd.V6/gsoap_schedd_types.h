#import "../condor_daemon_core.V6/gsoap_daemon_core_types.h"

//gsoap condorSchedd service namespace: urn:condor-schedd

typedef xsd__string condorSchedd__Requirement;

struct condorSchedd__Transaction
{
  xsd__int id 1:1;
  xsd__int duration 1:1; // change to xsd:duration ?
};

struct condorSchedd__Requirements
{
  condorSchedd__Requirement *__ptr;
  int __size;
};

struct condorSchedd__RequirementsAndStatus
{
  struct condorCore__Status status 1:1;
  struct condorSchedd__Requirements requirements 0:1;
};


struct condorSchedd__TransactionAndStatus
{
  struct condorCore__Status status 1:1;
  struct condorSchedd__Transaction transaction 0:1;
};

struct condorSchedd__IntAndStatus
{
  struct condorCore__Status status 1:1;
  xsd__int id 0:1;
};
