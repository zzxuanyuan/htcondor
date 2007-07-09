// server.c
#include "soapH.h"
#include "soapStub.h"
// #include "ns.nsmap"
#include "smdevp.h"
#include "wsseapi.h"
#include "condor_common.h"

#define BIND_ACCEPT_PORT 31310
/*
int main(void)
{
    struct soap *soap;
    soap = soap_new();
    soap_register_plugin(soap, soap_wsse);
    soap_omode(soap, SOAP_ENC_XML | SOAP_XML_GRAPH | SOAP_XML_INDENT);
    //soap_begin(soap);
    static char hmac_key[16] =
        { 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88,
          0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00 };

#ifdef BIND_ACCEPT_PORT
    fprintf(stderr, "binding.\n");
    if(!soap_valid_socket(soap_bind(soap, NULL, BIND_ACCEPT_PORT, 10))) {
        soap_print_fault(soap, stderr);
        exit(1);
    }
    fprintf(stderr, "bound, accepting\n");
    if(soap_valid_socket(soap_accept(soap))) {
        fprintf(stderr, "got valid socket.\n");
#endif		
		soap_wsse_verify_auto(soap, SOAP_SMD_HMAC_SHA1, hmac_key, 
							  sizeof(hmac_key));
		fprintf(stderr,"serving...\n");
		
		if(soap_serve(soap)) {
			fprintf(stderr, "error...\n");
			soap_wsse_delete_Security(soap);
			soap_print_fault(soap, stderr);
			soap_print_fault_location(soap, stderr);
		}
		
		fprintf(stderr, "done....\n");
#ifdef BIND_ACCEPT_PORT
    }
	soap_print_fault(soap, stderr);
#endif
    exit(1);
} */

int __condor__signature(struct soap *soap, struct _condor__signatureRequest *in) 
{
	return SOAP_OK;
}
/*
// As always, a namespace mapping table is needed:
struct Namespace namespaces[] =
{   // {"ns-prefix", "ns-name"}
    {"SOAP-ENV", "http://schemas.xmlsoap.org/soap/envelope/"},
    {"SOAP-ENC", "http://schemas.xmlsoap.org/soap/encoding/"},
    {"xsi", "http://www.w3.org/2001/XMLSchema-instance"},
    {"xsd", "http://www.w3.org/2001/XMLSchema"},
    {"ns", "urn:signature"}, // bind "ns" namespace prefix
    {NULL, NULL}
};*/ 
