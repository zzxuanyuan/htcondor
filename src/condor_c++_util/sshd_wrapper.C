/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

#include "sshd_wrapper.h"

#include "condor_common.h"
#include "condor_config.h" // for param
#include <malloc.h>

SshdWrapper::SshdWrapper()
{
	pubKeyFile = NULL;
	port = 4000; // param for this?
	sinful = NULL;
	dir = NULL;
	username = NULL;
}

SshdWrapper::~SshdWrapper()
{
	free(pubKeyFile);
	free(privKeyFile);
}

bool
SshdWrapper::initialize(char *filename)
{
	pubKeyFile = filename;

		// ASSERT filename ends in .pub
	privKeyFile = strdup(filename);
	privKeyFile[strlen(privKeyFile) - 4] = '\0';

	hostKeyFile = strcat(privKeyFile, "-host");
	return true;
}

bool 
SshdWrapper::createIdentityKeys()
{
	char *keygen = param("SSH_KEYGEN");

	if( keygen == NULL)	{
		dprintf(D_ALWAYS, "no SSH_KEYGEN in config file, can't create keys\n");
		return false;
	}

	char *args = param("SSH_KEYGEN_ARGS");
	if( args == NULL)	{
		dprintf(D_ALWAYS, "no SSH_KEYGEN_ARGS in config file, can't create keys\n");
		free(keygen);
		return false;
	}

		// TODO Fix!
	char command[256];

		// Assume args needs privKeyFile as trailing argument
	sprintf(command, "%s %s %s > /dev/null 2>&1 < /dev/null", keygen, args, privKeyFile);

	dprintf(D_ALWAYS, "Generating keys with %s\n", command);

		// And run the command...
	int ret = system(command);
	
    free( keygen);
    free( args );

	if (ret == 0) {
		return true;
	} else {
		dprintf(D_ALWAYS, "keygen failed with return code %d\n", ret);
		return false;
	}
} 

char *
SshdWrapper::getPubKeyFromFile()
{
	int ret;
	struct stat keyfile;

		// Get the size of the keyfile
	if (stat(pubKeyFile, &keyfile) != 0) {
		dprintf(D_ALWAYS, "Can't stat filename %s:%d\n", pubKeyFile, errno);
		return 0;
	}

	int length = keyfile.st_size;

	char *buf = (char *) malloc(length + 2);

		// Now read the file in in one swoop
	FILE *f = fopen(pubKeyFile, "r");

	if (f == NULL) {
		dprintf(D_ALWAYS, "can't open %s errno is %d\n", pubKeyFile, errno);
		free(buf);
		return 0;
	}

	ret = fread(buf, 1, length, f);
	if (ret < length) {
		dprintf(D_ALWAYS, "can't read from %s\n", pubKeyFile);
		free(buf);
		fclose(f);
		return 0;
	}

	if (f == NULL) {
		dprintf(D_ALWAYS, "Can't open idfilename %s\n", pubKeyFile);
		free(buf);
		fclose(f);
		return 0;
	}

	fclose(f);
		// null terminate
	buf[length + 1] = '0';

	return buf;
}

bool
SshdWrapper::setPubKeyToFile(const char *keystring)
{
	FILE *f = fopen(pubKeyFile, "w");
	if (f == NULL) {
		dprintf(D_ALWAYS, "can't open %s errno is %d\n", pubKeyFile, errno);
		return false;
	}

	fwrite(keystring, strlen(keystring), 1, f);
	fclose(f);
	return true;
}

bool
SshdWrapper::getSshdExecInfo(char* & executable, char* & args, char* & env )
{
	executable = param("SSHD");
	if (executable == NULL) {
		dprintf(D_ALWAYS, "Can't find SSHD in config file\n");
		args = NULL;
		env = NULL;
		return false;
	}

	char *buf = (char *) malloc(256); // TODO

	char *rawArgs = param("SSHD_ARGS");
	if (rawArgs == NULL) {
		dprintf(D_ALWAYS, "Can't find SSHD_ARGS in config file\n");
		free(executable);
		executable = NULL;
		args = NULL;
		env = NULL;
		return false;
	}


	sprintf(buf, "-p%d -oAuthorizedKeysFile=%s -h%s %s", port, pubKeyFile, privKeyFile, rawArgs);
	args = buf;
	
	free(rawArgs);
	env = NULL;

	launchTime = (int) time(NULL);

	return true;
}

bool 
SshdWrapper::onSshdExitTryAgain(int exit_status)
{
	int currentTime = (int) time(NULL);

		// If sshd has been running for more than two seconds
		// assume it hasn't failed due to a port collision,
		// so we shouldn't retry
	if( (currentTime - launchTime) > 2) {
		return false;
	}

	port++;
	return true;
}

bool 
SshdWrapper::getSshRuntimeInfo(char* & sinful_string, char* & dir, char* & 
					   username)
{
	sinful_string = strdup(this->sinful);
	dir = strdup(this->dir);
	username = strdup(this->username);
	return true;
}

bool 
SshdWrapper::setSshRuntimeInfo(const char* sinful_string, const char* dir, 
							   const char* username)
{
	this->sinful = sinful_string;
	this->dir = dir;
	this->username = username;
	return true;
}

bool 
SshdWrapper::sendPrivateKeyAndContactFile(const char* contactFileSrc)
{
	char buf[256];
	
	char *scp = param("SCP");  
	if (scp == NULL) {
		dprintf(D_ALWAYS, "Can't find SCP in config file");
		return false;
	}

	char *dstHost = "localhost";
	char *dstDir  = "/tmp/dir";

	sprintf(buf, "%s -q -B -P %d -i %s -oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null %s %s %s:%s > /dev/null 2>&1 < /dev/null", scp, port, privKeyFile, privKeyFile, contactFileSrc, dstHost, dstDir);

		// and run it...
	dprintf(D_ALWAYS, "Sending private keys over via: %s\n", buf);

	int r = system(buf);

	free(scp);
	if (r == 0) {
		return true;
	} else {
		return false;
	}
}

// Need cluster & node id ?
char* 
SshdWrapper::generateContactFileLine(int cluster, int node)
{
	return 0;
}
