#if !defined(_CONDOR_FIX_NT)
#define _CONDOR_FIX_NT

#include <io.h>
#include <fcntl.h>
#define lseek _lseek
#define O_RDONLY _O_RDONLY
#define O_WRONLY _O_WRONLY
#define O_RDWR _O_RDWR
#define O_CREAT _O_CREAT
#include <sys/stat.h>
#define stat _stat
#define MAXPATHLEN 1024
#define MAXHOSTNAMELEN 64
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#include <time.h>

#endif
