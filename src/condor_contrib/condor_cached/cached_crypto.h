#ifndef __CACHED_CRYPTO_H__
#define __CACHED_CRYPTO_H__

#include <string>

class Cryptographer {

friend class CachedServer;

public:
	Cryptographer();
	~Cryptographer();

private:
	// Encrypt a file
	int EncryptFile(const std::string file, const std::string algorithm, const int buffersize = 1048576);

	// Decrypt a file
	int DecryptFile(const std::string file, const std::string algorithm, const int buffersize = 1048576);
};

#endif
