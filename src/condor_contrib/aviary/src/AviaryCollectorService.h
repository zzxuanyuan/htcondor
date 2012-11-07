

          #ifndef AVIARYCOLLECTORSERVICE_H
          #define AVIARYCOLLECTORSERVICE_H

        /**
         * AviaryCollectorService.h
         *
         * This file was auto-generated from WSDL for "AviaryCollectorService|http://grid.redhat.com/aviary-collector/" service
         * by the Apache Axis2 version: 1.0  Built on : Sep 18, 2012 (08:44:03 EDT)
         *  AviaryCollectorService
         */

#include <ServiceSkeleton.h>
#include <stdio.h>
#include <axis2_svc.h>

using namespace wso2wsf;


using namespace com_redhat_grid_aviary_collector;



#define WSF_SERVICE_SKEL_INIT(class_name) \
AviaryCollectorServiceSkeleton* wsfGetAviaryCollectorServiceSkeleton(){ return new class_name(); }

AviaryCollectorServiceSkeleton* wsfGetAviaryCollectorServiceSkeleton(); 



        class AviaryCollectorService : public ServiceSkeleton
        {
            private:
                AviaryCollectorServiceSkeleton *skel;

            public:

               union {
                     
               } fault;


              WSF_EXTERN WSF_CALL AviaryCollectorService();

              OMElement* WSF_CALL invoke(OMElement *message, MessageContext *msgCtx);

              OMElement* WSF_CALL onFault(OMElement *message);

              virtual bool WSF_CALL init();

              ~AviaryCollectorService(); 
      };



#endif    //     AVIARYCOLLECTORSERVICE_H

    

