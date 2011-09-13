

#ifndef AVIARYLOCATORSERVICESTUB_H
#define AVIARYLOCATORSERVICESTUB_H
/**
* AviaryLocatorServiceStub.h
*
* This file was auto-generated from WSDL for "AviaryLocatorService|http://grid.redhat.com/aviary-locator/" service
* by the Apache Axis2/Java version: 1.0  Built on : Sep 07, 2011 (03:40:38 EDT)
*/

#include <stdio.h>
#include <OMElement.h>
#include <Stub.h>
#include <ServiceClient.h>


#include <AviaryLocator_Register.h>

#include <AviaryLocator_RegisterResponse.h>

#include <AviaryLocator_Lookup.h>

#include <AviaryLocator_LookupResponse.h>


namespace com_redhat_grid_aviary_locator
{

#define AVIARYLOCATORSERVICESTUB_ERROR_CODES_START (AXIS2_ERROR_LAST + 2000)

typedef enum
{
     AVIARYLOCATORSERVICESTUB_ERROR_NONE = AVIARYLOCATORSERVICESTUB_ERROR_CODES_START,

    AVIARYLOCATORSERVICESTUB_ERROR_LAST
} AviaryLocatorServiceStub_error_codes;

 class IAviaryLocatorServiceCallback;

 

class AviaryLocatorServiceStub : public wso2wsf::Stub
{

        public:
        /**
         *  Constructor of AviaryLocatorService class
         *  @param client_home WSF/C home directory
         *  
         */
        AviaryLocatorServiceStub(std::string& client_home);

        /**
         *  Constructor of AviaryLocatorService class
         *  @param client_home WSF/C home directory
         *  @param endpoint_uri The to endpoint uri,
         */

        AviaryLocatorServiceStub(std::string& client_home, std::string& endpoint_uri);

        /**
         * Populate Services for AviaryLocatorServiceStub
         */
        void WSF_CALL
        populateServicesForAviaryLocatorService();

        /**
         * Get the endpoint uri of the AviaryLocatorServiceStub
         */

        std::string WSF_CALL
        getEndpointUriOfAviaryLocatorService();

        

            /**
             * Auto generated function declaration
             * for "register|http://grid.redhat.com/aviary-locator/" operation.
             * 
             * @param _register12 of the AviaryLocator::Register
             *
             * @return AviaryLocator::RegisterResponse*
             */

            AviaryLocator::RegisterResponse* WSF_CALL _register( AviaryLocator::Register*  _register12);
          

            /**
             * Auto generated function declaration
             * for "lookup|http://grid.redhat.com/aviary-locator/" operation.
             * 
             * @param _lookup14 of the AviaryLocator::Lookup
             *
             * @return AviaryLocator::LookupResponse*
             */

            AviaryLocator::LookupResponse* WSF_CALL lookup( AviaryLocator::Lookup*  _lookup14);
          

        /**
         * Auto generated function for asynchronous invocations
         * for "register|http://grid.redhat.com/aviary-locator/" operation.
         * @param stub The stub
         * 
         * @param _register12 of the AviaryLocator::Register
         * @param ICallback callback handler
         */


        void WSF_CALL
        start__register(AviaryLocator::Register*  _register12,IAviaryLocatorServiceCallback* callback);

        

        /**
         * Auto generated function for asynchronous invocations
         * for "lookup|http://grid.redhat.com/aviary-locator/" operation.
         * @param stub The stub
         * 
         * @param _lookup14 of the AviaryLocator::Lookup
         * @param ICallback callback handler
         */


        void WSF_CALL
        start_lookup(AviaryLocator::Lookup*  _lookup14,IAviaryLocatorServiceCallback* callback);

          


};

/** we have to reserve some error codes for adb and for custom messages */



}


        
#endif        
   

