// This header file is NOT for a C/C++ compiler; it is for the 
// gSOAP stub generator.

typedef char *xsd__string; // encode xsd__string value as the xsd:string schema type 
typedef char *xsd__anyURI; // encode xsd__anyURI value as the xsd:anyURI schema type 
typedef float xsd__float; // encode xsd__float value as the xsd:float schema type 
typedef int xsd__int;  // encode xsd__int value as the xsd:int schema type 
typedef bool xsd__boolean; // encode xsd__boolean value as the xsd:boolean schema type 
typedef unsigned long long xsd__positiveInteger; // encode xsd__positiveInteger value as the xsd:positiveInteger schema type
typedef long long xsd__long;
typedef char xsd__byte;

//TODD gsoap namespace-prefix service name: service-name 
//TODD gsoap namespace-prefix service documentation: text 
//TODD gsoap namespace-prefix service portType: portType 
//TODD gsoap namespace-prefix service location: URL 
//TODD gsoap namespace-prefix service executable: executable-name 
//TODD gsoap namespace-prefix service encoding: literal 
//TODD gsoap namespace-prefix service namespace: namespace-URI 
//TODD gsoap namespace-prefix schema namespace: namespace-URI 
//TODD gsoap namespace-prefix service method-documentation: method-name //text 
//TODD gsoap namespace-prefix service method-action: method-name action 
//TODD gsoap namespace-prefix service method-encoding: method-name literal 


//gsoap condorCore service namespace: urn:condor-daemoncore

// n=int,f=float,s=string,x=expression,b=bool,u=undefined,e=error
struct condorCore__ClassAdStructAttr 
{	//public:
	xsd__string name 1:1;
	xsd__byte type 1:1;
	//int __typeOfValue; // the SOAP_TYPE pointed to by value below
	//void *value;
	xsd__string value 1:1;
	//int* valueInt 0:1;
	//float* valueFloat 0:1;
	//bool* valueBool 0:1;
	//char* valueExpr 0:1;
};
struct ClassAdStruct 
{	//public:
	struct condorCore__ClassAdStructAttr *__ptr;	
	int __size;	// number of elements pointed to
};
struct ClassAdStructArray 
{	//public:
	struct ClassAdStruct *__ptr;
	int __size;
};




int condorCore__getInfoAd(void *_, struct ClassAdStruct & ad);
int condorCore__getVersionString(void *_, char* & verstring);
int condorCore__getPlatformString(void *_, char* & verstring);

	
	
