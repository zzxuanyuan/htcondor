#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<time.h>
#include<signal.h> 

/** a very simple program that allocates a large chuck of memory and then
* traipses purposefully through creating various patterns of read and write
* accesses.  It will periodically ckpt itself, these ckpts can then be 
* compared in order to check the validity of the compare tool
*/

// some definitions
#define dprintf(x) if (DEBUG) printf x
#define usage(x) printf("Usage: %s [-s size] [-d]\n", x)
#define DEFAULT_ALLOC_SIZE 1000 * getpagesize()
#define RANDOM_CHAR (char)rand()

// externs
extern int errno;

// functions
void readArgs(int argc, char ** argv);
void ckpt();
void rename_ckpt_file(char * desc, int ckpt_num);

// global variables
int allocSize = -1;	// size to allocate
bool DEBUG = true;
int ckpt_num = 1;   // to rename and save each successive ckpt file
char ckpt_file_path [256];

int 
main (int argc, char ** argv) {
	readArgs(argc, argv);

	// set up the name of the ckpt file
	sprintf(ckpt_file_path, "%s.ckpt", argv[0]);

	// seed random, used for random writes and for dirty chars
	srand(time(NULL));

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

	// now randomly touch a bunch of pages and ckpt and rename
	dprintf(("Randomly touching a bunch of random pages\n"));
	for (i = 0; i < allocSize*2; i++) {
		array[rand() % allocSize] = RANDOM_CHAR;
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
	for (i = 0; i < allocSize/2; i++) {
		array[rand() % allocSize/2] = RANDOM_CHAR;
	}
	ckpt();
	rename_ckpt_file("random_first_half", ckpt_num++);
	
	// now touch every other page and ckpt and rename
	dprintf(("Touching every other page\n"));
	for (i = 0; i < allocSize; i += getpagesize()*2) {
		array[i]++;
	}
	ckpt();
	rename_ckpt_file("every_other_page", ckpt_num++);

	// now just read every page and ckpt and rename
	dprintf(("Reading every page, no writes\n"));
	int sum;
	for (i = 0; i < allocSize; i++) {
		sum += array[i];
	}
	ckpt();
	rename_ckpt_file("read_only", ckpt_num++);

	dprintf(("\nNow run compare against each successive pair of ckpts.\n"));
	return 0;
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
	dprintf(("Checkpointing, "));
}

void 
readArgs (int argc, char ** argv) {
	int curArg = 1;

	while (curArg < argc) {
		if ( ! strcmp(argv[curArg], "-s")) {
			allocSize = atoi(argv[++curArg]) * getpagesize();
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
	dprintf(("allocSize is %i (%i pages)\n", allocSize, allocSize / getpagesize()));
}
