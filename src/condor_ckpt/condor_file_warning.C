
#include "condor_common.h"
#include "image.h"
#include "condor_file.h"
#include "condor_syscall_mode.h"
#include "condor_debug.h"
#include "condor_sys.h"

extern "C" void _condor_file_warning( char *format, ... )
{
	va_list	args;
	va_start(args,format);

	static char text[1024];
	vsprintf(text,format,args);

	if(MyImage.GetMode()==STANDALONE) {
		fprintf(stderr,"Condor Warning: %s\n",text);
	} else if(LocalSysCalls()) {
		dprintf(D_ALWAYS,"Condor Warning: %s\n",text);
	} else {
		REMOTE_syscall( CONDOR_report_error, text );
	}

	va_end(args);
}

extern "C" void __pure_virtual()
{
}
