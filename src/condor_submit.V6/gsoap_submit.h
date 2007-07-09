#import "wsse.h"	// wsse = <http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd>
//gsoap condor schema namespace:	urn:condor
//gsoap condor schema form:	unqualified
// Imported element "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd":Security declared as _wsse__Security

#import "gsoap_daemon_core_types.h"
/// "urn:condor":submitRequest is a complexType.
struct _condor__signatureRequest
{
/// Element classAd of type "urn:condor":ClassAdStruct.
    struct condor__ClassAdStruct*        classAd                        0;	///< Nullable pointer.
};


/// Element "urn:condor":submitResponse of complexType.

/// "urn:condor":submitResponse is a complexType.
struct _condor__signatureResponse
{
/// Element response of type xs:int.
    int                                  response                       1;	///< Required element.
};

//gsoap condor service name:	condorSubmit
//gsoap condor service type:	condorSubmitPortType 

//gsoap condor service namespace:	urn:condor 
//gsoap condor service transport:	http://schemas.xmlsoap.org/soap/http 

struct SOAP_ENV__Header
{
    mustUnderstand                       // must be understood by receiver
    _wsse__Security                     *wsse__Security                ;	///< TODO: Check element type (imported type)

};

//gsoap condor service method-style:	condorSubmit document
//gsoap condor service method-encoding:	condorSubmit literal
//gsoap condor service method-action:	condorSubmit ""
//gsoap condor service method-input-header-part:	condorSubmit wsse__Security
//gsoap condor service method-output-header-part:	condorSubmit wsse__Security
int __condor__signature(
    struct _condor__signatureRequest*      condor__signatureRequest,
	void rv
    //struct _condor__signatureResponse*     condor__signatureResponse ///< response parameter
);

/* End of successor.h */
