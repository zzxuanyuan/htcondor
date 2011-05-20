/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "condor_base64.h"

// For base64 encoding
#if HAVE_EXT_OPENSSL
# include <openssl/sha.h>
# include <openssl/hmac.h>
# include <openssl/evp.h>
# include <openssl/bio.h>
# include <openssl/buffer.h>

// Caller needs to free the returned pointer
char* condor_base64_encode(const unsigned char *input, int length)
{
	BIO *bmem, *b64;
	BUF_MEM *bptr;

	b64 = BIO_new(BIO_f_base64());
	bmem = BIO_new(BIO_s_mem());
	b64 = BIO_push(b64, bmem);
	BIO_write(b64, input, length);
	(void)BIO_flush(b64);
	BIO_get_mem_ptr(b64, &bptr);

	char *buff = (char *)malloc(bptr->length);
	ASSERT(buff);
	memcpy(buff, bptr->data, bptr->length-1);
	buff[bptr->length-1] = 0;
	BIO_free_all(b64);

	return buff;
}

// Caller needs to free *output if non-NULL
void condor_base64_decode(const char *input,unsigned char **output, int *output_length)
{
	BIO *b64, *bmem;

	ASSERT( input );
	ASSERT( output );
	ASSERT( output_length );

	int input_length = strlen(input);

		// assuming output length is <= input_length
	*output = (unsigned char *)malloc(input_length + 1);
	ASSERT( *output );
	memset(*output, 0, input_length);

	b64 = BIO_new(BIO_f_base64());
	bmem = BIO_new_mem_buf((void *)input, input_length);
	bmem = BIO_push(b64, bmem);

	*output_length = BIO_read(bmem, *output, input_length);
	if( *output_length < 0 ) {
		free( *output );
		*output = NULL;
	}

	BIO_free_all(bmem);
}

#else // if !HAVE_EXT_OPENSSL

namespace {
static const char* b64_table = "ABCDEFGHIJKLMNOP"
			       "QRSTUVWXYZabcdef"
			       "ghijklmnopqrstuv"
			       "wxyz0123456789+/";

static int b64_decode_table[256];

static void initialize_decode_table();

int decode_table_initialized = 0;

static void initialize_decode_table()
{
	int ii;
	for(ii=0;ii<256;++ii)
		b64_decode_table[ii]=-1;
	for(ii=0;b64_table[ii];++ii)
		b64_decode_table[static_cast<int>(b64_table[ii])] = ii;
}
}

char* condor_base64_encode(const unsigned char* input, int length)
{
	int blen;
	blen = (length * 4+3)/3; // Space for the buffer
	blen += (blen + 76)/76; // Space for the newlines
	char* buff = (char*)malloc(blen*sizeof(char));
	ASSERT(buff);
	int jj,ii;
	int kk=0,ll=0;
	int result;
	for(jj=0,ii=3;ii<=length;ii+=3,jj+=3){
		result = (static_cast<int>(input[jj])<<16) |
			 (static_cast<int>(input[jj+1])<<8) |
			 (static_cast<int>(input[jj+2]));
		buff[ll++] = b64_table[(result>>18)&0x3f];	
		buff[ll++] = b64_table[(result>>12)&0x3f];	
		buff[ll++] = b64_table[(result>>6)&0x3f];	
		buff[ll++] = b64_table[result&0x3f];	
		kk += 4;
		// RFC 2045 says no lines can be more than 76 characters
		if(kk == 76){
			buff[ll++]='\n';
			kk=0;
		}
	}
	switch(length-jj){
		case 0: break;
		case 1: result = static_cast<int>(input[jj])<<16;
			buff[ll++] = b64_table[(result>>18)&0x3f];	
			buff[ll++] = b64_table[(result>>12)&0x3f];	
			buff[ll++] = '=';
			buff[ll++] = '=';
			kk+=4;
			break;
		case 2: result = (static_cast<int>(input[jj])<<16) |
			 (static_cast<int>(input[jj+1])<<8);
			buff[ll++] = b64_table[(result>>18)&0x3f];	
			buff[ll++] = b64_table[(result>>12)&0x3f];	
			buff[ll++] = b64_table[(result>>6)&0x3f];	
			buff[ll++] = '=';
			kk+=4;
			break;
		default: break;
	}
	buff[ll++]='\n';
	buff[ll]='\0';
	return buff;	
}

// Caller needs to free *output if non-NULL

void condor_base64_decode(const char *input,unsigned char **output, int *output_length)
{
	if(!decode_table_initialized)
		initialize_decode_table();
	ASSERT(input && output_length && output);
	if(output_length)
		*output_length = strlen(input)*3/4;	
	*output = (unsigned char*)malloc(sizeof(char) * *output_length);
	ASSERT(*output);
	int tolen=0;
	unsigned char* s = *output;
	int kk=0,result=0,seen_equal=0;
	while(*input){
		if(b64_decode_table[static_cast<int>(*input) & 0xff] >= 0){
			result <<= 6;
			result |= b64_decode_table[static_cast<int>(*input) & 0xff];
			++kk;
		} else if(*input == '='){
			result <<= 6;
			++kk;
			if(seen_equal < 2) ++seen_equal;
		}
		if(kk == 4){
			s[tolen++] = static_cast<unsigned char>((result >> 16) & 0xff);	
			s[tolen++] = static_cast<unsigned char>((result >> 8) & 0xff);	
			s[tolen++] = static_cast<unsigned char>(result & 0xff);	
			result = 0;
			kk = 0;
			if(seen_equal)
				break;
		}
		++input;
	}
	tolen -= seen_equal;	
	if(kk != 0) { // Input was not correct
		free(*output);
		*output = NULL;
		*output_length = 0;
	} else 
		*output_length = tolen;	
}
#endif // HAVE_EXT_OPENSSL
