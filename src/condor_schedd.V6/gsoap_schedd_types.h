#import "../condor_daemon_core.V6/gsoap_daemon_core_types.h"

typedef xsd__string condor__Requirement;

enum condor__HashType {
  NOHASH,
  MD5
};

enum condor__UniverseType {
  STANDARD = 1,
  PVM = 4,
  VANILLA = 5,
  SCHEDULER = 7,
  MPI = 8,
  GLOBUS = 9,
  JAVA = 10
};

struct condor__Transaction
{
  xsd__int id 1:1;
  xsd__int duration 1:1; // change to xsd:duration ?
};

struct Requirements
{
  condor__Requirement *__ptr;
  int __size;
};

struct condor__RequirementsAndStatus
{
  struct condor__Status status 1:1;
  struct Requirements requirements 0:1;
};


struct condor__TransactionAndStatus
{
  struct condor__Status status 1:1;
  struct condor__Transaction transaction 0:1;
};

struct condor__IntAndStatus
{
  struct condor__Status status 1:1;
  xsd__int integer 0:1;
};

struct condor__Base64DataAndStatus
{
  struct condor__Status status 1:1;
  struct xsd__base64Binary data 0:1;
};

struct condor__FileInfo
{
  xsd__string name 1:1;
  xsd__long size 1:1;
};

struct FileInfoArray
{
  struct condor__FileInfo *__ptr;
  int __size;
};

struct condor__FileInfoArrayAndStatus
{
  struct condor__Status status 1:1;
  struct FileInfoArray info 0:1;
};
