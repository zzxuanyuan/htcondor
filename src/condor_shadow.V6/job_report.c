
#include "job_report.h"
#include "syscall_numbers.h"

/* A horizontal bar to be printed many times in the report. */

static char *job_report_bar = "------------------------------------\n";

/*
System call numbers can be negative, so figure out how big the table must
big enough to hold all of them.
*/

#define SYSCALL_COUNT_SIZE (CONDOR_SYSCALL_MAX-CONDOR_SYSCALL_MIN)+1
static int syscall_counts[SYSCALL_COUNT_SIZE] = {0};

/* Record the execution of one system call. */

int job_report_add_call( int call )
{
	if( call>CONDOR_SYSCALL_MAX ) return 0;
	if( call<CONDOR_SYSCALL_MIN ) return 0;

	return ++syscall_counts[call-CONDOR_SYSCALL_MIN];
}

/* Display a list of all the calls made */

int job_report_display_calls( FILE *f )
{
	int i;

	fprintf(f,"\nRemote System Calls Performed\n");
	fprintf(f,job_report_bar);

	for( i=0; i<SYSCALL_COUNT_SIZE; i++ ) {
		if(syscall_counts[i]) {
			fprintf(f,"%-30s %5d\n",
				_condor_syscall_name(i+CONDOR_SYSCALL_MIN),
				syscall_counts[i]);
		}
	}

	fprintf(f,job_report_bar);
}

/*
The user job is going to send along arbitrary bits of text to
include in the system call report.

Some of these are classified as performance info, and some are
classified as errors.  Keep a separate list for each one.
*/

struct job_report_text {
	char *text;
	struct job_report_text *next;
};

/* Private functions for manipulating the lists */

static int job_report_add_text( char *text, struct job_report_text **head );
static int job_report_display_text( FILE *f, struct job_report_text *node );

/* One list for info, one list for errors. */

static struct job_report_text *job_report_info_head=0;
static struct job_report_text *job_report_error_head=0;

int job_report_add_info( char *format, ... )
{
	static char buffer[JOB_REPORT_RECORD_MAX];

	va_list args;
	va_start( args, format );

	vsprintf( buffer, format, args );

	va_end( args );

	return job_report_add_text( buffer, &job_report_info_head );
}

int job_report_add_error( char *format, ... )
{
	static char buffer[JOB_REPORT_RECORD_MAX];

	va_list args;
	va_start( args, format );

	vsprintf( buffer, format, args );

	va_end( args );

	return job_report_add_text( buffer, &job_report_error_head );
}

int job_report_display_info( FILE *f )
{
	if(job_report_info_head) {
		fprintf(f,"\nRemote Execution Notices\n");
		fprintf(f,job_report_bar);
		job_report_display_text( f, job_report_info_head );
		fprintf(f,job_report_bar);
	}
}

int job_report_display_errors( FILE *f )
{
	if(job_report_error_head) {
		fprintf(f,"\nRemote Execution Errors\n");
		fprintf(f,job_report_bar);
		job_report_display_text( f, job_report_error_head );
		fprintf(f,job_report_bar);
	}
}

/* Private function: Add text to the end of a list */

static int job_report_add_text( char *text, struct job_report_text **head )
{
	struct job_report_text *node;

	/* Create a node containing the text */
	
	node = malloc(sizeof(struct job_report_text));
	if(!node) return 0;

	node->text = malloc(strlen(text)+1);
	if(!node->text) {
		free(node);
		return 0;
	}
	strcpy(node->text,text);

	/* Put the notice at the head of the list */

	node->next = *head;
	*head = node;

	return 1;
}

/* Private function: display a list */

static int job_report_display_text( FILE *f, struct job_report_text *node )
{
	/* The most recently added node is at the head of the list,
	   so find the end of the list recursively. */

	if(node) {
		if(node->next) {
			job_report_display_text(f,node->next);
			free(node->next);
		}
		fprintf(f,"%s\n",node->text);
	}
	return 1;
}

