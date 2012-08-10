

        /**
         * AviaryResourceService.cpp
         *
         * This file was auto-generated from WSDL for "AviaryResourceService|http://grid.redhat.com/aviary-resource/" service
         * by the Apache Axis2 version: 1.0  Built on : Jul 18, 2012 (11:51:19 EDT)
         *  AviaryResourceService
         */

        #include "AviaryResourceServiceSkeleton.h"
        #include "AviaryResourceService.h"  
        #include <ServiceSkeleton.h>
        #include <stdio.h>
        #include <axis2_svc.h>
        #include <Environment.h>
        #include <axiom_soap.h>


        using namespace wso2wsf;
        
        using namespace com_redhat_grid_aviary_resource;
        

        /** Load the service into axis2 engine */
        WSF_SERVICE_INIT(AviaryResourceService)

          
         /**
          * function to free any soap input headers
          */
         AviaryResourceService::AviaryResourceService()
	{
          skel = wsfGetAviaryResourceServiceSkeleton();
    }


	bool WSF_CALL
	AviaryResourceService::init()
	{

      return true;
	}


	AviaryResourceService::~AviaryResourceService()
	{
    }


     

     




	/*
	 * This method invokes the right service method
	 */
	OMElement* WSF_CALL
	AviaryResourceService::invoke(OMElement *omEle, MessageContext *msgCtx)
	{
         /* Using the function name, invoke the corresponding method
          */

          axis2_op_ctx_t *operation_ctx = NULL;
          axis2_op_t *operation = NULL;
          axutil_qname_t *op_qname = NULL;
          axis2_char_t *op_name = NULL;
          axis2_msg_ctx_t *in_msg_ctx = NULL;
          
          axiom_soap_envelope_t *req_soap_env = NULL;
          axiom_soap_header_t *req_soap_header = NULL;
          axiom_soap_envelope_t *res_soap_env = NULL;
          axiom_soap_header_t *res_soap_header = NULL;

          axiom_node_t *ret_node = NULL;
          axiom_node_t *input_header = NULL;
          axiom_node_t *output_header = NULL;
          axiom_node_t *header_base_node = NULL;
          axis2_msg_ctx_t *msg_ctx = NULL;
          axiom_node_t* content_node = omEle->getAxiomNode();

          
            AviaryResource::GetAttributesResponse* ret_val1;
            AviaryResource::GetAttributes* input_val1;
            
       
          msg_ctx = msgCtx->getAxis2MessageContext();
          operation_ctx = axis2_msg_ctx_get_op_ctx(msg_ctx, Environment::getEnv());
          operation = axis2_op_ctx_get_op(operation_ctx, Environment::getEnv());
          op_qname = (axutil_qname_t *)axis2_op_get_qname(operation, Environment::getEnv());
          op_name = axutil_qname_get_localpart(op_qname, Environment::getEnv());

          if (op_name)
          {
               

                if ( axutil_strcmp(op_name, "getAttributes") == 0 )
                {

                    
                    input_val1 =
                        
                        new AviaryResource::GetAttributes();
                        if( AXIS2_FAILURE ==  input_val1->deserialize(&content_node, NULL, false))
                        {
                                        
                            AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_DATA_ELEMENT_IS_NULL, AXIS2_FAILURE);
                            AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the AviaryResource::GetAttributes_deserialize: "
                                        "This should be due to an invalid XML");
                            return NULL;      
                        }
                        
                        //AviaryResourceServiceSkeleton skel;
                        ret_val1 =  skel->getAttributes(msgCtx ,input_val1);
                    
                        if ( NULL == ret_val1 )
                        {
                            
                                delete input_val1;
                            
                            return NULL; 
                        }
                        ret_node = 
                                            ret_val1->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                                            delete ret_val1;
                                        
                                            delete input_val1;
                                        

                        return new OMElement(NULL,ret_node);
                    

                    /* since this has no output params it just returns NULL */                    
                    

                }
             
             }
            
          AXIS2_LOG_ERROR(Environment::getEnv()->log, AXIS2_LOG_SI, "AviaryResourceService service ERROR: invalid OM parameters in request\n");
          return NULL;
    }

    OMElement* WSF_CALL
    AviaryResourceService::onFault(OMElement* omEle)
	{
		axiom_node_t *error_node = NULL;
		axiom_element_t *error_ele = NULL;
        axutil_error_codes_t error_code;
        axiom_node_t *node = omEle->getAxiomNode();
        error_code = (axutil_error_codes_t)Environment::getEnv()->error->error_number;

        if(error_code <= AVIARYRESOURCESERVICESKELETON_ERROR_NONE ||
                error_code >= AVIARYRESOURCESERVICESKELETON_ERROR_LAST )
        {
            error_ele = axiom_element_create(Environment::getEnv(), node, "fault", NULL,
                            &error_node);
            axiom_element_set_text(error_ele, Environment::getEnv(), "AviaryResourceService|http://grid.redhat.com/aviary-resource/ failed",
                            error_node);
        }
        

		return new OMElement(NULL,error_node);
	}

    

