#if !defined(_CONDOR_FIX_NT)
#define _CONDOR_FIX_NT

#include <io.h>
#include <fcntl.h>
#include <direct.h>		// for _chdir , etc
#define lseek _lseek
#define O_RDONLY _O_RDONLY
#define O_WRONLY _O_WRONLY
#define O_RDWR _O_RDWR
#define O_CREAT _O_CREAT
#include <sys/stat.h>
#define stat _stat
#define MAXPATHLEN 1024
#define MAXHOSTNAMELEN 64
#define	_POSIX_PATH_MAX 255
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define strdup _strdup
#define chdir _chdir
#include <time.h>
#include <lmcons.h> // for UNLEN

#endif
