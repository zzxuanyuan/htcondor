// This header file is NOT for a C/C++ compiler; it is for the 
// gSOAP stub generator.

//gsoap condorCore service namespace: urn:condor-daemoncore

/*
 Below are all the xsd schema types that are required. When gSOAP
 parses this file it will see types of for form x__y, the x will
 become the namespace and y will be the actual name of the type.

 gSOAP also recognizes structs with values "*__ptr" and "__size" are
 representing an array type.

 The numbers after a struct's member's name is
 MIN_OCCURRANCES:MAX_OCCURRANCES.
 */

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

struct condorCore__Status
{
  xsd__int code 1:1;
  xsd__string message 0:1;
  struct condorCore__Status *next 0:1;
};

enum condorCore__ClassAdAttrType
{
  INTEGER = 'n',
  FLOAT = 'f',
  STRING = 's',
  EXPRESSION = 'x',
  // I'd like BOOLEAN but condor_c++_util/user_log.c++.h:34 claimed it already!
  BOOL = 'b',
  UNDEFINED = 'u',
  ERROR = 'e'
};

// n=int,f=float,s=string,x=expression,b=bool,u=undefined,e=error
struct condorCore__ClassAdStructAttr 
{
  xsd__string name 1:1;
  //	xsd__byte type 1:1;
  enum condorCore__ClassAdAttrType type 1:1;
  xsd__string value 1:1;
};

struct condorCore__ClassAdStruct 
{
	struct condorCore__ClassAdStructAttr *__ptr;	
	int __size;	// number of elements pointed to
};

struct condorCore__ClassAdStructArray 
{
	struct condorCore__ClassAdStruct *__ptr;
	int __size;
};

struct condorCore__ClassAdStructAndStatus
{
  struct condorCore__Status status 1:1;
  struct condorCore__ClassAdStruct classAd 0:1;
};

struct condorCore__ClassAdStructArrayAndStatus
{
  struct condorCore__Status status 1:1;
  struct condorCore__ClassAdStructArray classAdArray 0:1;
};

struct condorCore__StringAndStatus
{
  struct condorCore__Status status 1:1;
  xsd__string message 0:1;
};
