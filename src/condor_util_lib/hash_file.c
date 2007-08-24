/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <openssl/sha.h>

/*
 * This is a horrible horrible hack, but i implemented this method
 * before I realized that the openssl that condor links with (through
 * globus) doesn't contain support for sha256.  Eventually it will,
 * and we'll get a checksum that's not broken!  Until then, we'll just
 * sha1.  This should be fixed when this code gets merged.
 */
#ifdef SHA256_DIGEST_LENGTH
#define HASH_CTX SHA256_CTX
#define HASH_INIT SHA256_Init
#define HASH_UPDATE SHA256_Update
#define HASH_FINAL SHA256_Final
#define HASH_DIGEST_LENGTH SHA256_DIGEST_LENGTH
#else
#define HASH_CTX SHA_CTX
#define HASH_INIT SHA1_Init
#define HASH_UPDATE SHA1_Update
#define HASH_FINAL SHA1_Final
#define HASH_DIGEST_LENGTH SHA_DIGEST_LENGTH
#endif

char *
condor_hash_file(char *fn)
{
	HASH_CTX ctx;
	char *rv = NULL;
	static const char hex[] = "0123456789abcdef";
	int fd;
	int offset = 0, length;
	struct stat f_stat;
	int n, i;
	unsigned char hash[HASH_DIGEST_LENGTH];
	unsigned char buf[BUFSIZ];

	rv = (char *)malloc(HASH_DIGEST_LENGTH*2+1);
	if(!rv) {
		perror("malloc");
		fprintf(stderr, "malloc error.\n");
		return NULL;
	}

	HASH_INIT(&ctx);
	fd = open(fn, O_RDONLY);
	if(fd < 0) {
		fprintf(stderr, "Couldn't open file '%s'\n", fn);
		free(rv);
		return NULL;
	}
	if(fstat(fd, &f_stat) < 0) {
		fprintf(stderr, "Stat error on file '%s'\n", fn);
		free(rv);
		close(fd);
		return NULL;
	}
	length = f_stat.st_size;
	n = length;
	i = 0;
	while(n > 0) {
		if(n > sizeof(buf)) {
			i = read(fd, buf, sizeof(buf));
		} else {
			i = read(fd, buf, n);
		}
		if(i < 0) {
			break;
		}
		HASH_UPDATE(&ctx, buf, i);
		n -= i;
	}
	HASH_FINAL(hash, &ctx);
	for(i = 0; i < HASH_DIGEST_LENGTH; i++) {
		rv[i*2] = hex[hash[i] >> 4];
		rv[i*2+1] = hex[hash[i] & 0x0f];
	}
	rv[i*2] = '\0';
	return rv;
}

