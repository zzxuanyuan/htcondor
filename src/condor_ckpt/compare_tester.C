#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<time.h>
#include<signal.h> 
#include<setjmp.h>
#include<assert.h>

/** a very simple program that allocates a large chuck of memory and then
* traipses purposefully through creating various patterns of read and write
* accesses.  It will periodically ckpt itself, these ckpts can then be 
* compared in order to check the validity of the compare tool
*/

// some definitions
#define dprintf(x) if (DEBUG) printf x
#define usage(x) printf("Usage: %s [-s size] [-d]\n", x)
#define DEFAULT_ALLOC_SIZE 1000 * getpagesize()
#define RANDOM_CHAR (char)random()

// externs
extern int errno;

// functions
void readArgs(int argc, char ** argv);
void ckpt();
void rename_ckpt_file(char * desc, int ckpt_num);
void increase_stack_size();
char * getpagestart (char * );


// global variables
int allocSize = -1;	// size to allocate
bool DEBUG = true;
int ckpt_num = 1;   // to rename and save each successive ckpt file
char ckpt_file_path [256];
int pagesize = -1;

int 
main (int argc, char ** argv) {
	// set up the name of the ckpt file
	sprintf(ckpt_file_path, "%s.ckpt", argv[0]);

	// seed random, and set pagesize 
	srandom(time(NULL));
	pagesize = getpagesize();

	readArgs(argc, argv);

	// now actually allocate the memory
	char * array = (char *) malloc(allocSize);
	if (array == NULL) {
		perror("malloc failed");
	}

	// save the initial image
	dprintf(("Saving the initial image\n"));
	ckpt();
	rename_ckpt_file("first", ckpt_num++);

	// now linearly touch each page and ckpt and rename
	int i;
	dprintf(("Linearly touching all pages\n"));
	for (i = 0; i < allocSize; i++) {
		array[i]++; 
	}
	ckpt();
	rename_ckpt_file("linear", ckpt_num++);

	// now linearly touch every other byte and ckpt and rename
	dprintf(("Linearly touching every other byte all pages\n"));
	for (i = 0; i < allocSize; i += 2) {
		array[i]++; 
	}
	ckpt();
	rename_ckpt_file("every_other_byte", ckpt_num++);

	// now linearly touch every fourth byte and ckpt and rename
	dprintf(("Linearly touching every fourth byte all pages\n"));
	for (i = 0; i < allocSize; i += 4) {
		array[i]++; 
	}
	ckpt();
	rename_ckpt_file("every_fourth_byte", ckpt_num++);

	// now randomly touch a bunch of pages and ckpt and rename
	dprintf(("Randomly touching a bunch of random pages\n"));
	for (i = 0; i < allocSize; i++) {
		array[random() % allocSize] = RANDOM_CHAR;
	} 
	ckpt();
	rename_ckpt_file("random", ckpt_num++);

	// now linearly touch the first half of the array and ckpt and rename
	dprintf(("Linearly touching the first half of the array\n"));
	for (i = 0; i < allocSize/2; i++) {
		array[i]++;
	}
	ckpt();
	rename_ckpt_file("linear_first_half", ckpt_num++);

	// now randomly touch the first half of the array and ckpt and rename
	dprintf(("Randomly touching the first half of the array\n"));
	for (i = 0; i < allocSize; i++) {
		array[random() % allocSize/2] = RANDOM_CHAR;
	}
	ckpt();
	rename_ckpt_file("random_first_half", ckpt_num++);
	
	// now touch one byte on every other page and ckpt and rename
	dprintf(("Touching one byte on every other page\n"));
	for (i = 0; i < allocSize; i += pagesize*2) {
		array[i]++;
	}
	ckpt();
	rename_ckpt_file("one_byte_every_other_page", ckpt_num++);
	
	// now touch every byte on every other page and ckpt and rename
	dprintf(("Touching every byte on every other page\n"));
	// start with the first full page of the array
	char * pagealignedarray = getpagestart (array) + pagesize;
	for (int page = 1; page < allocSize / pagesize; page += 2) {
		for (i = 0; i < pagesize; i++) {
			pagealignedarray[page * pagesize + i]++;
		}
	}
	ckpt();
	rename_ckpt_file("every_byte_every_other_page", ckpt_num++);

	// now just read every page and ckpt and rename
	dprintf(("Reading every page, no writes\n"));
	int sum;
	for (i = 0; i < allocSize; i++) {
		sum += array[i];
	}
	ckpt();
	rename_ckpt_file("read_only", ckpt_num++);

	/****** I couldn't get this to work right *******
	// this should really be wrapped in a #ifdef
	// now pull in a shared library
	dprintf(("Pulling in a shared library, only interesting on Solaris\n"));
	jmp_buf jb;
	setjmp(jb);
	ckpt();
	rename_ckpt_file("new_library", ckpt_num++);	
	************************************************/

	// now make the stack segment larger and ckpt and rename
	increase_stack_size();

	// now make the data segment larger and ckpt and rename
	dprintf(("Increasing data segment size\n"));
	char * array2 = (char *) malloc(allocSize);
	ckpt();
	rename_ckpt_file("larger_data", ckpt_num++);

	dprintf(("\nNow run compare against each successive pair of ckpts.\n"));
	return 0;
}

/** this function increases the stack size and then ckpts and renames */
void
increase_stack_size() {
	char array[allocSize];
	dprintf(("Increasing stack segment size.\n"));
	ckpt();
	rename_ckpt_file("larger_stack", ckpt_num++);
}

/** this function saves the current ckpt_file */
void
rename_ckpt_file(char * desc, int ckpt_num) {
	char new_name[256];
	sprintf(new_name, "ckpt.%i.%s", ckpt_num, desc);
	int status = rename (ckpt_file_path, new_name);
	if (status < 0) {
		perror("rename failed");
		exit(-1);
	}
	dprintf(("saved %s as %s\n", ckpt_file_path, new_name));
}	

/* takes an address and returns the start boundary of the containing page */
char *
getpagestart (char * addr) {
    char * p;
    // align to a multiple of pagesize, p will be >= to mem
    p = (char *)(((int) addr + pagesize-1) & ~(pagesize-1));
    if (p != addr) {
        assert (p > addr);
        p -= pagesize;
    }
    return p;
}


/** this function calls the condor library and initiates a ckpt event */
void
ckpt() {
	// this code eventually will be linked with condor stand-alone ckpting
	// and the appropriate thing will be place here to initiate a ckpt
	// for the time being, just to develop the code on my laptop, I will just
	// create a bogus file

	int fd = creat(ckpt_file_path, S_IRWXU);
	if (fd < 0) {
		perror("creat failed");
		exit(-1);
	}	
	kill( getpid(), SIGUSR2 ); /* this will make condor checkpoint */
	dprintf(("\tCheckpointing, "));
}

void 
readArgs (int argc, char ** argv) {
	int curArg = 1;

	while (curArg < argc) {
		if ( ! strcmp(argv[curArg], "-s")) {
			allocSize = atoi(argv[++curArg]) * pagesize;
		} else if ( ! strcmp(argv[curArg], "-d")) {
			DEBUG = false;
		} else {
			usage(argv[0]);
			exit(-1);
		}
		curArg++;
	}

	if (allocSize < 0) {
		allocSize = DEFAULT_ALLOC_SIZE;
	}
	dprintf(("allocSize is %i (%i pages)\n", allocSize, allocSize / pagesize));
}
