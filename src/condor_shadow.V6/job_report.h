
#ifndef JOB_REPORT_H
#define JOB_REPORT_H

#include "condor_common.h"

BEGIN_C_DECLS

int job_report_add_call( int call );
int job_report_add_info( char *text );
int job_report_add_error( char *text );

int job_report_display_errors( FILE *f );
int job_report_display_calls( FILE *f );
int job_report_display_info( FILE *f );

END_C_DECLS

#endif
