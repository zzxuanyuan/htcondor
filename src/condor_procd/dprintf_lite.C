#include "condor_common.h"
#include "condor_debug.h"

FILE* debug_fp = NULL;

extern "C" void
dprintf(int, const char* format, ...)
{
	if (debug_fp != NULL) {
		va_list ap;
		va_start(ap, format);
		vfprintf(debug_fp, format, ap);
		va_end(ap);
		fflush(debug_fp);
	}
}

int	_EXCEPT_Line;
char*	_EXCEPT_File;
int	_EXCEPT_Errno;

extern "C" void
_EXCEPT_(char* format, ...)
{
	if (debug_fp) {
		fprintf(debug_fp, "ERROR: ");
		va_list ap;
		va_start(ap, format);
		vfprintf(debug_fp, format, ap);
		va_end(ap);
		fprintf(debug_fp, "\n");
		fflush(debug_fp);
	}
	exit(1);
}
