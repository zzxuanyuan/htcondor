
#include "job_report.h"
#include "syscall_numbers.h"
#include "condor_sys.h"
#include "pseudo_ops.h"

/* An array to store a count of all system calls */

#define SYSCALL_COUNT_SIZE (CONDOR_SYSCALL_MAX-CONDOR_SYSCALL_MIN)+1
static int syscall_counts[SYSCALL_COUNT_SIZE] = {0};

/* Record the execution of one system call. */

int job_report_store_call( int call )
{
	if( call>CONDOR_SYSCALL_MAX ) return -1;
	if( call<CONDOR_SYSCALL_MIN ) return -1;

	return ++syscall_counts[call-CONDOR_SYSCALL_MIN];
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

/* A linked list for storing text reports */

struct error_node {
	char *text;
	struct error_node *next;
};

static struct error_node *error_head=0;

/* Record a line of error information about the job */

int job_report_store_error( char *format, ... )
{
	struct error_node *e;

	va_list args;
	va_start( args, format );

	/* Create a node containing the text */
	
	e = (struct error_node *) malloc(sizeof(struct error_node));
	if(!e) return 0;

	e->text = (char *) malloc(JOB_REPORT_RECORD_MAX);
	if(!e->text) {
		free(e);
		return 0;
	}
	vsprintf( e->text, format, args );

	e->next = error_head;
	error_head = e;

	va_end( args );

	return 1;
}

/* Display the list of stored errors */

void job_report_display_errors( FILE *f )
{
	struct error_node *i;

	if( error_head ) {
		fprintf( f, "*** ERRORS:\n");

		for( i=error_head; i; i=i->next ) {
			fprintf(f,"\t* %s\n",i->text);
		}
	}
}


/* A linked list for storing file reports */

struct file_info {
	char kind[_POSIX_PATH_MAX];
	char name[_POSIX_PATH_MAX];
	int read_count, write_count, seek_count;
	int read_bytes, write_bytes;
	struct file_info *next;
};

static struct file_info *open_files=0;
static struct file_info *closed_files=0;

/* Store the current number of I/O ops performed */

void job_report_store_file_info( char *kind, char *name, int rc, int wc, int sc, int rb, int wb, int done )
{
	struct file_info *i;

	if( !rc && !wc && !sc ) return;

	/* Has this file been opened and closed before? */

	if(done) {
		for( i=closed_files; i; i=i->next ) {
			if(!strcmp(i->name,name) && !strcmp(i->kind,kind)) {
				i->read_count += rc;
				i->write_count += wc;
				i->seek_count += sc;
				i->read_bytes += rb;
				i->write_bytes += wb;
				return;
			}
		}
	}

	/* If not, make a new node. */

	i = (struct file_info *) malloc(sizeof(struct file_info));
	if(!i) return;

	strcpy( i->kind, kind );
	strcpy( i->name, name );
	i->read_count = rc;
	i->write_count = wc;
	i->seek_count = sc;
	i->read_bytes = rb;
	i->write_bytes = wb;

	if(done) {
		i->next = closed_files;
		closed_files = i;
	} else {
		i->next = open_files;
		open_files = i;
	}
}

static void sum_file_info( struct file_info *buffered, struct file_info *actual )
{
	struct file_info *i, *j;

	actual->read_count=0;
	actual->write_count=0;
	actual->seek_count=0;
	actual->read_bytes=0;
	actual->write_bytes=0;

	buffered->read_count=0;
	buffered->write_count=0;
	buffered->seek_count=0;
	buffered->read_bytes=0;
	buffered->write_bytes=0;

	for( i=open_files; i; i=j ) {
		j = i->next;
		if(strcmp(i->kind,"buffer")) {
			actual->read_count += i->read_count;
			actual->write_count += i->write_count;
			actual->seek_count += i->seek_count;
			actual->read_bytes += i->read_bytes;
			actual->write_bytes += i->write_bytes;
		} else {
			buffered->read_count += i->read_count;
			buffered->write_count += i->write_count;
			buffered->seek_count += i->seek_count;
			buffered->read_bytes += i->read_bytes;
			buffered->write_bytes += i->write_bytes;
		}
		free(i);
	}
	open_files = 0;

	for( i=closed_files; i; i=i->next ) {
		if(strcmp(i->kind,"buffer")) {
			actual->read_count += i->read_count;
			actual->write_count += i->write_count;
			actual->seek_count += i->seek_count;
			actual->read_bytes += i->read_bytes;
			actual->write_bytes += i->write_bytes;
		} else {
			buffered->read_count += i->read_count;
			buffered->write_count += i->write_count;
			buffered->seek_count += i->seek_count;
			buffered->read_bytes += i->read_bytes;
			buffered->write_bytes += i->write_bytes;
		}
	}
}

/* Display the total I/O ops performed */

void job_report_display_file_info( FILE *f, int total_time )
{
	struct file_info *i;
	struct file_info actual, buffered;

	sum_file_info( &buffered, &actual );
	
	fprintf(f,"\nBuffered I/O performed:\n\n");
	fprintf(f,"\t%d reads totaling %s\n",
		buffered.read_count, metric_units(buffered.read_bytes) );
	fprintf(f,"\t%d writes totaling %s\n",
		buffered.write_count, metric_units(buffered.write_bytes) );
	fprintf(f,"\t%d seeks\n", buffered.seek_count );
	
	fprintf(f,"\nActual I/O performed:\n\n");
	fprintf(f,"\t%d reads totaling %s\n",
		actual.read_count, metric_units(actual.read_bytes) );
	fprintf(f,"\t%d writes totaling %s\n",
		actual.write_count, metric_units(actual.write_bytes) );
	fprintf(f,"\t%d seeks\n", actual.seek_count );

	fprintf(f,"\nI/O sorted by file:\n\n");

	for( i=closed_files; i; i=i->next ) {
		fprintf(f,"%s\n%s\n",i->kind,i->name);
		fprintf(f,"\t%d reads totaling %s\n",
			i->read_count, metric_units(i->read_bytes) );
		fprintf(f,"\t%d writes totaling %s\n",
			i->write_count, metric_units(i->write_bytes) );
		fprintf(f,"\t%d seeks\n", i->seek_count );
	}
}

/* Send the stored info back to the Q */

void job_report_update_queue( PROC *proc )
{
	struct file_info actual, buffered;

	sum_file_info( &buffered, &actual );

	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_ACTUAL_READ_COUNT, actual.read_count );
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_ACTUAL_READ_BYTES, actual.read_bytes );
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_ACTUAL_WRITE_COUNT, actual.write_count );
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_ACTUAL_WRITE_BYTES, actual.write_bytes );
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_ACTUAL_SEEK_COUNT, actual.seek_count );
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_READ_COUNT, buffered.read_count );
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_READ_BYTES, buffered.read_bytes );
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_WRITE_COUNT, buffered.write_count );
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_WRITE_BYTES, buffered.write_bytes );
	SetAttributeInt( proc->id.cluster, proc->id.proc, ATTR_FILE_SEEK_COUNT, buffered.seek_count );
}


