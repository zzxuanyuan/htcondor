

          #ifndef AVIARYRESOURCESERVICE_H
          #define AVIARYRESOURCESERVICE_H

        /**
         * AviaryResourceService.h
         *
         * This file was auto-generated from WSDL for "AviaryResourceService|http://grid.redhat.com/aviary-resource/" service
         * by the Apache Axis2 version: 1.0  Built on : Jul 18, 2012 (11:51:19 EDT)
         *  AviaryResourceService
         */

#include <ServiceSkeleton.h>
#include <stdio.h>
#include <axis2_svc.h>

using namespace wso2wsf;


using namespace com_redhat_grid_aviary_resource;



#define WSF_SERVICE_SKEL_INIT(class_name) \
AviaryResourceServiceSkeleton* wsfGetAviaryResourceServiceSkeleton(){ return new class_name(); }

AviaryResourceServiceSkeleton* wsfGetAviaryResourceServiceSkeleton(); 



        class AviaryResourceService : public ServiceSkeleton
        {
            private:
                AviaryResourceServiceSkeleton *skel;

            public:

               union {
                     
               } fault;


              WSF_EXTERN WSF_CALL AviaryResourceService();

              OMElement* WSF_CALL invoke(OMElement *message, MessageContext *msgCtx);

              OMElement* WSF_CALL onFault(OMElement *message);

              virtual bool WSF_CALL init();

              ~AviaryResourceService(); 
      };



#endif    //     AVIARYRESOURCESERVICE_H

    

