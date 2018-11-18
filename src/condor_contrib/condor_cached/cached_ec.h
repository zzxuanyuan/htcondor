
#ifndef __CACHED_EC_H__
#define __CACHED_EC_H__

#include <string>
#include <list>
#include <vector>

class ErasureCoder {

friend class CachedServer;

public:
	ErasureCoder();
	~ErasureCoder();

private:
	// Encoding a directory
	int JerasureEncodeDir(const std::string directory, const int k, const int m, std::string codeTech, const int w=8, const int packetsize=1024, const int buffersize=500000);

	// Encoding list of files
	std::vector<std::string> JerasureEncodeFile(const std::string file, const int k, const int m, std::string codeTech, const int w=8, const int packetsize=1024, const int buffersize=500000);

	// Decoding a directory
	int JerasureDecodeFile(const std::string filePath, int& returnK, int& returnM, std::string& returnCodeTech, int& returnW, int& returnPacketSize, int& returnBufferSize);
};

#endif
