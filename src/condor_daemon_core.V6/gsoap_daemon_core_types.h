// This header file is NOT for a C/C++ compiler; it is for the 
// gSOAP stub generator.

/*
 Below are all the xsd schema types that are required. When gSOAP
 parses this file it will see types of for form x__y, the x will
 become the namespace and y will be the actual name of the type.

 gSOAP also recognizes structs with values "*__ptr" and "__size" as
 representing an array type.

 The numbers after a struct's member's name is
 MIN_OCCURRANCES:MAX_OCCURRANCES.
 */

//gsoap condor service namespace: urn:condor
//gsoap condor service style: rpc
//gsoap condor service encoding: encoded

typedef char *xsd__string;
typedef char *xsd__anyURI;
typedef float xsd__float;
typedef int xsd__int;
typedef bool xsd__boolean;
typedef long long xsd__long;
typedef char xsd__byte;
struct xsd__base64Binary
{
  unsigned char * __ptr;
  int __size;
};

enum condor__StatusCode
{
  SUCCESS,
  FAIL,
  INVALIDTRANSACTION,
  UNKNOWNCLUSTER,
  UNKNOWNJOB,
  UNKNOWNFILE,
  INCOMPLETE,
  INVALIDOFFSET,
  ALREADYEXISTS
};

struct condor__Status
{
  enum condor__StatusCode code 1:1;
  xsd__string message 0:1;
  struct condor__Status *next 0:1;
};

enum condor__ClassAdAttrType
{
  INTEGER_ATTR = 'n',
  FLOAT_ATTR = 'f',
  STRING_ATTR = 's',
  EXPRESSION_ATTR = 'x',
  BOOLEAN_ATTR = 'b',
  UNDEFINED_ATTR = 'u',
  ERROR_ATTR = 'e'
};

// n=int,f=float,s=string,x=expression,b=bool,u=undefined,e=error
struct condor__ClassAdStructAttr
{
  xsd__string name 1:1;
  //	xsd__byte type 1:1;
  enum condor__ClassAdAttrType type 1:1;
  xsd__string value 1:1;
};

struct ClassAdStruct
{
	struct condor__ClassAdStructAttr *__ptr;	
	int __size;
};

struct ClassAdStructArray 
{
	struct ClassAdStruct *__ptr;
	int __size;
};

struct condor__ClassAdStructAndStatus
{
  struct condor__Status status 1:1;
  struct ClassAdStruct classAd 0:1;
};

struct condor__ClassAdStructArrayAndStatus
{
  struct condor__Status status 1:1;
  struct ClassAdStructArray classAdArray 0:1;
};

struct condor__StringAndStatus
{
  struct condor__Status status 1:1;
  xsd__string message 0:1;
};

struct condor__StringAndStatusResponse {
  struct condor__StringAndStatus response;
};

struct condor__ClassAdStructAndStatusResponse {
  struct condor__ClassAdStructAndStatus response;
};

struct condor__ClassAdStructArrayAndStatusResponse {
  struct condor__ClassAdStructArrayAndStatus response;
};

struct condor__StatusResponse {
  struct condor__Status response;
};

