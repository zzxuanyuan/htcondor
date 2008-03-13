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
#include "condor_crypt.h"
#include "condor_md.h"
#include "condor_random_num.h"
#ifdef HAVE_EXT_OPENSSL
#include <openssl/rand.h>              // SSLeay rand function
#endif
#include "condor_debug.h"

Condor_Crypt :: Condor_Crypt(const KeyInfo& keyInfo)
	: keyInfo_ (keyInfo)
{
	dprintf(D_SECURITY, "Instantiating Condor_Crypt object.\n");

	cryptKeyLen_ = 0;
	cryptCipher_ = NULL;
	unsigned char *keyData;

	resetState();
	Protocol p = keyInfo_.getProtocol();
	switch(p) {
	case CONDOR_BLOWFISH_PRE_EVP:
		dprintf(D_SECURITY, "Encryption method: blowfish.\n");
		//cryptCipher_ = EVP_bf_cfb();
		// Generate the key
		BF_set_key(&key_, keyInfo_.getKeyLength(), keyInfo_.getKeyData());
		return;
		break;
	case CONDOR_3DES_PRE_EVP:
		dprintf(D_SECURITY, "Encryption method: 3des.\n");
		//cryptCipher_ = EVP_des_ede3_cfb();
		keyData = keyInfo_.getPaddedKeyData(24);
		ASSERT(keyData);
		des_set_key((des_cblock *)  keyData    , keySchedule1_);
		des_set_key((des_cblock *) (keyData+8) , keySchedule2_);
		des_set_key((des_cblock *) (keyData+16), keySchedule3_);
		free(keyData);
		return;
		break;
	case CONDOR_BLOWFISH_EVP:
		dprintf(D_SECURITY, "Encryption method: blowfish.\n");
		cryptCipher_ = EVP_bf_cfb();
		break;
	case CONDOR_3DES_EVP:
		dprintf(D_SECURITY, "Encryption method: 3des.\n");
		cryptCipher_ = EVP_des_ede3_cfb();
		break;
	case CONDOR_AES256:
		dprintf(D_SECURITY, "Encryption method: aes256.\n");
		cryptCipher_ = EVP_aes_256_cfb();
		break;
	case CONDOR_AES192:
		dprintf(D_SECURITY, "Encryption method: aes192.\n");
		cryptCipher_ = EVP_aes_192_cfb();
		break;
	case CONDOR_AES128:
		dprintf(D_SECURITY, "Encryption method: aes128.\n");
		cryptCipher_ = EVP_aes_128_cfb();
		break;
	default:
		dprintf(D_SECURITY, "Encryption method: none.\n");
		break;
	}
	cryptKeyLen_ = EVP_CIPHER_key_length(cryptCipher_);
	//dprintf(D_SECURITY, "cryptKeyLen_ = %d\n", cryptKeyLen_);
	encryptKeyData_ = keyInfo_.getPaddedKeyData(cryptKeyLen_);
	decryptKeyData_ = keyInfo_.getPaddedKeyData(cryptKeyLen_);
	EVP_CIPHER_CTX_init(&encryptCtx_);
	EVP_CIPHER_CTX_init(&decryptCtx_);
	//dprintf(D_SECURITY, "IV_LEN: %d\n", EVP_MAX_IV_LENGTH);
	EVP_EncryptInit_ex(&encryptCtx_, cryptCipher_, NULL, encryptKeyData_, encryptIVec_);
	EVP_DecryptInit_ex(&decryptCtx_, cryptCipher_, NULL, decryptKeyData_, decryptIVec_);
	//dprintf(D_SECURITY, "end constructor.\n");
}

void Condor_Crypt :: resetState()
{
	Protocol p = keyInfo_.getProtocol();
	switch(p) {
	case CONDOR_BLOWFISH_PRE_EVP:
	case CONDOR_3DES_PRE_EVP:
		memset(ivec_, 0, 8);
		num_ = 0;
		break;
	default:
		memset(encryptIVec_, 0, EVP_MAX_IV_LENGTH);
		memset(decryptIVec_, 0, EVP_MAX_IV_LENGTH);
		break;
	}
}

Condor_Crypt :: Condor_Crypt()
{
}

Condor_Crypt :: ~Condor_Crypt()
{
	dprintf(D_SECURITY, "Encryption cleanup.\n");
	Protocol p = keyInfo_.getProtocol();
	switch(p) {
	case CONDOR_BLOWFISH_PRE_EVP:
	case CONDOR_3DES_PRE_EVP:
		break;
	default:
		EVP_CIPHER_CTX_cleanup(&encryptCtx_);
		EVP_CIPHER_CTX_cleanup(&decryptCtx_);
		if(encryptKeyData_)
			free(encryptKeyData_);
		if(decryptKeyData_)
			free(decryptKeyData_);
		break;
	}
}

bool Condor_Crypt :: operateCipher(unsigned char *  input, 
                                   int              input_len, 
                                   unsigned char *& output, 
                                   int&             output_len,
								   int              encrypt_or_decrypt)
{
#if !defined(SKIP_AUTHENTICATION)
	output_len = 0;
    output = (unsigned char *) malloc(input_len+EVP_MAX_BLOCK_LENGTH);
	if(!output) {
		return false;
	}
	Protocol p = keyInfo_.getProtocol();
	switch(p) {
	case CONDOR_BLOWFISH_PRE_EVP:
		output_len = input_len;
        BF_cfb64_encrypt(input, output, output_len, 
						 &key_, ivec_, &num_, encrypt_or_decrypt);
		return true;
		break;
	case CONDOR_3DES_PRE_EVP:
		output_len = input_len;
		des_ede3_cfb64_encrypt(input, output, output_len,
							   keySchedule1_, keySchedule2_, keySchedule3_,
							   (des_cblock *)ivec_, &num_, encrypt_or_decrypt);
		return true;
		break;
	default:
		
		int rv = 0;
		if(encrypt_or_decrypt) {
			rv = EVP_EncryptUpdate(&encryptCtx_, output, &output_len, 
								   input, input_len);
		} else {
			rv = EVP_DecryptUpdate(&decryptCtx_, output, &output_len, 
								   input, input_len);
		}
		if(rv) {
			return true;
		}
		else {
			free(output);
			output = NULL;
			output_len = 0;
			return false;
		}
		break;
	}
#else
	return true;
#endif
}

bool Condor_Crypt :: encrypt(unsigned char *  input, 
                             int              input_len, 
                             unsigned char *& output, 
                             int&             output_len)
{
	return operateCipher(input, input_len, output, output_len, 1);
}

bool Condor_Crypt :: decrypt(unsigned char *  input, 
                             int              input_len, 
                             unsigned char *& output, 
                             int&             output_len)
{
	return operateCipher(input, input_len, output, output_len, 0);
}

int Condor_Crypt :: encryptedSize(int inputLength, int blockSize)
{
#ifdef HAVE_EXT_OPENSSL
    int size = inputLength % blockSize;
	dprintf(D_SECURITY, "encryptedSize\n");
    return (inputLength + ((size == 0) ? blockSize : (blockSize - size)));
#else
    return -1;
#endif
}

Protocol Condor_Crypt :: protocol()
{
#ifdef HAVE_EXT_OPENSSL
    return keyInfo_.getProtocol();
#else
    return (Protocol)0;
#endif
}

unsigned char * Condor_Crypt :: randomKey(int length)
{
    unsigned char * key = (unsigned char *)(malloc(length));

	memset(key, 0, length);

#ifdef HAVE_EXT_OPENSSL
	static bool already_seeded = false;
    int size = 128;
    if( ! already_seeded ) {
        unsigned char * buf = (unsigned char *) malloc(size);
		for (int i = 0; i < size; i++) {
			buf[i] = get_random_int() & 0xFF;
		}

        RAND_seed(buf, size);
        free(buf);
		already_seeded = true;
    }

    RAND_bytes(key, length);
#else
    // use condor_util_lib/get_random.c
    int r, s, size = sizeof(r);
    unsigned char * tmp = key;
    for (s = 0; s < length; s+=size, tmp+=size) {
        r = get_random_int();
        memcpy(tmp, &r, size);
    }
    if (length > s) {
        r = get_random_int();
        memcpy(tmp, &r, length - s);
    }
#endif
    return key;
}

unsigned char * Condor_Crypt :: oneWayHashKey(const char * initialKey)
{
#ifdef HAVE_EXT_OPENSSL
    return Condor_MD_MAC::computeOnce((unsigned char *)initialKey, strlen(initialKey));
#else 
    return 0;
#endif
}
