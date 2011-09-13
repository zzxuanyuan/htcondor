

        #ifndef AviaryCommon_ENDPOINTID_H
        #define AviaryCommon_ENDPOINTID_H

       /**
        * EndpointID.h
        *
        * This file was auto-generated from WSDL
        * by the Apache Axis2/Java version: 1.0  Built on : Sep 07, 2011 (03:40:57 EDT)
        */

       /**
        *  EndpointID class
        */

        namespace AviaryCommon{
            class EndpointID;
        }
        

        
       #include "AviaryCommon_ResourceType.h"
          

        #include <stdio.h>
        #include <OMElement.h>
        #include <ServiceClient.h>
        #include <ADBDefines.h>

namespace AviaryCommon
{
        
        

        class EndpointID {

        private:
             AviaryCommon::ResourceType* property_Type;

                
                bool isValidType;
            std::string property_Name;

                
                bool isValidName;
            axutil_uri_t* property_Url;

                
                bool isValidUrl;
            

        /*** Private methods ***/
          

        bool WSF_CALL
        setTypeNil();
            

        bool WSF_CALL
        setNameNil();
            

        bool WSF_CALL
        setUrlNil();
            



        /******************************* public functions *********************************/

        public:

        /**
         * Constructor for class EndpointID
         */

        EndpointID();

        /**
         * Destructor EndpointID
         */
        ~EndpointID();


       

        /**
         * Constructor for creating EndpointID
         * @param 
         * @param Type AviaryCommon::ResourceType*
         * @param Name std::string
         * @param Url axutil_uri_t*
         * @return newly created EndpointID object
         */
        EndpointID(AviaryCommon::ResourceType* arg_Type,std::string arg_Name,axutil_uri_t* arg_Url);
        
        
        /********************************** Class get set methods **************************************/
        
        

        /**
         * Getter for type. 
         * @return AviaryCommon::ResourceType*
         */
        WSF_EXTERN AviaryCommon::ResourceType* WSF_CALL
        getType();

        /**
         * Setter for type.
         * @param arg_Type AviaryCommon::ResourceType*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setType(AviaryCommon::ResourceType*  arg_Type);

        /**
         * Re setter for type
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetType();
        
        

        /**
         * Getter for name. 
         * @return std::string*
         */
        WSF_EXTERN std::string WSF_CALL
        getName();

        /**
         * Setter for name.
         * @param arg_Name std::string*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setName(const std::string  arg_Name);

        /**
         * Re setter for name
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetName();
        
        

        /**
         * Getter for url. 
         * @return axutil_uri_t*
         */
        WSF_EXTERN axutil_uri_t* WSF_CALL
        getUrl();

        /**
         * Setter for url.
         * @param arg_Url axutil_uri_t*
         * @return true on success, false otherwise
         */
        WSF_EXTERN bool WSF_CALL
        setUrl(axutil_uri_t*  arg_Url);

        /**
         * Re setter for url
         * @return true on success, false
         */
        WSF_EXTERN bool WSF_CALL
        resetUrl();
        


        /******************************* Checking and Setting NIL values *********************************/
        

        /**
         * NOTE: set_nil is only available for nillable properties
         */

        

        /**
         * Check whether type is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isTypeNil();


        

        /**
         * Check whether name is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isNameNil();


        

        /**
         * Check whether url is Nill
         * @return true if the element is Nil, false otherwise
         */
        bool WSF_CALL
        isUrlNil();


        

        /**************************** Serialize and De serialize functions ***************************/
        /*********** These functions are for use only inside the generated code *********************/

        
        /**
         * Deserialize the ADB object to an XML
         * @param dp_parent double pointer to the parent node to be deserialized
         * @param dp_is_early_node_valid double pointer to a flag (is_early_node_valid?)
         * @param dont_care_minoccurs Dont set errors on validating minoccurs, 
         *              (Parent will order this in a case of choice)
         * @return true on success, false otherwise
         */
        bool WSF_CALL
        deserialize(axiom_node_t** omNode, bool *isEarlyNodeValid, bool dontCareMinoccurs);
                         
            

       /**
         * Declare namespace in the most parent node 
         * @param parent_element parent element
         * @param namespaces hash of namespace uri to prefix
         * @param next_ns_index pointer to an int which contain the next namespace index
         */
        void WSF_CALL
        declareParentNamespaces(axiom_element_t *parent_element, axutil_hash_t *namespaces, int *next_ns_index);


        

        /**
         * Serialize the ADB object to an xml
         * @param EndpointID_om_node node to serialize from
         * @param EndpointID_om_element parent element to serialize from
         * @param tag_closed Whether the parent tag is closed or not
         * @param namespaces hash of namespace uris to prefixes
         * @param next_ns_index an int which contains the next namespace index
         * @return axiom_node_t on success,NULL otherwise.
         */
        axiom_node_t* WSF_CALL
        serialize(axiom_node_t* EndpointID_om_node, axiom_element_t *EndpointID_om_element, int tag_closed, axutil_hash_t *namespaces, int *next_ns_index);

        /**
         * Check whether the EndpointID is a particle class (E.g. group, inner sequence)
         * @return true if this is a particle class, false otherwise.
         */
        bool WSF_CALL
        isParticle();



        /******************************* get the value by the property number  *********************************/
        /************NOTE: This method is introduced to resolve a problem in unwrapping mode *******************/

      
        

        /**
         * Getter for type by property number (1)
         * @return AviaryCommon::ResourceType
         */

        AviaryCommon::ResourceType* WSF_CALL
        getProperty1();

    
        

        /**
         * Getter for name by property number (2)
         * @return std::string
         */

        std::string WSF_CALL
        getProperty2();

    
        

        /**
         * Getter for url by property number (3)
         * @return axutil_uri_t*
         */

        axutil_uri_t* WSF_CALL
        getProperty3();

    

};

}        
 #endif /* ENDPOINTID_H */
    

