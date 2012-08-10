
    /**
     * AviaryResourceServiceSkeleton.h
     *
     * This file was auto-generated from WSDL for "AviaryResourceService|http://grid.redhat.com/aviary-resource/" service
     * by the WSO2 WSF/CPP Version: 1.0
     * AviaryResourceServiceSkeleton WSO2 WSF/CPP Skeleton for the Service Header File
     */
#ifndef AVIARYRESOURCESERVICESKELETON_H
#define AVIARYRESOURCESERVICESKELETON_H

    #include <OMElement.h>
    #include <MessageContext.h>
   
     #include <AviaryResource_GetAttributes.h>
    
     #include <AviaryResource_GetAttributesResponse.h>
    
namespace com_redhat_grid_aviary_resource{
    

   /** we have to reserve some error codes for adb and for custom messages */
    #define AVIARYRESOURCESERVICESKELETON_ERROR_CODES_START (AXIS2_ERROR_LAST + 2500)

    typedef enum
    {
        AVIARYRESOURCESERVICESKELETON_ERROR_NONE = AVIARYRESOURCESERVICESKELETON_ERROR_CODES_START,

        AVIARYRESOURCESERVICESKELETON_ERROR_LAST
    } AviaryResourceServiceSkeleton_error_codes;

    


class AviaryResourceServiceSkeleton
{
        public:
            AviaryResourceServiceSkeleton(){}


     




		 


        /**
         * Auto generated method declaration
         * for "getAttributes|http://grid.redhat.com/aviary-resource/" operation.
         * 
         * @param _getAttributes of the AviaryResource::GetAttributes
         *
         * @return AviaryResource::GetAttributesResponse*
         */
        

         virtual 
        AviaryResource::GetAttributesResponse* getAttributes(wso2wsf::MessageContext *outCtx ,AviaryResource::GetAttributes* _getAttributes);


     



};


}



        
#endif // AVIARYRESOURCESERVICESKELETON_H
    

