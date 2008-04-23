/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_string.h"
#include "string_list.h"
#include "MyString.h"
#include "util_lib_proto.h"
#include "stat_wrapper.h"
#include "amazongahp_common.h"
#include "amazonCommands.h"

// For gsoap
#include <stdsoap2.h>
#include <smdevp.h> 
#include "ec2H.h"
#include <wsseapi.h>
#include "AmazonEC2Binding.nsmap"

// For base64 encoding
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

// Caller need to free the returned pointer
static char* base64_encode(const unsigned char *input, int length)
{
	BIO *bmem, *b64;
	BUF_MEM *bptr;

	b64 = BIO_new(BIO_f_base64());
	bmem = BIO_new(BIO_s_mem());
	b64 = BIO_push(b64, bmem);
	BIO_write(b64, input, length);
	BIO_flush(b64);
	BIO_get_mem_ptr(b64, &bptr);

	char *buff = (char *)malloc(bptr->length);
	ASSERT(buff);
	memcpy(buff, bptr->data, bptr->length-1);
	buff[bptr->length-1] = 0;
	BIO_free_all(b64);

	return buff;
}

void
AmazonRequest::ParseSoapError(const char* callerstring) 
{
	if( !m_soap ) {
		return;
	}

	const char** code = NULL;
	code = soap_faultcode(m_soap);

	if( *code ) {
		// NOTE: The faultcode appears to have a qualifying 
		// namespace, which we need to strip
		char *s = strchr(*code, ':');
		if( s ) {
			s++;
			error_code = s;
		}
	}

	const char** reason = NULL;
	reason = soap_faultstring(m_soap);
	if( *reason ) {
		error_msg = *reason;
	}

	char buffer[512];

	// We use the buffer as a string, so lets be safe and make
	// sure when you write to the front of it you always have a
	// NULL at the end
	memset((void *)buffer, 0, sizeof(buffer));

	soap_sprint_fault(m_soap, buffer, sizeof(buffer));
	if( strlen(buffer) ) { 
		vmprintf(D_ALWAYS, "Call to %s failed: %s\n", 
				callerstring? callerstring:"", buffer);
	}
	return;
}

bool 
AmazonRequest::SetupSoap(void)
{
	if( secretkeyfile.IsEmpty() ) {
		vmprintf(D_ALWAYS, "There is no privatekey\n");
		return false;
	}
	if( accesskeyfile.IsEmpty() ) {
		vmprintf(D_ALWAYS, "There is no accesskeyfile\n");
		return false;
	}

	if( m_soap ) {
		CleanupSoap();
	}

	// Must use canoicalization
	if( !(m_soap = soap_new1(SOAP_XML_CANONICAL))) {
		error_msg = "Failed to create SOAP context";
		vmprintf(D_ALWAYS, "%s\n", error_msg.Value());
		return false;
	}

	if (soap_register_plugin(m_soap, soap_wsse)) {
		ParseSoapError("setup WS-Security plugin");
		return false;
	}

	FILE *file = NULL;
	if ((file = safe_fopen_wrapper(secretkeyfile.Value(), "r"))) {
		m_rsa_privk = PEM_read_PrivateKey(file, NULL, NULL, NULL);
		fclose(file);

		if( !m_rsa_privk ) {
			error_msg.sprintf("Could not read private RSA key from: %s",
					secretkeyfile.Value());
			vmprintf(D_ALWAYS, "%s\n", error_msg.Value());
			return false;
		}
	} else {
		error_msg.sprintf("Could not read private key file: %s",
				secretkeyfile.Value());
		vmprintf(D_ALWAYS, "%s\n", error_msg.Value());
		return false;
	}

	if ((file = safe_fopen_wrapper(accesskeyfile.Value(), "r"))) {
		m_cert = PEM_read_X509(file, NULL, NULL, NULL);
		fclose(file);

		if (!m_cert) {
			error_msg.sprintf("Could not read accesskeyfile from: %s",
					accesskeyfile.Value());
			vmprintf(D_ALWAYS, "%s\n", error_msg.Value());
			return false;
		}
	} else {
		error_msg.sprintf("Could not read accesskeyfile file: %s",
				accesskeyfile.Value());
		vmprintf(D_ALWAYS, "%s\n", error_msg.Value());
		return false;
	}

	// Timestamp must be signed, the "Timestamp" value just needs
	// to be non-NULL
	if( soap_wsse_add_Timestamp(m_soap, "Timestamp", 10)) { 
		error_msg = "Failed to sign timestamp";
		vmprintf(D_ALWAYS, "%s\n", error_msg.Value());
		return false;
	}

	if( soap_wsse_add_BinarySecurityTokenX509(m_soap, "BinarySecurityToken", m_cert)) {
		error_msg.sprintf("Could not set BinarySecurityToken from: %s", 
				accesskeyfile.Value());
		vmprintf(D_ALWAYS, "%s\n", error_msg.Value());
		return false;
	}

	// May be optional
	if( soap_wsse_add_KeyInfo_SecurityTokenReferenceX509(m_soap, "#X509Token") ) {
		error_msg = "Failed to setup SecurityTokenReference";
		vmprintf(D_ALWAYS, "%s\n", error_msg.Value());
		return false;
	}

	// Body must be signed
	if( soap_wsse_sign_body(m_soap, SOAP_SMD_SIGN_RSA_SHA1, m_rsa_privk, 0)) {
		error_msg = "Failed to setup signing of SOAP body";
		vmprintf(D_ALWAYS, "%s\n", error_msg.Value());
		return false;
	}
	
	return true;
}

void
AmazonRequest::CleanupSoap(void)
{
	if (m_rsa_privk) {
		EVP_PKEY_free(m_rsa_privk);
		m_rsa_privk = NULL;
	}

	if (m_cert) {
		X509_free(m_cert);
		m_cert = NULL;
	}

	if( m_soap ) {
		soap_wsse_delete_Security(m_soap);
		soap_end(m_soap);
		soap_done(m_soap);

		free(m_soap);
		m_soap = NULL;
	}
}

bool
AmazonVMKeypairNames::gsoapRequest(void)
{
	if( !SetupSoap() ) {
		vmprintf(D_ALWAYS, "Failed to setup SOAP context\n");
		return false;
	}

	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(),
				secretkeyfile.GetCStr(), error_msg) ) {
		vmprintf(D_ALWAYS, "AmazonVMKeypairNames Error: %s\n", error_msg.Value());
		return false;
	}

	int code = -1;
	int i = 0;
	struct ec2__DescribeKeyPairsType request;
	struct ec2__DescribeKeyPairsResponseType response;

	// Want info on all keys...
	request.keySet = NULL;
	if (!(code = soap_call___ec2__DescribeKeyPairs(m_soap,
					AWS_URL,
					NULL,
					&request,
					&response))) {

		if( response.keySet && response.keySet->item ) {
			for (i = 0; i < response.keySet->__sizeitem; i++) {
				keynames.append(response.keySet->item[i]->keyName);
			}
		}
		return true;

	}else {
		// Error
		ParseSoapError("DescribeKeyPairs");
	}

	return false;
}

bool
AmazonVMCreateKeypair::gsoapRequest(void)
{
	if( !SetupSoap() ) {
		vmprintf(D_ALWAYS, "Failed to setup SOAP context\n");
		return false;
	}

	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		vmprintf(D_ALWAYS, "AmazonVMCreateKeyPair Error: %s\n", error_msg.Value());
		return false;
	}

	if( keyname.IsEmpty() ) {
		error_msg = "Empty_Keyname";
		vmprintf(D_ALWAYS, "AmazonVMCreateKeyPair Error: %s\n", error_msg.Value());
		return false;
	}

	if( strcmp(outputfile.Value(), NULL_FILE) ) { 
		has_outputfile = true;
	}

	// check if output file could be created
	if( has_outputfile ) { 
		if( check_create_file(outputfile.GetCStr()) == false ) {
			error_msg = "No_permission_for_keypair_outputfile";
			vmprintf(D_ALWAYS, "AmazonVMCreateKeypair Error: %s\n", error_msg.Value());
			return false;
		}
	}

	int code = -1;
	struct ec2__CreateKeyPairType request;
	struct ec2__CreateKeyPairResponseType response;

	request.keyName = (char *) keyname.GetCStr();

	if (!(code = soap_call___ec2__CreateKeyPair(m_soap,
					AWS_URL,
					NULL,
					&request,
					&response))) {

		if( has_outputfile && response.keyMaterial ) {

			FILE *fp = NULL;
			fp = safe_fopen_wrapper(outputfile.Value(), "w");
			if( !fp ) {
				error_msg.sprintf("failed to safe_fopen_wrapper %s in write mode: "
						"safe_fopen_wrapper returns %s", 
						outputfile.Value(), strerror(errno));
				vmprintf(D_ALWAYS, "%s\n", error_msg.Value());
				return false;
			}

			fprintf(fp,"%s", response.keyMaterial);
			fclose(fp);
		}
		return true;
	}else {
		// Error
		ParseSoapError("CreateKeyPair");
	}
	return false;
}

bool
AmazonVMDestroyKeypair::gsoapRequest(void)
{
	if( !SetupSoap() ) {
		vmprintf(D_ALWAYS, "Failed to setup SOAP context\n");
		return false;
	}

	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(),
				secretkeyfile.GetCStr(), error_msg) ) {
		vmprintf(D_ALWAYS, "AmazonVMDestroyKeypair Error: %s\n", 
				error_msg.Value());
		return false;
	}

	if( keyname.IsEmpty() ) {
		error_msg = "Empty_Keyname";
		vmprintf(D_ALWAYS, "AmazonVMDestroyKeypair Error: %s\n", 
				error_msg.Value());
		return false;
	}

	int code = -1;
	struct ec2__DeleteKeyPairType request;
	struct ec2__DeleteKeyPairResponseType response;

	request.keyName = (char *) keyname.GetCStr();

	if (!(code = soap_call___ec2__DeleteKeyPair(m_soap,
					AWS_URL,
					NULL,
					&request,
					&response))) {
		return true;
	}else {
		// Error
		ParseSoapError("DeleteKeyPair");
	}
	return false;
}

bool
AmazonVMRunningKeypair::gsoapRequest(void)
{
	return AmazonVMStatusAll::gsoapRequest();
}

bool
AmazonVMStart::gsoapRequest(void)
{
	if( !SetupSoap() ) {
		vmprintf(D_ALWAYS, "Failed to setup SOAP context\n");
		return false;
	}

	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(),
				secretkeyfile.GetCStr(), error_msg) ) {
		vmprintf(D_ALWAYS, "AmazonVMStart Error: %s\n", error_msg.Value());
		return false;
	}

	if( user_data_file.IsEmpty() == false ) {
		if( !check_read_access_file( user_data_file.GetCStr()) ) {
			error_msg.sprintf("Cannot read the file for user data(%s)",
					user_data_file.Value());
			return false;
		}
	}

	if( ami_id.IsEmpty() ) {
		error_msg = "Empty_AMI_ID";
		vmprintf(D_ALWAYS, "AmazonVMStart Error: %s\n", error_msg.Value());
		return false;
	}

	int code = -1;
	struct ec2__RunInstancesType request;
	struct ec2__ReservationInfoType response;

	// userData
	if( user_data_file.IsEmpty() == false ) {
		// Need to read file
		int fd = -1;
		fd = safe_open_wrapper(user_data_file.Value(), O_RDONLY);
		if( fd < 0 ) {
			error_msg.sprintf("failed to safe_open_wrapper file(%s) : "
					"safe_open_wrapper returns %s", user_data_file.Value(), 
					strerror(errno));
			vmprintf(D_ALWAYS, "%s\n", error_msg.Value());
			return false;
		}

		int file_size = 0;
		StatWrapper swrap(fd);
		file_size = swrap.GetStatBuf()->st_size;

		char *readbuffer = (char*)malloc(file_size);
		ASSERT(readbuffer);

		int ret = read(fd, readbuffer, file_size);
		close(fd);

		if( ret != file_size ) {
			error_msg.sprintf("failed to read(need %d but real read %d) "
					"in file(%s)", file_size, ret, user_data_file.Value());
			vmprintf(D_ALWAYS, "%s\n", error_msg.Value());

			free(readbuffer);
			readbuffer = NULL;
			return false;
		}
	
		base64_userdata = base64_encode((unsigned char*)readbuffer, file_size);
		free(readbuffer);
	}else {
		if( user_data.IsEmpty() == false ) { 
			base64_userdata = base64_encode((unsigned char*)user_data.GetCStr(), user_data.Length());
		}
	}

	// image id
	request.imageId = (char *) ami_id.Value();
	// min Count
	request.minCount = 1;
	// max Count
	request.maxCount = 1;

	// Keypair
	if( keypair.IsEmpty() == false ) {
		request.keyName = (char *) keypair.Value();
	}else {
		request.keyName = NULL;
	}

	// groupSet
	struct ec2__GroupSetType groupSet;
	struct ec2__GroupItemType *groupItems = NULL;

	if( groupnames.isEmpty() == false ) {
		int group_nums = groupnames.number();

		groupItems = (struct ec2__GroupItemType *)
			soap_malloc(m_soap,
						sizeof(struct ec2__GroupItemType)*group_nums);
		ASSERT(groupItems);

		int i=0;
		char *one_group = NULL;
		groupnames.rewind();
		while((one_group = groupnames.next()) != NULL ) {
			groupItems[i++].groupId = one_group;
		}

		groupSet.__sizeitem = group_nums;
		groupSet.item = &groupItems;

		request.groupSet = &groupSet;
	}else {
		request.groupSet = NULL;
	}

	// additionalInfo
	request.additionalInfo = NULL;

	// user data
	struct ec2__UserDataType userdata_type;
	if( base64_userdata ) {
		// TODO 
		// Need to check
		userdata_type.data = base64_userdata;
		userdata_type.version = "1.0";
		userdata_type.encoding = "base64";
		userdata_type.__mixed = NULL;

		request.userData = &userdata_type;
	}else {
		request.userData = NULL;
	}

	// addressingType
	request.addressingType = NULL;

	// instanceType
	if( instance_type.IsEmpty() == false ) {
		request.instanceType = (char *) instance_type.Value();
	}else {
		request.instanceType = NULL;
	}

	if (!(code = soap_call___ec2__RunInstances(m_soap,
					AWS_URL,
					NULL,
					&request,
					&response))) {

		if( groupItems ) {
			free(groupItems);
			groupItems = NULL;
		}

		if( response.instancesSet && response.instancesSet->item ) {
			instance_id = response.instancesSet->item[0]->instanceId;
			return true;
		}
	}else {
		// Error
		if( groupItems ) {
			free(groupItems);
			groupItems = NULL;
		}
		ParseSoapError("RunInstances");
	}
	return false;
}

bool
AmazonVMStop::gsoapRequest(void)
{
	if( !SetupSoap() ) {
		vmprintf(D_ALWAYS, "Failed to setup SOAP context\n");
		return false;
	}

	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(),
				secretkeyfile.GetCStr(), error_msg) ) {
		vmprintf(D_ALWAYS, "AmazonVMStop Error: %s\n", error_msg.Value());
		return false;
	}

	if( instance_id.IsEmpty() ) {
		error_msg = "Empty_Instance_ID";
		vmprintf(D_ALWAYS, "AmazonVMStop Error: %s\n", error_msg.Value());
		return false;
	}

	int code = -1;
	struct ec2__TerminateInstancesType request;
	struct ec2__TerminateInstancesResponseType response;

	struct ec2__TerminateInstancesInfoType instanceSet;
	struct ec2__TerminateInstancesItemType *item;

	item = (struct ec2__TerminateInstancesItemType *)
		soap_malloc(m_soap, sizeof(struct ec2__TerminateInstancesItemType));
	ASSERT(item);

	item->instanceId = (char *) instance_id.GetCStr();

	instanceSet.__sizeitem = 1;
	instanceSet.item = &item;

	request.instancesSet = &instanceSet;

	if (!(code = soap_call___ec2__TerminateInstances(m_soap,
					AWS_URL,
					NULL,
					&request,
					&response))) {
		return true;
	}else {
		// Error
		ParseSoapError("TerminateInstances");

	}
	return false;
}

bool
AmazonVMStatus::gsoapRequest(void)
{
	if( !SetupSoap() ) {
		vmprintf(D_ALWAYS, "Failed to setup SOAP context\n");
		return false;
	}

	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(),
				secretkeyfile.GetCStr(), error_msg) ) {
		vmprintf(D_ALWAYS, "AmazonVMStatus Error: %s\n", error_msg.Value());
		return false;
	}

	if( instance_id.IsEmpty() ) {
		error_msg = "Empty_Instance_ID";
		vmprintf(D_ALWAYS, "AmazonVMStatus Error: %s\n", error_msg.Value());
		return false;
	}

	int code = -1;
	int i = 0;
	struct ec2__DescribeInstancesType request;
	struct ec2__DescribeInstancesResponseType response;

	struct ec2__DescribeInstancesInfoType instancesSet;
	struct ec2__DescribeInstancesItemType *item;

	item = (struct ec2__DescribeInstancesItemType *)
		soap_malloc(m_soap, sizeof(struct ec2__DescribeInstancesItemType));
	ASSERT(item);

	item->instanceId = (char *) instance_id.GetCStr();

	instancesSet.__sizeitem = 1;
	instancesSet.item = &item;

	request.instancesSet = &instancesSet;

	if (!(code = soap_call___ec2__DescribeInstances(m_soap,
					AWS_URL,
					NULL,
					&request,
					&response))) {

		if( response.reservationSet && response.reservationSet->item ) {
			int total_nums = response.reservationSet->__sizeitem;

			for (i = 0; i < total_nums; i++) {
				struct ec2__RunningInstancesSetType* _instancesSet = 
					response.reservationSet->item[i]->instancesSet;

				if( !_instancesSet ) {
					continue;
				}

				struct ec2__RunningInstancesItemType **instance = 
					_instancesSet->item;

				if( !instance || !(*instance)->instanceId ) {
					continue;
				}

				if( !(*instance)->instanceState || 
						!(*instance)->instanceState->name) {
					vmprintf(D_ALWAYS, "Failed to get valid status\n");
					continue;
				}

				if( !strcmp((*instance)->instanceId, instance_id.Value()) ) {

					status_result.status = (*instance)->instanceState->name;
					status_result.instance_id = (*instance)->instanceId;
					status_result.ami_id = (*instance)->imageId;
					status_result.public_dns = (*instance)->dnsName;
					status_result.private_dns = (*instance)->privateDnsName;
					status_result.keyname = (*instance)->keyName;
					status_result.instancetype = (*instance)->instanceType;

					// Set group names
					struct ec2__GroupSetType* groupSet =
						response.reservationSet->item[i]->groupSet;

					if( groupSet && groupSet->item ) {
						int j = 0;
						for( j = 0; j < groupSet->__sizeitem; j++ ) {
							status_result.groupnames.append(groupSet->item[j]->groupId);
						}
					}

					break;
				}
			}
		}
		return true;
	}else {
		// Error
		ParseSoapError("DescribeInstance");

	}
	return false;
}

bool
AmazonVMStatusAll::gsoapRequest(void)
{
	if( !SetupSoap() ) {
		vmprintf(D_ALWAYS, "Failed to setup SOAP context\n");
		return false;
	}

	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(),
				secretkeyfile.GetCStr(), error_msg) ) {
		vmprintf(D_ALWAYS, "AmazonVMStatusAll Error: %s\n", error_msg.Value());
		return false;
	}

	int code = -1;
	int i = 0;
	struct ec2__DescribeInstancesType request;
	struct ec2__DescribeInstancesResponseType response;

	request.instancesSet = NULL;

	if (!(code = soap_call___ec2__DescribeInstances(m_soap,
					AWS_URL,
					NULL,
					&request,
					&response))) {

		if( response.reservationSet && response.reservationSet->item ) {
			int total_nums = response.reservationSet->__sizeitem;

			status_results = new AmazonStatusResult[total_nums];
			ASSERT(status_results);

			for (i = 0; i < total_nums; i++) {
				struct ec2__RunningInstancesSetType* instancesSet = 
					response.reservationSet->item[i]->instancesSet;

				if( !instancesSet ) {
					continue;
				}

				struct ec2__RunningInstancesItemType **instance = 
					instancesSet->item;

				if( !instance || !(*instance)->instanceId ) {
					continue;
				}

				if( !(*instance)->instanceState || 
						!(*instance)->instanceState->name) {
					vmprintf(D_ALWAYS, "Failed to get valid status\n");
					continue;
				}

				status_results[status_num].status = 
					(*instance)->instanceState->name;

				status_results[status_num].instance_id =
				   	(*instance)->instanceId;
				status_results[status_num].ami_id = 
					(*instance)->imageId;
				status_results[status_num].public_dns =
					(*instance)->dnsName;
				status_results[status_num].private_dns =
					(*instance)->privateDnsName;
				status_results[status_num].keyname =
					(*instance)->keyName;
				status_results[status_num].instancetype =
					(*instance)->instanceType;

				// Set group names
				struct ec2__GroupSetType* groupSet =
					response.reservationSet->item[i]->groupSet;

				if( groupSet && groupSet->item ) {
					int j = 0;
					for( j = 0; j < groupSet->__sizeitem; j++ ) {
						status_results[status_num].groupnames.append(
								groupSet->item[j]->groupId);
					}
				}

				status_num++;
			}
		}
		return true;
	}else {
		// Error
		ParseSoapError("DescribeInstances");

	}
	return false;
}


