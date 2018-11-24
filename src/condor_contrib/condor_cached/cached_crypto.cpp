#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "cached_server.h"
#include "compat_classad.h"
#include "file_transfer.h"
#include "condor_version.h"
#include "classad_log.h"
#include "get_daemon_name.h"
#include "ipv6_hostname.h"
#include "basename.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <assert.h>
#include "condor_debug.h"
#include "cached_crypto.h"
#include "cryptopp/aes.h"
#include "cryptopp/filters.h"
#include "cryptopp/modes.h"
#include "cryptopp/files.h"

#define byte uint8_t
byte aes_key[CryptoPP::AES::DEFAULT_KEYLENGTH ] = "123456789";

Cryptographer::Cryptographer(){}
Cryptographer::~Cryptographer(){}

std::string Cryptographer::EncryptFile(const std::string file, const std::string algorithm, const int buffersize) {
	dprintf(D_FULLDEBUG, "In EncryptFile, entering it\n");
	std::string encrypted_file = file + ".encrypted";
	std::ifstream is(file.c_str(), std::ifstream::binary);
	if (!is) {
		dprintf(D_FULLDEBUG, "In EncryptFile, cannot open file %s\n", file.c_str());
		return encrypted_file;
	}
	is.seekg (0, is.end);
	int length = is.tellg();
	if(length < 0) {
		dprintf(D_FULLDEBUG, "In EncryptFile, file length is not correct\n");
		return encrypted_file;
	}
	is.seekg (0, is.beg);
	int sz = (length < buffersize) ? length : buffersize;
	char * buffer = new char [sz];
	int n = (length-1)/sz + 1;
	
	std::ofstream os(encrypted_file.c_str(), std::ofstream::binary);

	int flag=length%16;
	if (flag)
		os.write("1",1);
	else
		os.write("0",1);

	for(int i = 0; i < n; ++i) {
		int readin_size = (length < sz) ? length : sz;
		if(algorithm == "AES") {
			std::string cipher;
			is.read(buffer, readin_size);
			CryptoPP::AES::Encryption aes(aes_key, CryptoPP::AES::DEFAULT_KEYLENGTH);
			CryptoPP::ECB_Mode_ExternalCipher::Encryption ecb(aes);
			if(flag&&(i==n-1))
			{
				CryptoPP::StreamTransformationFilter stf(ecb, new CryptoPP::StringSink(cipher));
				stf.Put(reinterpret_cast<const unsigned char*>(buffer), readin_size);
				stf.MessageEnd();
			}
				
			else	
			{
				CryptoPP::StreamTransformationFilter stf(ecb, new CryptoPP::StringSink(cipher), CryptoPP::StreamTransformationFilter::NO_PADDING);
				stf.Put(reinterpret_cast<const unsigned char*>(buffer), readin_size);
				stf.MessageEnd();
			}
			os.write(cipher.c_str(), cipher.size());
		}
		length -= sz;
		if(length < sz) {
			dprintf(D_FULLDEBUG, "In EncryptFile, length < sz so i should be n-1 (i=%d,n-1=%d)\n", i, n-1);
		}
	}
	delete [] buffer;
	is.close();
	os.close();
	return encrypted_file;
}

std::string Cryptographer::DecryptFile(const std::string file, const std::string algorithm, const int buffersize) {
	dprintf(D_FULLDEBUG, "In DecryptFile, entering it\n");
	std::string decrypted_file = file + ".decrypted";
	std::ifstream is(file.c_str(), std::ifstream::binary);
	if (!is) {
		dprintf(D_FULLDEBUG, "In DecryptFile, cannot open file %s\n", file.c_str());
		return decrypted_file;
	}
	is.seekg (0, is.end);
	int length = is.tellg();
	if(length < 0) {
		dprintf(D_FULLDEBUG, "In DecryptFile, file length is not correct\n");
		return decrypted_file;
	}
	length--;
	is.seekg (0, is.beg);
	int sz = (length < buffersize) ? length : buffersize;
	char * buffer = new char [sz];
	int n = (length-1)/sz + 1;
	is.read(buffer, 1);
	int flag=(buffer[0]=='1');
	
	std::ofstream os(decrypted_file.c_str(), std::ofstream::binary);
	for(int i = 0; i < n; ++i) {
		int readin_size = (length < sz) ? length : sz;
		if(algorithm == "AES") {
			std::string decipher;
			is.read(buffer, readin_size);
			CryptoPP::AES::Decryption aes(aes_key, CryptoPP::AES::DEFAULT_KEYLENGTH);
			CryptoPP::ECB_Mode_ExternalCipher::Decryption ecb(aes);
			if(flag&&(i==n-1))
			{
				CryptoPP::StreamTransformationFilter stf(ecb, new CryptoPP::StringSink(decipher));
				stf.Put(reinterpret_cast<const unsigned char*>(buffer), readin_size);
				stf.MessageEnd();
			}
			else
			{
				CryptoPP::StreamTransformationFilter stf(ecb, new CryptoPP::StringSink(decipher),CryptoPP::StreamTransformationFilter::NO_PADDING);
				stf.Put(reinterpret_cast<const unsigned char*>(buffer), readin_size);
				stf.MessageEnd();
			}
			os.write(decipher.c_str(), decipher.size());
		}
		length -= sz;
		if(length < sz) {
			dprintf(D_FULLDEBUG, "In DecryptFile, length < sz so i should be n-1 (i=%d,n-1=%d)\n", i, n-1);
		}
	}
	delete [] buffer;
	is.close();
	os.close();
	return decrypted_file;
}
