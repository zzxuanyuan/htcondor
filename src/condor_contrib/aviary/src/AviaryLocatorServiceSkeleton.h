

    /**
     * AviaryLocatorServiceSkeleton.h
     *
     * This file was auto-generated from WSDL for "AviaryLocatorService|http://grid.redhat.com/aviary-locator/" service
     * by the WSO2 WSF/CPP Version: 1.0
     * AviaryLocatorServiceSkeleton WSO2 WSF/CPP Skeleton for the Service Header File
     */
#ifndef AVIARYLOCATORSERVICESKELETON_H
#define AVIARYLOCATORSERVICESKELETON_H

    #include <OMElement.h>
    #include <MessageContext.h>
   
     #include <Register.h>
    
     #include <RegisterResponse.h>
    
     #include <Lookup.h>
    
     #include <LookupResponse.h>
    
namespace com_redhat_grid_aviary_locator{
    

   /** we have to reserve some error codes for adb and for custom messages */
    #define AVIARYLOCATORSERVICESKELETON_ERROR_CODES_START (AXIS2_ERROR_LAST + 2500)

    typedef enum
    {
        AVIARYLOCATORSERVICESKELETON_ERROR_NONE = AVIARYLOCATORSERVICESKELETON_ERROR_CODES_START,

        AVIARYLOCATORSERVICESKELETON_ERROR_LAST
    } AviaryLocatorServiceSkeleton_error_codes;

    


class AviaryLocatorServiceSkeleton
{
        public:
            AviaryLocatorServiceSkeleton(){}


     




		 


        /**
         * Auto generated method declaration
         * for "register|http://grid.redhat.com/aviary-locator/" operation.
         * 
         * @param _register of the AviaryLocator::Register
         *
         * @return AviaryLocator::RegisterResponse*
         */
        

         virtual 
        AviaryLocator::RegisterResponse* _register(wso2wsf::MessageContext *outCtx ,AviaryLocator::Register* _register);


     




		 


        /**
         * Auto generated method declaration
         * for "lookup|http://grid.redhat.com/aviary-locator/" operation.
         * 
         * @param _lookup of the AviaryLocator::Lookup
         *
         * @return AviaryLocator::LookupResponse*
         */
        

         virtual 
        AviaryLocator::LookupResponse* lookup(wso2wsf::MessageContext *outCtx ,AviaryLocator::Lookup* _lookup);


     



};


}



        
#endif // AVIARYLOCATORSERVICESKELETON_H
    

