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
#include <chrono>

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
	
	std::chrono::time_point<std::chrono::system_clock> cpu_start;
	std::chrono::time_point<std::chrono::system_clock> cpu_end;
	std::chrono::time_point<std::chrono::system_clock> io_start;
	std::chrono::time_point<std::chrono::system_clock> io_end;
	std::chrono::duration<double> cpu_duration(0);
	std::chrono::duration<double> io_duration(0);

	std::ofstream os(encrypted_file.c_str(), std::ofstream::binary);
	for(int i = 0; i < n; ++i) {
		int readin_size = (length < sz) ? length : sz;
		if(algorithm == "AES") {
			std::string cipher;
			io_start = std::chrono::system_clock::now();
			is.read(buffer, readin_size);
			io_end = std::chrono::system_clock::now();
			io_duration += (io_end - io_start);
			cpu_start = std::chrono::system_clock::now();
			CryptoPP::AES::Encryption aes(aes_key, CryptoPP::AES::DEFAULT_KEYLENGTH);
			CryptoPP::ECB_Mode_ExternalCipher::Encryption ecb(aes);
			if(length>=buffersize)
			{
				CryptoPP::StreamTransformationFilter stf(ecb, new CryptoPP::StringSink(cipher), CryptoPP::StreamTransformationFilter::NO_PADDING);
				stf.Put(reinterpret_cast<const unsigned char*>(buffer), readin_size);
				stf.MessageEnd();
			}
				
			else	
			{
				CryptoPP::StreamTransformationFilter stf(ecb, new CryptoPP::StringSink(cipher));
				stf.Put(reinterpret_cast<const unsigned char*>(buffer), readin_size);
				stf.MessageEnd();
			}
			cpu_end = std::chrono::system_clock::now();
			cpu_duration += (cpu_end - cpu_start);
			io_start = std::chrono::system_clock::now();
			os.write(cipher.c_str(), cipher.size());
			io_end = std::chrono::system_clock::now();
			io_duration += (io_end - io_start);
		}
		length -= sz;
		if(length < sz) {
			dprintf(D_FULLDEBUG, "In EncryptFile, length < sz so i should be n-1 (i=%d,n-1=%d)\n", i, n-1);
		}
	}
	std::fstream outfile;//##
	outfile.open("/home/centos/encrypt_cpu_io.txt", std::fstream::out);//##
	outfile << "In EncryptFile, cpu_duration = " << cpu_duration.count() << ", io_duration = " << io_duration.count() << std::endl;//##
	outfile.close();//##
	dprintf(D_FULLDEBUG, "In EncryptFile, cpu_duration = %f, io_duration = %f\n", cpu_duration.count(), io_duration.count());
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
	is.seekg (0, is.beg);
	int sz = (length < buffersize) ? length : buffersize;
	char * buffer = new char [sz];
	int n = (length-1)/sz + 1;
	
	std::chrono::time_point<std::chrono::system_clock> cpu_start;
	std::chrono::time_point<std::chrono::system_clock> cpu_end;
	std::chrono::time_point<std::chrono::system_clock> io_start;
	std::chrono::time_point<std::chrono::system_clock> io_end;
	std::chrono::duration<double> cpu_duration(0);
	std::chrono::duration<double> io_duration(0);

	std::ofstream os(decrypted_file.c_str(), std::ofstream::binary);
	for(int i = 0; i < n; ++i) {
		int readin_size = (length < sz) ? length : sz;
		if(algorithm == "AES") {
			std::string decipher;
			io_start = std::chrono::system_clock::now();
			is.read(buffer, readin_size);
			io_end = std::chrono::system_clock::now();
			io_duration += (io_end - io_start);
			cpu_start = std::chrono::system_clock::now();
			CryptoPP::AES::Decryption aes(aes_key, CryptoPP::AES::DEFAULT_KEYLENGTH);
			CryptoPP::ECB_Mode_ExternalCipher::Decryption ecb(aes);
			if(length>=buffersize)
			{
				CryptoPP::StreamTransformationFilter stf(ecb, new CryptoPP::StringSink(decipher),CryptoPP::StreamTransformationFilter::NO_PADDING);
				stf.Put(reinterpret_cast<const unsigned char*>(buffer), readin_size);
				stf.MessageEnd();
			}
			else
			{
				CryptoPP::StreamTransformationFilter stf(ecb, new CryptoPP::StringSink(decipher));
				stf.Put(reinterpret_cast<const unsigned char*>(buffer), readin_size);
				stf.MessageEnd();
			}
			cpu_end = std::chrono::system_clock::now();
			cpu_duration += (cpu_end - cpu_start);
			io_start = std::chrono::system_clock::now();
			os.write(decipher.c_str(), decipher.size());
			io_end = std::chrono::system_clock::now();
			io_duration += (io_end - io_start);
		}
		length -= sz;
		if(length < sz) {
			dprintf(D_FULLDEBUG, "In DecryptFile, length < sz so i should be n-1 (i=%d,n-1=%d)\n", i, n-1);
		}
	}
	std::fstream outfile;//##
	outfile.open("/home/centos/decrypt_cpu_io.txt", std::fstream::out);//##
	outfile << "In DecryptFile, cpu_duration = " << cpu_duration.count() << ", io_duration = " << io_duration.count() << std::endl;//##
	outfile.close();//##
	dprintf(D_FULLDEBUG, "In DecryptFile, cpu_duration = %f, io_duration = %f\n", cpu_duration.count(), io_duration.count());
	delete [] buffer;
	is.close();
	os.close();
	return decrypted_file;
}
