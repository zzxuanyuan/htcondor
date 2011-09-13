
      /**
       * AviaryLocatorServiceStub.cpp
       *
       * This file was auto-generated from WSDL for "AviaryLocatorService|http://grid.redhat.com/aviary-locator/" service
       * by the Apache Axis2/Java version: 1.0  Built on : Sep 07, 2011 (03:40:38 EDT)
       */

      #include "AviaryLocatorServiceStub.h"
      #include "IAviaryLocatorServiceCallback.h"
      #include <axis2_msg.h>
      #include <axis2_policy_include.h>
      #include <neethi_engine.h>
      #include <Stub.h>
      #include <Environment.h>
      #include <WSFError.h>
      
      using namespace std;
      using namespace wso2wsf;
        
      using namespace com_redhat_grid_aviary_locator;
        
      /**
       * AviaryLocatorServiceStub CPP implementation
       */
       AviaryLocatorServiceStub::AviaryLocatorServiceStub(std::string& clientHome)
        {
                if(clientHome.empty())
                {
                   cout<<"Please specify the client home";
                }
                std::string endpointUri= getEndpointUriOfAviaryLocatorService();

                init(clientHome,endpointUri);

                populateServicesForAviaryLocatorService();


        }


      AviaryLocatorServiceStub::AviaryLocatorServiceStub(std::string& clientHome,std::string& endpointURI)
      {
         std::string endpointUri;

         if(clientHome.empty())
         {
            cout<<"Please specify the client home";
         }
         endpointUri = endpointURI;

         if (endpointUri.empty())
         {
            endpointUri = getEndpointUriOfAviaryLocatorService();
         }


         init(clientHome,endpointUri);

         populateServicesForAviaryLocatorService();

      }


      void WSF_CALL
      AviaryLocatorServiceStub::populateServicesForAviaryLocatorService()
      {
         axis2_svc_client_t *svc_client = NULL;
         axutil_qname_t *svc_qname =  NULL;
         axutil_qname_t *op_qname =  NULL;
         axis2_svc_t *svc = NULL;
         axis2_op_t *op = NULL;
         axis2_op_t *annon_op = NULL;
         axis2_msg_t *msg_out = NULL;
         axis2_msg_t *msg_in = NULL;
         axis2_msg_t *msg_out_fault = NULL;
         axis2_msg_t *msg_in_fault = NULL;
         axis2_policy_include_t *policy_include = NULL;

         axis2_desc_t *desc = NULL;
         axiom_node_t *policy_node = NULL;
         axiom_element_t *policy_root_ele = NULL;
         neethi_policy_t *neethi_policy = NULL;


         /* Modifying the Service */
	 svc_client = serviceClient->getAxis2SvcClient();
         svc = (axis2_svc_t*)axis2_svc_client_get_svc( svc_client, Environment::getEnv() );

         annon_op = axis2_svc_get_op_with_name(svc, Environment::getEnv(), AXIS2_ANON_OUT_IN_OP);
         msg_out = axis2_op_get_msg(annon_op, Environment::getEnv(), AXIS2_MSG_OUT);
         msg_in = axis2_op_get_msg(annon_op, Environment::getEnv(), AXIS2_MSG_IN);
         msg_out_fault = axis2_op_get_msg(annon_op, Environment::getEnv(), AXIS2_MSG_OUT_FAULT);
         msg_in_fault = axis2_op_get_msg(annon_op, Environment::getEnv(), AXIS2_MSG_IN_FAULT);

         svc_qname = axutil_qname_create(Environment::getEnv(),"AviaryLocatorService" ,NULL, NULL);
         axis2_svc_set_qname (svc, Environment::getEnv(), svc_qname);
		 axutil_qname_free(svc_qname,Environment::getEnv());

         /* creating the operations*/

         
           op_qname = axutil_qname_create(Environment::getEnv(),
                                         "register" ,
                                         "http://grid.redhat.com/aviary-locator/",
                                         NULL);
           op = axis2_op_create_with_qname(Environment::getEnv(), op_qname);
           axutil_qname_free(op_qname,Environment::getEnv());

           
           axis2_op_set_msg_exchange_pattern(op, Environment::getEnv(), AXIS2_MEP_URI_OUT_IN);
             
           axis2_msg_increment_ref(msg_out, Environment::getEnv());
           axis2_msg_increment_ref(msg_in, Environment::getEnv());
           axis2_msg_increment_ref(msg_out_fault, Environment::getEnv());
           axis2_msg_increment_ref(msg_in_fault, Environment::getEnv());
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT, msg_out);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN, msg_in);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT_FAULT, msg_out_fault);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN_FAULT, msg_in_fault);
       
           
           axis2_svc_add_op(svc, Environment::getEnv(), op);
         
           op_qname = axutil_qname_create(Environment::getEnv(),
                                         "lookup" ,
                                         "http://grid.redhat.com/aviary-locator/",
                                         NULL);
           op = axis2_op_create_with_qname(Environment::getEnv(), op_qname);
           axutil_qname_free(op_qname,Environment::getEnv());

           
           axis2_op_set_msg_exchange_pattern(op, Environment::getEnv(), AXIS2_MEP_URI_OUT_IN);
             
           axis2_msg_increment_ref(msg_out, Environment::getEnv());
           axis2_msg_increment_ref(msg_in, Environment::getEnv());
           axis2_msg_increment_ref(msg_out_fault, Environment::getEnv());
           axis2_msg_increment_ref(msg_in_fault, Environment::getEnv());
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT, msg_out);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN, msg_in);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_OUT_FAULT, msg_out_fault);
           axis2_op_add_msg(op, Environment::getEnv(), AXIS2_MSG_IN_FAULT, msg_in_fault);
       
           
           axis2_svc_add_op(svc, Environment::getEnv(), op);
         
      }

      /**
       *return end point picked from wsdl
       */
      std::string WSF_CALL
      AviaryLocatorServiceStub::getEndpointUriOfAviaryLocatorService()
      {
        std::string endpoint_uri;
        /* set the address from here */
        
        endpoint_uri = string("http://localhost");
            
        return endpoint_uri;
      }


  
         /**
          * Auto generated method signature
          * For "register|http://grid.redhat.com/aviary-locator/" operation.
          *
          * @param _register12 of the AviaryLocator::Register
          *
          * @return AviaryLocator::RegisterResponse*
          */

         AviaryLocator::RegisterResponse* WSF_CALL AviaryLocatorServiceStub::_register(AviaryLocator::Register*  _register12)
         {
            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;
            axiom_node_t *ret_node = NULL;

            const axis2_char_t *soap_action = NULL;
            axutil_qname_t *op_qname =  NULL;
            axiom_node_t *payload = NULL;
            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            AviaryLocator::RegisterResponse* ret_val;
            
                                payload = _register12->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           
	    svc_client = serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
                AXIS2_LOG_ERROR(Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
                return (AviaryLocator::RegisterResponse*)NULL;
            }
            soap_act = axis2_options_get_soap_action( options, Environment::getEnv() );
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-locator/register";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-locator/register");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);    
            }

            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             
            ret_node =  axis2_svc_client_send_receive_with_op_qname( svc_client, Environment::getEnv(), op_qname, payload);
 
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);    
              
              axis2_options_set_action( options, Environment::getEnv(), NULL);
            }
            if(soap_act)
            {
              axutil_string_free(soap_act, Environment::getEnv());
            }

            
                    if ( NULL == ret_node )
                    {
                        return (AviaryLocator::RegisterResponse*)NULL;
                    }
                    ret_val = new AviaryLocator::RegisterResponse();

                    if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                    {
                        if(ret_val != NULL)
                        {
                           delete ret_val;
                        }

                        AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the _deserialize: "
                                                                "This should be due to an invalid XML");
                        return (AviaryLocator::RegisterResponse*)NULL;
                    }

                   
                            return ret_val;
                       
        }
        
         /**
          * Auto generated method signature
          * For "lookup|http://grid.redhat.com/aviary-locator/" operation.
          *
          * @param _lookup14 of the AviaryLocator::Lookup
          *
          * @return AviaryLocator::LookupResponse*
          */

         AviaryLocator::LookupResponse* WSF_CALL AviaryLocatorServiceStub::lookup(AviaryLocator::Lookup*  _lookup14)
         {
            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;
            axiom_node_t *ret_node = NULL;

            const axis2_char_t *soap_action = NULL;
            axutil_qname_t *op_qname =  NULL;
            axiom_node_t *payload = NULL;
            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            AviaryLocator::LookupResponse* ret_val;
            
                                payload = _lookup14->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           
	    svc_client = serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
                AXIS2_LOG_ERROR(Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
                return (AviaryLocator::LookupResponse*)NULL;
            }
            soap_act = axis2_options_get_soap_action( options, Environment::getEnv() );
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-locator/lookup";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-locator/lookup");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);    
            }

            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             
            ret_node =  axis2_svc_client_send_receive_with_op_qname( svc_client, Environment::getEnv(), op_qname, payload);
 
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);    
              
              axis2_options_set_action( options, Environment::getEnv(), NULL);
            }
            if(soap_act)
            {
              axutil_string_free(soap_act, Environment::getEnv());
            }

            
                    if ( NULL == ret_node )
                    {
                        return (AviaryLocator::LookupResponse*)NULL;
                    }
                    ret_val = new AviaryLocator::LookupResponse();

                    if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                    {
                        if(ret_val != NULL)
                        {
                           delete ret_val;
                        }

                        AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the _deserialize: "
                                                                "This should be due to an invalid XML");
                        return (AviaryLocator::LookupResponse*)NULL;
                    }

                   
                            return ret_val;
                       
        }
        

        struct axis2_stub_AviaryLocatorService__register_callback_data
        {   
            IAviaryLocatorServiceCallback *callback;
          
        };

        static axis2_status_t WSF_CALL axis2_stub_on_error_AviaryLocatorService__register(axis2_callback_t *axis_callback, const axutil_env_t *env, int exception)
        {
            struct axis2_stub_AviaryLocatorService__register_callback_data* callback_data = NULL;
            callback_data = (struct axis2_stub_AviaryLocatorService__register_callback_data*)axis2_callback_get_data(axis_callback);
        
            IAviaryLocatorServiceCallback* callback = NULL;
            callback = callback_data->callback;
            callback->receiveError__register(exception);
            return AXIS2_SUCCESS;
        } 

        axis2_status_t  AXIS2_CALL axis2_stub_on_complete_AviaryLocatorService__register(axis2_callback_t *axis_callback, const axutil_env_t *env)
        {
            struct axis2_stub_AviaryLocatorService__register_callback_data* callback_data = NULL;
            axis2_status_t status = AXIS2_SUCCESS;
            AviaryLocator::RegisterResponse* ret_val;
            

            axiom_node_t *ret_node = NULL;
            axiom_soap_envelope_t *soap_envelope = NULL;

            

            IAviaryLocatorServiceCallback *callback = NULL;

            callback_data = (struct axis2_stub_AviaryLocatorService__register_callback_data*)axis2_callback_get_data(axis_callback);

            callback = callback_data->callback;

            soap_envelope = axis2_callback_get_envelope(axis_callback, Environment::getEnv());
            if(soap_envelope)
            {
                axiom_soap_body_t *soap_body;
                soap_body = axiom_soap_envelope_get_body(soap_envelope, Environment::getEnv());
                if(soap_body)
                {
                    axiom_soap_fault_t *soap_fault = NULL;
                    axiom_node_t *body_node = axiom_soap_body_get_base_node(soap_body, Environment::getEnv());

                      if(body_node)
                    {
                        ret_node = axiom_node_get_first_child(body_node, Environment::getEnv());
                    }
                }
                
                
            }


            
                    if(ret_node != NULL)
                    {
                        ret_val = new AviaryLocator::RegisterResponse();
     
                        if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                        {
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the LendResponse_deserialize: "
                                                                    "This should be due to an invalid XML");
                            delete ret_val;
                            ret_val = NULL;
                        }
                     }
                     else
                     {
                         ret_val = NULL; 
                     }

                     
                     callback->receiveResult__register(ret_val);
                         
 
            if(callback_data)
            {
                AXIS2_FREE(Environment::getEnv()->allocator, callback_data);
            }
            return AXIS2_SUCCESS;
        }

        /**
          * auto generated method signature for asynchronous invocations
          * for "register|http://grid.redhat.com/aviary-locator/" operation.
          * @param stub The stub
          * @param env environment ( mandatory)
          * @param _register12 of the AviaryLocator::Register
          * @param user_data user data to be accessed by the callbacks
          * @param on_complete callback to handle on complete
          * @param on_error callback to handle on error
          */

         void WSF_CALL
        AviaryLocatorServiceStub::start__register(AviaryLocator::Register*  _register12,
                                IAviaryLocatorServiceCallback* cb)
         {

            axis2_callback_t *callback = NULL;

            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;

            const axis2_char_t *soap_action = NULL;
            axiom_node_t *payload = NULL;

            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            
            
            struct axis2_stub_AviaryLocatorService__register_callback_data *callback_data;

            callback_data = (struct axis2_stub_AviaryLocatorService__register_callback_data*) AXIS2_MALLOC(Environment::getEnv()->allocator, 
                                    sizeof(struct axis2_stub_AviaryLocatorService__register_callback_data));
            if(NULL == callback_data)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_NO_MEMORY, AXIS2_FAILURE);
                AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "Can not allocate memory for the callback data structures");
                return;
            }
            

            
                                payload = _register12->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           

	    svc_client =   serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
              AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
              AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
              return;
            }

            soap_act =axis2_options_get_soap_action (options, Environment::getEnv());
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-locator/register";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-locator/register");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);
            }
            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             

            callback = axis2_callback_create(Environment::getEnv());
            /* Set our on_complete function pointer to the callback object */
            axis2_callback_set_on_complete(callback, axis2_stub_on_complete_AviaryLocatorService__register);
            /* Set our on_error function pointer to the callback object */
            axis2_callback_set_on_error(callback, axis2_stub_on_error_AviaryLocatorService__register);

            callback_data->callback = cb;
            axis2_callback_set_data(callback, (void*)callback_data);

            /* Send request */
            axis2_svc_client_send_receive_non_blocking(svc_client, Environment::getEnv(), payload, callback);
            
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);
              
              axis2_options_set_action(options, Environment::getEnv(), NULL);
            }
         }

         

        struct axis2_stub_AviaryLocatorService_lookup_callback_data
        {   
            IAviaryLocatorServiceCallback *callback;
          
        };

        static axis2_status_t WSF_CALL axis2_stub_on_error_AviaryLocatorService_lookup(axis2_callback_t *axis_callback, const axutil_env_t *env, int exception)
        {
            struct axis2_stub_AviaryLocatorService_lookup_callback_data* callback_data = NULL;
            callback_data = (struct axis2_stub_AviaryLocatorService_lookup_callback_data*)axis2_callback_get_data(axis_callback);
        
            IAviaryLocatorServiceCallback* callback = NULL;
            callback = callback_data->callback;
            callback->receiveError_lookup(exception);
            return AXIS2_SUCCESS;
        } 

        axis2_status_t  AXIS2_CALL axis2_stub_on_complete_AviaryLocatorService_lookup(axis2_callback_t *axis_callback, const axutil_env_t *env)
        {
            struct axis2_stub_AviaryLocatorService_lookup_callback_data* callback_data = NULL;
            axis2_status_t status = AXIS2_SUCCESS;
            AviaryLocator::LookupResponse* ret_val;
            

            axiom_node_t *ret_node = NULL;
            axiom_soap_envelope_t *soap_envelope = NULL;

            

            IAviaryLocatorServiceCallback *callback = NULL;

            callback_data = (struct axis2_stub_AviaryLocatorService_lookup_callback_data*)axis2_callback_get_data(axis_callback);

            callback = callback_data->callback;

            soap_envelope = axis2_callback_get_envelope(axis_callback, Environment::getEnv());
            if(soap_envelope)
            {
                axiom_soap_body_t *soap_body;
                soap_body = axiom_soap_envelope_get_body(soap_envelope, Environment::getEnv());
                if(soap_body)
                {
                    axiom_soap_fault_t *soap_fault = NULL;
                    axiom_node_t *body_node = axiom_soap_body_get_base_node(soap_body, Environment::getEnv());

                      if(body_node)
                    {
                        ret_node = axiom_node_get_first_child(body_node, Environment::getEnv());
                    }
                }
                
                
            }


            
                    if(ret_node != NULL)
                    {
                        ret_val = new AviaryLocator::LookupResponse();
     
                        if(ret_val->deserialize(&ret_node, NULL, AXIS2_FALSE ) == AXIS2_FAILURE)
                        {
                            WSF_LOG_ERROR_MSG( Environment::getEnv()->log, AXIS2_LOG_SI, "NULL returned from the LendResponse_deserialize: "
                                                                    "This should be due to an invalid XML");
                            delete ret_val;
                            ret_val = NULL;
                        }
                     }
                     else
                     {
                         ret_val = NULL; 
                     }

                     
                     callback->receiveResult_lookup(ret_val);
                         
 
            if(callback_data)
            {
                AXIS2_FREE(Environment::getEnv()->allocator, callback_data);
            }
            return AXIS2_SUCCESS;
        }

        /**
          * auto generated method signature for asynchronous invocations
          * for "lookup|http://grid.redhat.com/aviary-locator/" operation.
          * @param stub The stub
          * @param env environment ( mandatory)
          * @param _lookup14 of the AviaryLocator::Lookup
          * @param user_data user data to be accessed by the callbacks
          * @param on_complete callback to handle on complete
          * @param on_error callback to handle on error
          */

         void WSF_CALL
        AviaryLocatorServiceStub::start_lookup(AviaryLocator::Lookup*  _lookup14,
                                IAviaryLocatorServiceCallback* cb)
         {

            axis2_callback_t *callback = NULL;

            axis2_svc_client_t *svc_client = NULL;
            axis2_options_t *options = NULL;

            const axis2_char_t *soap_action = NULL;
            axiom_node_t *payload = NULL;

            axis2_bool_t is_soap_act_set = AXIS2_TRUE;
            axutil_string_t *soap_act = NULL;

            
            
            struct axis2_stub_AviaryLocatorService_lookup_callback_data *callback_data;

            callback_data = (struct axis2_stub_AviaryLocatorService_lookup_callback_data*) AXIS2_MALLOC(Environment::getEnv()->allocator, 
                                    sizeof(struct axis2_stub_AviaryLocatorService_lookup_callback_data));
            if(NULL == callback_data)
            {
                AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_NO_MEMORY, AXIS2_FAILURE);
                AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "Can not allocate memory for the callback data structures");
                return;
            }
            

            
                                payload = _lookup14->serialize(NULL, NULL, AXIS2_TRUE, NULL, NULL);
                           

	    svc_client =   serviceClient->getAxis2SvcClient();
            
           
            
            

	    options = clientOptions->getAxis2Options();
            if (NULL == options)
            {
              AXIS2_ERROR_SET(Environment::getEnv()->error, AXIS2_ERROR_INVALID_NULL_PARAM, AXIS2_FAILURE);
              AXIS2_LOG_ERROR( Environment::getEnv()->log, AXIS2_LOG_SI, "options is null in stub");
              return;
            }

            soap_act =axis2_options_get_soap_action (options, Environment::getEnv());
            if (NULL == soap_act)
            {
              is_soap_act_set = AXIS2_FALSE;
              soap_action = "http://grid.redhat.com/aviary-locator/lookup";
              soap_act = axutil_string_create(Environment::getEnv(), "http://grid.redhat.com/aviary-locator/lookup");
              axis2_options_set_soap_action(options, Environment::getEnv(), soap_act);
            }
            
            axis2_options_set_soap_version(options, Environment::getEnv(), AXIOM_SOAP11);
             

            callback = axis2_callback_create(Environment::getEnv());
            /* Set our on_complete function pointer to the callback object */
            axis2_callback_set_on_complete(callback, axis2_stub_on_complete_AviaryLocatorService_lookup);
            /* Set our on_error function pointer to the callback object */
            axis2_callback_set_on_error(callback, axis2_stub_on_error_AviaryLocatorService_lookup);

            callback_data->callback = cb;
            axis2_callback_set_data(callback, (void*)callback_data);

            /* Send request */
            axis2_svc_client_send_receive_non_blocking(svc_client, Environment::getEnv(), payload, callback);
            
            if (!is_soap_act_set)
            {
              
              axis2_options_set_soap_action(options, Environment::getEnv(), NULL);
              
              axis2_options_set_action(options, Environment::getEnv(), NULL);
            }
         }

         

