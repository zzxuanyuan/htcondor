
#include "job_report.h"
#include "syscall_numbers.h"
#include "condor_sys.h"

/* An array to store a count of all system calls */

#define SYSCALL_COUNT_SIZE (CONDOR_SYSCALL_MAX-CONDOR_SYSCALL_MIN)+1
static int syscall_counts[SYSCALL_COUNT_SIZE] = {0};

/* Counts of various I/O operations */

static int read_count=0, read_bytes=0;
static int write_count=0, write_bytes=0;
static int seek_count=0;
static int actual_read_count=0, actual_read_bytes=0;
static int actual_write_count=0, actual_write_bytes=0;

/* A linked list for storing text reports */

struct job_report_text {
	char *text;
	struct job_report_text *next;
};

/* Private functions for manipulating the lists */

static int job_report_store_text( char *text, struct job_report_text **head );
static int job_report_display_text( FILE *f, struct job_report_text *node );

/* One list for info, one list for errors. */

static struct job_report_text *job_report_info_head=0;
static struct job_report_text *job_report_error_head=0;

/* Record the execution of one system call. */

int job_report_store_call( int call )
{
	if( call>CONDOR_SYSCALL_MAX ) return -1;
	if( call<CONDOR_SYSCALL_MIN ) return -1;

	return ++syscall_counts[call-CONDOR_SYSCALL_MIN];
}

/* Record a line of non-error information about the job */

int job_report_store_info( char *format, ... )
{
	static char buffer[JOB_REPORT_RECORD_MAX];

	va_list args;
	va_start( args, format );

	vsprintf( buffer, format, args );

	va_end( args );

	return job_report_store_text( buffer, &job_report_info_head );
}

/* Record a line of error information about the job */

int job_report_store_error( char *format, ... )
{
	static char buffer[JOB_REPORT_RECORD_MAX];

	va_list args;
	va_start( args, format );

	vsprintf( buffer, format, args );

	va_end( args );

	return job_report_store_text( buffer, &job_report_error_head );
}

/* Store the current number of I/O ops performed */

void job_report_store_file_totals( int rc, int rb, int wc, int wb, int sc, int arc, int arb, int awc, int awb )
{
	read_count = rc;
	read_bytes = rb;
	write_count = wc;
	write_bytes = wb;
	seek_count = sc;	
	actual_read_count = arc;
	actual_read_bytes = arb;
	actual_write_count = awc;
	actual_write_bytes = awb;
}

/* Display a list of all the calls made */

void job_report_display_calls( FILE *f )
{
	int i;

	fprintf(f,"\nRemote System Calls:\n");

	for( i=0; i<SYSCALL_COUNT_SIZE; i++ ) {
		if(syscall_counts[i]) {
			fprintf(f,"\t%-30s %5d\n",
				_condor_syscall_name(i+CONDOR_SYSCALL_MIN),
				syscall_counts[i]);
		}
	}
}

/* Display the list of all info lines recorded */

void job_report_display_info( FILE *f )
{
	if(job_report_info_head) {
		fprintf(f,"\nNotices:\n");
		job_report_display_text( f, job_report_info_head );
	}
}

/* Display the list of errors recorded */

void job_report_display_errors( FILE *f )
{
	if(job_report_error_head) {
		fprintf(f,"\n*** ERRORS:\n");
		job_report_display_text( f, job_report_error_head );
		fprintf(f,"***\n");
	}
}

/* Display the total I/O ops performed */

void job_report_display_file_totals( FILE *f, int total_time )
{
	float seek_ratio=0, buffer_eff=0;
	int effective_tput=0, actual_tput=0;

	if((read_count+write_count)>0) {
		buffer_eff = 100.0-100.0*(actual_read_count+actual_write_count)/(read_count+write_count);
		seek_ratio = 100.0*seek_count/(read_count+write_count);
	}

	if(total_time>0) {
		effective_tput = (read_bytes+write_bytes)/total_time;
		actual_tput = (actual_read_bytes+actual_write_bytes)/total_time;
	}

	fprintf(f,"\nI/O Performed:\n");
	fprintf(f,"\t%d reads totaling %s\n",
		read_count, metric_units(read_bytes) );
	fprintf(f,"\t%d writes totaling %s\n",
		write_count, metric_units(write_bytes) );
	fprintf(f,"\t%d seeks\n", seek_count );
	fprintf(f,"\t%5.2f%% seek ratio\n",seek_ratio);

	fprintf(f,"\nI/O Actually Done After Buffering:\n");
	fprintf(f,"\t%d reads totaling %s\n",
		actual_read_count, metric_units(actual_read_bytes) );
	fprintf(f,"\t%d writes totaling %s\n",
		actual_write_count, metric_units(actual_write_bytes) );
	fprintf(f,"\t%5.2f%% buffer efficiency\n",buffer_eff);
	fprintf(f,"\t%s/s effective throughput\n",
		metric_units(effective_tput));
	fprintf(f,"\t%s/s actual throughput\n",
		metric_units(actual_tput));
}

/* Send the stored info back to the Q */

void job_report_update_queue( PROC *proc )
{
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_READ_COUNT, read_count );
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_READ_BYTES, read_bytes );
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_WRITE_COUNT, write_count );
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_WRITE_BYTES, write_bytes );
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_SEEK_COUNT, seek_count);
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_ACTUAL_READ_COUNT, actual_read_count );
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_ACTUAL_READ_BYTES, actual_read_bytes );
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_ACTUAL_WRITE_COUNT, actual_write_count );
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_ACTUAL_WRITE_BYTES, actual_write_bytes );
}

/* Private function: Add text to the end of a list */

static int job_report_store_text( char *text, struct job_report_text **head )
{
	struct job_report_text *node;

	/* Create a node containing the text */
	
	node = (struct job_report_text *) malloc(sizeof(struct job_report_text));
	if(!node) return 0;

	node->text = (char *) malloc(strlen(text)+1);
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
		fprintf(f,"\t%s\n",node->text);
	}
	return 1;
}
