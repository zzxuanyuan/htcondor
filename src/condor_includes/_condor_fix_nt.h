#if !defined(_CONDOR_FIX_NT)
#define _CONDOR_FIX_NT

#include <io.h>
#define lseek _lseek
#include <sys/stat.h>
#define stat _stat
#define MAXPATHLEN 1024
#define MAXHOSTNAMELEN 64
#define strcasecmp _stricmp
#define strncasecmp _strnicmp

#endif
