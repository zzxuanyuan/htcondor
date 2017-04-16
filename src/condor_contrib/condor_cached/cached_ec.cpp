/* *
 * Copyright (c) 2014, James S. Plank and Kevin Greenan
 * All rights reserved.
 *
 * Jerasure - A C/C++ Library for a Variety of Reed-Solomon and RAID-6 Erasure
 * Coding Techniques
 *
 * Revision 2.0: Galois Field backend now links to GF-Complete
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 *  - Neither the name of the University of Tennessee nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* Jerasure's authors:
   Revision 2.x - 2014: James S. Plank and Kevin M. Greenan.
   Revision 1.2 - 2008: James S. Plank, Scott Simmerman and Catherine D. Schuman.
   Revision 1.0 - 2007: James S. Plank.
   */

/* 
   This program takes as input an inputfile, k, m, a coding 
   technique, w, and packetsize.  It creates k+m files from 
   the original file so that k of these files are parts of 
   the original file and m of the files are encoded based on 
   the given coding technique. The format of the created files 
   is the file name with "_k#" or "_m#" and then the extension.  
   (For example, inputfile test.txt would yield file "test_k1.txt".)
   */
#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "cached_server.h"
#include "compat_classad.h"
#include "file_transfer.h"
#include "condor_version.h"
#include "classad_log.h"
#include "get_daemon_name.h"
#include "ipv6_hostname.h"
#include "basename.h"

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <assert.h>
//#include <time.h>
//#include <sys/time.h>
//#include <sys/stat.h>
//#include <unistd.h>
//#include <string.h>
//#include <stdio.h>
//#include <stdlib.h>
//#include <errno.h>
//#include <signal.h>
//#include <unistd.h>
//#include "condor_common.h"
//#include "condor_config.h"
//#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "cached_ec.h"
//#include "gf_rand.h"
#include "gf_complete.h"
#include "jerasure.h"
#include "jerasure/reed_sol.h"
#include "jerasure/cauchy.h"
#include "jerasure/liberation.h"
#include <boost/filesystem.hpp>

using namespace std;
using namespace boost::filesystem;

enum Coding_Technique {Reed_Sol_Van, Reed_Sol_R6_Op, Cauchy_Orig, Cauchy_Good, Liberation, Blaum_Roth, Liber8tion, RDP, EVENODD, No_Coding};

//static std::string Methods = {"reed_sol_van", "reed_sol_r6_op", "cauchy_orig", "cauchy_good", "liberation", "blaum_roth", "liber8tion", "no_coding"};

static int readins, n;
static enum Coding_Technique method;

/* Function prototypes */
static int  is_prime(int w);
static int  jerasureEncoder (std::string fileName, int k, int m, std::string codeTech, int w, int packetsize, int buffersize);
static int  jerasureDecoder (std::string fileName);

static void print_datablock(char *data, int blocksize){
        int i;
        for(i=0;i<15;++i){
                dprintf(D_ALWAYS, "block[%d]=0x%x ,", i, data[i]);
                if(i%6==0) printf("\n");
        }
}

static void fill_region(void *reg, int size)
{
	uint32_t *r32;
	int i;
	r32 = (uint32_t *) reg;
	for (i = 0; i < size/4; i++) r32[i] = (uint32_t)rand();
}

static int jfread(void *ptr, int size, int nmembers, FILE *stream)
{
	if (stream != NULL) return fread(ptr, size, nmembers, stream);

	fill_region(ptr, size);
	return size;
}

ErasureCoder::ErasureCoder(){}

ErasureCoder::~ErasureCoder(){}

int ErasureCoder::JerasureEncodeDir (std::string directory, const int data, const int parity, std::string codeTech, const int w, const int packetsize, const int buffersize) {
	dprintf(D_ALWAYS, "In jerasureEncodeDir 1!!!\n");//##
	int rc = 0;
	path p(directory.c_str());
	dprintf(D_ALWAYS, "In jerasureEncodeDir 2!!!\n");//##
	dprintf(D_ALWAYS, "directory=%s\n",directory.c_str());//##

	directory_iterator end_itr;
	path file_name;
	// cycle through the directory
	for (directory_iterator itr(p); itr != end_itr; ++itr)
	{
		// If it's not a directory, list it. If you want to list directories too, just remove this check.
		file_name = itr->path();
		dprintf(D_ALWAYS, "file path = %s\n",file_name.c_str());//##
		if (is_regular_file(itr->path())) {
			dprintf(D_ALWAYS, "start encoding %s\n",file_name.c_str());//##
			// assign current file name to current_file and echo it out to the console.
			string current_file = itr->path().string();
			cout << current_file << endl;
			rc = jerasureEncoder(current_file, data, parity, codeTech, w, packetsize, buffersize);
		}
	}
	dprintf(D_ALWAYS, "In jerasureEncodeDir 3!!!\n");//##
	return rc;

}

int ErasureCoder::JerasureEncodeFile (const std::string file, const int data, const int parity, std::string codeTech, const int w, const int packetsize, const int buffersize) {
	dprintf(D_ALWAYS, "In JerasureEncodeFile file = %s\n", file.c_str());//##
	int rc = 0;
	rc = jerasureEncoder(file, data, parity, codeTech, w, packetsize, buffersize);
	return rc;
}

int ErasureCoder::JerasureDecodeFile (std::string filePath) {
	dprintf(D_ALWAYS, "In jerasureDecodeDir 1!!!\n");//##
	int rc = 0;
	path p(filePath.c_str());
	dprintf(D_ALWAYS, "In jerasureDecodeDir 2!!!\n");//##
	dprintf(D_ALWAYS, "filePath=%s\n",filePath.c_str());//##

	std::string file_name = p.string();
	rc = jerasureDecoder(file_name);
	dprintf(D_ALWAYS, "In jerasureDecodeDir 3!!!\n");//##
	return rc;
}

static int jerasureEncoder (std::string fileName, int k, int m, std::string codeTech, int w, int packetsize, int buffersize) {
	dprintf(D_ALWAYS, "In jerasureEncoder func!!!\n");//##
	const char *file_name = fileName.c_str();
	const char *coding_technique = codeTech.c_str();

	FILE *fp, *fp2;				// file pointers
	char *block;				// padding file
	int size, newsize;			// size of file and temp size 
	struct stat status;			// finding file size

	enum Coding_Technique tech;		// coding technique (parameter)
	int i;						// loop control variables
	int blocksize;					// size of k+m files
	int total;
	int extra;

	/* Jerasure Arguments */
	char **data;				
	char **coding;
	int *matrix;
	int *bitmatrix;
	int **schedule;

	/* Creation of file name variables */
	char temp[5];
	char *s1, *s2, *extension;
	char *fname;
	int md;
	const char *curdir;
	boost::filesystem::path Path(file_name);
	boost::filesystem::path Parent = Path.parent_path();
	curdir = Parent.c_str();
	boost::filesystem::path Dir(curdir);
	Dir /= "Coding";
	boost::filesystem::create_directory(Dir);

	/* Find buffersize */
	int up, down;

	/* Start timing */
	matrix = NULL;
	bitmatrix = NULL;
	schedule = NULL;
	dprintf(D_ALWAYS, "Printing parameters: k=%d,m=%d,w=%d,packetsize=%d,buffersize=%d,coding_technique=%s,file_name=%s,curdir=%s,Dir=%s\n",k,m,w,packetsize,buffersize,coding_technique,file_name,curdir,Dir.c_str());//##
	/* Conversion of parameters and error checking */	
	if (k <= 0) {
		fprintf(stderr,  "Invalid value for k\n");
		exit(0);
	}
	if (m < 0) {
		fprintf(stderr,  "Invalid value for m\n");
		exit(0);
	}
	if (w <= 0) {
		fprintf(stderr,  "Invalid value for w.\n");
		exit(0);
	}
	if (packetsize < 0) {
		fprintf(stderr,  "Invalid value for packetsize.\n");
		exit(0);
	}
	if (buffersize < 0) {
		fprintf(stderr, "Invalid value for buffersize\n");
		exit(0);
	}
	dprintf(D_ALWAYS, "before determining buffersize\n");//##
	/* Determine proper buffersize by finding the closest valid buffersize to the input value  */
	if (buffersize != 0) {
		if (packetsize != 0 && buffersize%(sizeof(long)*w*k*packetsize) != 0) { 
			up = buffersize;
			down = buffersize;
			while (up%(sizeof(long)*w*k*packetsize) != 0 && (down%(sizeof(long)*w*k*packetsize) != 0)) {
				up++;
				if (down == 0) {
					down--;
				}
			}
			if (up%(sizeof(long)*w*k*packetsize) == 0) {
				buffersize = up;
			}
			else {
				if (down != 0) {
					buffersize = down;
				}
			}
		}
		else if (packetsize == 0 && buffersize%(sizeof(long)*w*k) != 0) {
			up = buffersize;
			down = buffersize;
			while (up%(sizeof(long)*w*k) != 0 && down%(sizeof(long)*w*k) != 0) {
				up++;
				down--;
			}
			if (up%(sizeof(long)*w*k) == 0) {
				buffersize = up;
			}
			else {
				buffersize = down;
			}
		}
	}
	dprintf(D_ALWAYS, "before setting coding technique, coding_technique=%s\n", coding_technique);//##
	/* Setting of coding technique and error checking */

	if (strncmp(coding_technique, "no_coding", 9) == 0) {
		tech = No_Coding;
	}
	else if (strncmp(coding_technique, "reed_sol_van", 12) == 0) {
		dprintf(D_ALWAYS, "choosing Reed_Sol_Van as our coding technique\n");//##
		tech = Reed_Sol_Van;
		if (w != 8 && w != 16 && w != 32) {
			fprintf(stderr,  "w must be one of {8, 16, 32}\n");
			exit(0);
		}
	}
	else if (strncmp(coding_technique, "reed_sol_r6_op", 14) == 0) {
		if (m != 2) {
			fprintf(stderr,  "m must be equal to 2\n");
			exit(0);
		}
		if (w != 8 && w != 16 && w != 32) {
			fprintf(stderr,  "w must be one of {8, 16, 32}\n");
			exit(0);
		}
		tech = Reed_Sol_R6_Op;
	}
	else if (strncmp(coding_technique, "cauchy_orig", 11) == 0) {
		tech = Cauchy_Orig;
		if (packetsize == 0) {
			fprintf(stderr, "Must include packetsize.\n");
			exit(0);
		}
	}
	else if (strncmp(coding_technique, "cauchy_good", 11) == 0) {
		tech = Cauchy_Good;
		if (packetsize == 0) {
			fprintf(stderr, "Must include packetsize.\n");
			exit(0);
		}
	}
	else if (strncmp(coding_technique, "liberation", 10) == 0) {
		if (k > w) {
			fprintf(stderr,  "k must be less than or equal to w\n");
			exit(0);
		}
		if (w <= 2 || !(w%2) || !is_prime(w)) {
			fprintf(stderr,  "w must be greater than two and w must be prime\n");
			exit(0);
		}
		if (packetsize == 0) {
			fprintf(stderr, "Must include packetsize.\n");
			exit(0);
		}
		if ((packetsize%(sizeof(long))) != 0) {
			fprintf(stderr,  "packetsize must be a multiple of sizeof(long)\n");
			exit(0);
		}
		tech = Liberation;
	}
	else if (strncmp(coding_technique, "blaum_roth", 10) == 0) {
		if (k > w) {
			fprintf(stderr,  "k must be less than or equal to w\n");
			exit(0);
		}
		if (w <= 2 || !((w+1)%2) || !is_prime(w+1)) {
			fprintf(stderr,  "w must be greater than two and w+1 must be prime\n");
			exit(0);
		}
		if (packetsize == 0) {
			fprintf(stderr, "Must include packetsize.\n");
			exit(0);
		}
		if ((packetsize%(sizeof(long))) != 0) {
			fprintf(stderr,  "packetsize must be a multiple of sizeof(long)\n");
			exit(0);
		}
		tech = Blaum_Roth;
	}
	else if (strncmp(coding_technique, "liber8tion", 10) == 0) {
		if (packetsize == 0) {
			fprintf(stderr, "Must include packetsize\n");
			exit(0);
		}
		if (w != 8) {
			fprintf(stderr, "w must equal 8\n");
			exit(0);
		}
		if (m != 2) {
			fprintf(stderr, "m must equal 2\n");
			exit(0);
		}
		if (k > w) {
			fprintf(stderr, "k must be less than or equal to w\n");
			exit(0);
		}
		tech = Liber8tion;
	}
	else {
		dprintf(D_ALWAYS, "no coding technique is selected\n");//##
		fprintf(stderr,  "Not a valid coding technique. Choose one of the following: reed_sol_van, reed_sol_r6_op, cauchy_orig, cauchy_good, liberation, blaum_roth, liber8tion, no_coding\n");
		exit(0);
	}

	/* Set global variable method for signal handler */
	method = tech;

	/* Get current working directory for construction of file names */

	if (file_name[0] != '-') {
		dprintf(D_ALWAYS, "creating directory 1\n");//##
		/* Open file and error check */
		fp = fopen(file_name, "rb");
		dprintf(D_ALWAYS, "Open the file: file_name = %s\n", file_name);//##
		if (fp == NULL) {
			dprintf(D_ALWAYS, "Unable to open file.\n");//##
			fprintf(stderr,  "Unable to open file.\n");
			exit(0);
		}
		dprintf(D_ALWAYS, "creating directory 2\n");//##

		/* Create Coding directory */
		i = mkdir("Coding", S_IRWXU);
		if (i == -1 && errno != EEXIST) {
			dprintf(D_ALWAYS, "Unable to create Coding directory.\n");//##
			fprintf(stderr, "Unable to create Coding directory.\n");
			exit(0);
		}
		dprintf(D_ALWAYS, "creating directory 3\n");//##

		/* Determine original size of file */
		stat(file_name, &status);	
		size = status.st_size;
	} else {
		if (sscanf(file_name+1, "%d", &size) != 1 || size <= 0) {
			fprintf(stderr, "Files starting with '-' should be sizes for randomly created input\n");
			exit(1);
		}
		fp = NULL;
//		MOA_Seed(time(0));
		(uint32_t)rand();
	}

	newsize = size;

	/* Find new size by determining next closest multiple */
	if (packetsize != 0) {
		if (size%(k*w*packetsize*sizeof(long)) != 0) {
			while (newsize%(k*w*packetsize*sizeof(long)) != 0) 
				newsize++;
		}
	}
	else {
		if (size%(k*w*sizeof(long)) != 0) {
			while (newsize%(k*w*sizeof(long)) != 0) 
				newsize++;
		}
	}

	if (buffersize != 0) {
		while (newsize%buffersize != 0) {
			newsize++;
		}
	}

	dprintf(D_ALWAYS, "determining block size\n");//##
	/* Determine size of k+m files */
	blocksize = newsize/k;

	/* Allow for buffersize and determine number of read-ins */
	if (size > buffersize && buffersize != 0) {
		if (newsize%buffersize != 0) {
			readins = newsize/buffersize;
		}
		else {
			readins = newsize/buffersize;
		}
		block = (char *)malloc(sizeof(char)*buffersize);
		blocksize = buffersize/k;
	}
	else {
		readins = 1;
		buffersize = size;
		block = (char *)malloc(sizeof(char)*newsize);
	}

	/* Break inputfile name into the filename and extension */	
	s1 = (char*)malloc(sizeof(char)*(strlen(file_name)+20));
	char *file_dup = strdup(file_name);
	s2 = strrchr(file_dup, '/');
	if (s2 != NULL) {
		s2++;
		strcpy(s1, s2);
	}
	else {
		strcpy(s1, file_dup);
	}
	s2 = strchr(s1, '.');
	if (s2 != NULL) {
		extension = strdup(s2);
		*s2 = '\0';
	} else {
		extension = strdup("");
	}

	/* Allocate for full file name */
	fname = (char*)malloc(sizeof(char)*(strlen(file_name)+strlen(curdir)+20));
	sprintf(temp, "%d", k);
	md = strlen(temp);

	/* Allocate data and coding */
	data = (char **)malloc(sizeof(char*)*k);
	coding = (char **)malloc(sizeof(char*)*m);
	for (i = 0; i < m; i++) {
		coding[i] = (char *)malloc(sizeof(char)*blocksize);
		if (coding[i] == NULL) { perror("malloc"); exit(1); }
	}

	dprintf(D_ALWAYS, "allocating space for files\n");//##

	/* Create coding matrix or bitmatrix and schedule */
	switch(tech) {
		case No_Coding:
			break;
		case Reed_Sol_Van:
			dprintf(D_ALWAYS, "We are using Reed_Sol_Van\n");//##
			matrix = reed_sol_vandermonde_coding_matrix(k, m, w);
			break;
		case Reed_Sol_R6_Op:
			dprintf(D_ALWAYS, "We are using Reed_Sol_R6_Op\n");//##
			break;
		case Cauchy_Orig:
			dprintf(D_ALWAYS, "We are using Cauchy_Orig\n");//##
			matrix = cauchy_original_coding_matrix(k, m, w);
			bitmatrix = jerasure_matrix_to_bitmatrix(k, m, w, matrix);
			schedule = jerasure_smart_bitmatrix_to_schedule(k, m, w, bitmatrix);
			break;
		case Cauchy_Good:
			dprintf(D_ALWAYS, "We are using Cauchy_Good\n");//##
			matrix = cauchy_good_general_coding_matrix(k, m, w);
			bitmatrix = jerasure_matrix_to_bitmatrix(k, m, w, matrix);
			schedule = jerasure_smart_bitmatrix_to_schedule(k, m, w, bitmatrix);
			break;	
		case Liberation:
			dprintf(D_ALWAYS, "We are using Liberation\n");//##
			bitmatrix = liberation_coding_bitmatrix(k, w);
			schedule = jerasure_smart_bitmatrix_to_schedule(k, m, w, bitmatrix);
			break;
		case Blaum_Roth:
			dprintf(D_ALWAYS, "We are using Blaum_Roth\n");//##
			bitmatrix = blaum_roth_coding_bitmatrix(k, w);
			schedule = jerasure_smart_bitmatrix_to_schedule(k, m, w, bitmatrix);
			break;
		case Liber8tion:
			dprintf(D_ALWAYS, "We are using Liber8tion\n");//##
			bitmatrix = liber8tion_coding_bitmatrix(k);
			schedule = jerasure_smart_bitmatrix_to_schedule(k, m, w, bitmatrix);
			break;
		case RDP:
		case EVENODD:
			assert(0);
	}

	/* Read in data until finished */
	n = 1;
	total = 0;
	dprintf(D_ALWAYS,"readins = %d!!!\n",readins);//##
	while (n <= readins) {
		/* Check if padding is needed, if so, add appropriate 
		   number of zeros */
		if (total < size && total+buffersize <= size) {
			total += jfread(block, sizeof(char), buffersize, fp);
		}
		else if (total < size && total+buffersize > size) {
			extra = jfread(block, sizeof(char), buffersize, fp);
			for (i = extra; i < buffersize; i++) {
				block[i] = '0';
			}
		}
		else if (total == size) {
			for (i = 0; i < buffersize; i++) {
				block[i] = '0';
			}
		}

		/* Set pointers to point to file data */
		for (i = 0; i < k; i++) {
			data[i] = block+(i*blocksize);
		}

                int print_index = 0;//##
		dprintf(D_ALWAYS, "\n\nBefore, Here we are printing out all memory data\n\n");//##
                for(print_index = 0; print_index < k; ++print_index){
                        dprintf(D_ALWAYS, "\n\ndata block k=%d\n",print_index);//##
                        print_datablock(data[print_index],blocksize);//##
                }
                for(print_index = 0; print_index < m; ++print_index){
                        dprintf(D_ALWAYS, "\n\ncoding block m=%d\n",print_index);//##
                        print_datablock(coding[print_index],blocksize);//##
                }

		/* Encode according to coding method */
		switch(tech) {	
			case No_Coding:
				break;
			case Reed_Sol_Van:
				dprintf(D_ALWAYS,"We encoding with Reed_Sol_Van\n");//##
				jerasure_matrix_encode(k, m, w, matrix, data, coding, blocksize);
				break;
			case Reed_Sol_R6_Op:
				reed_sol_r6_encode(k, w, data, coding, blocksize);
				break;
			case Cauchy_Orig:
				jerasure_schedule_encode(k, m, w, schedule, data, coding, blocksize, packetsize);
				break;
			case Cauchy_Good:
				jerasure_schedule_encode(k, m, w, schedule, data, coding, blocksize, packetsize);
				break;
			case Liberation:
				jerasure_schedule_encode(k, m, w, schedule, data, coding, blocksize, packetsize);
				break;
			case Blaum_Roth:
				jerasure_schedule_encode(k, m, w, schedule, data, coding, blocksize, packetsize);
				break;
			case Liber8tion:
				jerasure_schedule_encode(k, m, w, schedule, data, coding, blocksize, packetsize);
				break;
			case RDP:
			case EVENODD:
				assert(0);
		}
		dprintf(D_ALWAYS, "Before creating Coding directory!!!\n");//##
		/* Write data and encoded data to k+m files */
		for	(i = 1; i <= k; i++) {
			if (fp == NULL) {
				bzero(data[i-1], blocksize);
			} else {
				sprintf(fname, "%s/Coding/%s_k%0*d%s", curdir, s1, md, i, extension);
				dprintf(D_ALWAYS, "fname=%s, curdir=%s, s1=%s, extension=%s\n", fname,curdir,s1,extension);//##
				if (n == 1) {
					fp2 = fopen(fname, "wb");
				}
				else {
					fp2 = fopen(fname, "ab");
				}
				dprintf(D_ALWAYS, "fname=%s, curdir=%s, s1=%s, extension=%s alive ? 1\n", fname,curdir,s1,extension);//##
				fwrite(data[i-1], sizeof(char), blocksize, fp2);
				dprintf(D_ALWAYS, "fname=%s, curdir=%s, s1=%s, extension=%s alive ? 2\n", fname,curdir,s1,extension);//##
				fclose(fp2);
			}

		}
		for	(i = 1; i <= m; i++) {
			if (fp == NULL) {
				bzero(data[i-1], blocksize);
			} else {
				sprintf(fname, "%s/Coding/%s_m%0*d%s", curdir, s1, md, i, extension);
				dprintf(D_ALWAYS, "fname=%s, curdir=%s, s1=%s, extension=%s\n", fname,curdir,s1,extension);//##
				if (n == 1) {
					fp2 = fopen(fname, "wb");
				}
				else {
					fp2 = fopen(fname, "ab");
				}
				fwrite(coding[i-1], sizeof(char), blocksize, fp2);
				fclose(fp2);
			}
		}
		n++;

		dprintf(D_ALWAYS, "\n\nAfter, Here we are printing out all memory data\n\n");//##
                for(print_index = 0; print_index < k; ++print_index){
                        dprintf(D_ALWAYS, "\n\ndata block k=%d\n",print_index);//##
                        print_datablock(data[print_index],blocksize);//##
                }
                for(print_index = 0; print_index < m; ++print_index){
                        dprintf(D_ALWAYS, "\n\ncoding block m=%d\n",print_index);//##
                        print_datablock(coding[print_index],blocksize);//##
                }
	}

	/* Create metadata file */
	if (fp != NULL) {
		sprintf(fname, "%s/Coding/%s_meta.txt", curdir, s1);
		dprintf(D_ALWAYS, "fname=%s, curdir=%s, s1=%s\n", fname,curdir,s1);//##
		fp2 = fopen(fname, "wb");
		fprintf(fp2, "%s\n", file_name);
		fprintf(fp2, "%d\n", size);
		fprintf(fp2, "%d %d %d %d %d\n", k, m, w, packetsize, buffersize);
		fprintf(fp2, "%s\n", coding_technique);
		fprintf(fp2, "%d\n", tech);
		fprintf(fp2, "%d\n", readins);
		fclose(fp2);
	}


	/* Free allocated memory */
	free(s1);
	free(fname);
	free(block);
//	free(curdir);
	fclose(fp);

	return 0;
}

static int jerasureDecoder(std::string fileName)
{
        FILE *fp;                               // File pointer
        const char *file_name = fileName.c_str();

        /* Jerasure arguments */
        char **data;
        char **coding;
        int *erasures;
        int *erased;
        int *matrix;
        int *bitmatrix;

        /* Parameters */
        int k, m, w, packetsize, buffersize;
        int tech;
        char *c_tech;

        int i, j;                               // loop control variable, s
        int blocksize = 0;                      // size of individual files
        int origsize;                   // size of file before padding
        int total;                              // used to write data, not padding to file
        struct stat status;             // used to find size of individual files
        int numerased;                  // number of erased files

        /* Used to recreate file names */
        char *temp;
        char *cs1, *cs2, *extension;
        char *fname;
        int md;
        const char *curdir;
        boost::filesystem::path Path(file_name);
        boost::filesystem::path Parent = Path.parent_path();
        curdir = Parent.c_str();

        matrix = NULL;
        bitmatrix = NULL;
	dprintf(D_ALWAYS, "In jerasureDecoder 1\n");//##

        /* Begin recreation of file names */
        cs1 = (char*)malloc(sizeof(char)*strlen(file_name));
	char *file_dup = strdup(file_name);
        cs2 = strrchr(file_dup, '/');
        if (cs2 != NULL) {
                cs2++;
                strcpy(cs1, cs2);
        }
        else {
                strcpy(cs1, file_dup);
        }
        cs2 = strchr(cs1, '.');
        if (cs2 != NULL) {
                extension = strdup(cs2);
                *cs2 = '\0';
        } else {
           extension = strdup("");
        }
        fname = (char *)malloc(sizeof(char*)*(100+strlen(file_name)+20));
	dprintf(D_ALWAYS, "In jerasureDecoder cs1=%s,cs2=%s\n",cs1,cs2);//##

        /* Read in parameters from metadata file */
        sprintf(fname, "%s/Coding/%s_meta.txt", curdir, cs1);
	dprintf(D_ALWAYS, "In jerasureDecoder, fname=%s\n", fname);//##
        fp = fopen(fname, "rb");
        if (fp == NULL) {
          fprintf(stderr, "Error: no metadata file %s\n", fname);
          exit(1);
        }
        temp = (char *)malloc(sizeof(char)*(strlen(file_name)+20));
        if (fscanf(fp, "%s", temp) != 1) {
                fprintf(stderr, "Metadata file - bad format\n");
                exit(0);
        }

        if (fscanf(fp, "%d", &origsize) != 1) {
                fprintf(stderr, "Original size is not valid\n");
                exit(0);
        }
        if (fscanf(fp, "%d %d %d %d %d", &k, &m, &w, &packetsize, &buffersize) != 5) {
                fprintf(stderr, "Parameters are not correct\n");
                exit(0);
        }
        c_tech = (char *)malloc(sizeof(char)*(strlen(file_name)+20));
        if (fscanf(fp, "%s", c_tech) != 1) {
                fprintf(stderr, "Metadata file - bad format\n");
                exit(0);
        }
        if (fscanf(fp, "%d", &tech) != 1) {
                fprintf(stderr, "Metadata file - bad format\n");
                exit(0);
        }
        method = (Coding_Technique)tech;
        if (fscanf(fp, "%d", &readins) != 1) {
                fprintf(stderr, "Metadata file - bad format\n");
                exit(0);
        }
        fclose(fp);
	dprintf(D_ALWAYS, "In jerasureDecoder, k=%d, m=%d, packetsize=%d, buffersize=%d, w=%d\n", k, m, packetsize, buffersize, w);//##

        /* Allocate memory */
        erased = (int *)malloc(sizeof(int)*(k+m));
        for (i = 0; i < k+m; i++)
                erased[i] = 0;
        erasures = (int *)malloc(sizeof(int)*(k+m));

        data = (char **)malloc(sizeof(char *)*k);
        coding = (char **)malloc(sizeof(char *)*m);
        if (buffersize != origsize) {
                for (i = 0; i < k; i++) {
                        data[i] = (char *)malloc(sizeof(char)*(buffersize/k));
                }
                for (i = 0; i < m; i++) {
                        coding[i] = (char *)malloc(sizeof(char)*(buffersize/k));
                }
                blocksize = buffersize/k;
        }

        sprintf(temp, "%d", k);
        md = strlen(temp);

        /* Create coding matrix or bitmatrix */
        switch(tech) {
                case No_Coding:
                        break;
                case Reed_Sol_Van:
                        matrix = reed_sol_vandermonde_coding_matrix(k, m, w);
                        break;
                case Reed_Sol_R6_Op:
                        matrix = reed_sol_r6_coding_matrix(k, w);
                        break;
                case Cauchy_Orig:
                        matrix = cauchy_original_coding_matrix(k, m, w);
                        bitmatrix = jerasure_matrix_to_bitmatrix(k, m, w, matrix);
                        break;
                case Cauchy_Good:
                        matrix = cauchy_good_general_coding_matrix(k, m, w);
                        bitmatrix = jerasure_matrix_to_bitmatrix(k, m, w, matrix);
                        break;
                case Liberation:
                        bitmatrix = liberation_coding_bitmatrix(k, w);
                        break;
                case Blaum_Roth:
                        bitmatrix = blaum_roth_coding_bitmatrix(k, w);
                        break;
                case Liber8tion:
                        bitmatrix = liber8tion_coding_bitmatrix(k);
        }

        /* Begin decoding process */
        total = 0;
        n = 1;
	int read_size;
        while (n <= readins) {
                numerased = 0;
                /* Open files, check for erasures, read in data/coding */
                for (i = 1; i <= k; i++) {
                        sprintf(fname, "%s/Coding/%s_k%0*d%s", curdir, cs1, md, i, extension);
			dprintf(D_ALWAYS, "In jerasureDecoder, and decoding fname=%s\n", fname);//##
                        fp = fopen(fname, "rb");
                        if (fp == NULL) {
				dprintf(D_ALWAYS, "Missing fname=%s\n", fname);//##
                                erased[i-1] = 1;
                                erasures[numerased] = i-1;
                                numerased++;
                        }
                        else {
                                if (buffersize == origsize) {
                                        stat(fname, &status);
                                        blocksize = status.st_size;
                                        data[i-1] = (char *)malloc(sizeof(char)*blocksize);
                                        read_size = jfread(data[i-1], sizeof(char), blocksize, fp);
					if (read_size != blocksize) {
						dprintf(D_ALWAYS, "in if, jfread error!\n");
						return 1;
					}
                                }
                                else {
                                        fseek(fp, blocksize*(n-1), SEEK_SET);
                                        read_size = jfread(data[i-1], sizeof(char), buffersize/k, fp);
					if (read_size != blocksize/k) {
						dprintf(D_ALWAYS, "in else, jfread error!\n");
						return 1;
					}
                                }
                                fclose(fp);
                        }
                }
                for (i = 1; i <= m; i++) {
                        sprintf(fname, "%s/Coding/%s_m%0*d%s", curdir, cs1, md, i, extension);
			dprintf(D_ALWAYS, "In jerasureDecoder, and decoding fname=%s\n", fname);//##
                        fp = fopen(fname, "rb");
                        if (fp == NULL) {
				dprintf(D_ALWAYS, "Missing fname=%s\n", fname);//##
                                erased[k+(i-1)] = 1;
                                erasures[numerased] = k+i-1;
                                numerased++;
                        }
                        else {
                                if (buffersize == origsize) {
                                        stat(fname, &status);
                                        blocksize = status.st_size;
                                        coding[i-1] = (char *)malloc(sizeof(char)*blocksize);
                                        read_size = jfread(coding[i-1], sizeof(char), blocksize, fp);
                                        if (read_size != blocksize) {
                                                dprintf(D_ALWAYS, "in if, jfread error!\n");
                                                return 1;
                                        }
                                }
                                else {
                                        fseek(fp, blocksize*(n-1), SEEK_SET);
                                        read_size = jfread(coding[i-1], sizeof(char), blocksize, fp);
                                        if (read_size != blocksize) {
                                                dprintf(D_ALWAYS, "in else, jfread error!\n");
                                                return 1;
                                        }
                                }
                                fclose(fp);
                        }
                }
                /* Finish allocating data/coding if needed */
                if (n == 1) {
                        for (i = 0; i < numerased; i++) {
                                if (erasures[i] < k) {
                                        data[erasures[i]] = (char *)malloc(sizeof(char)*blocksize);
                                }
                                else {
                                        coding[erasures[i]-k] = (char *)malloc(sizeof(char)*blocksize);
                                }
                        }
                }

                erasures[numerased] = -1;

                dprintf(D_ALWAYS, "Before Decoding\n");//##
                int print_index = 0;//##
                for(print_index = 0; print_index < k; ++print_index){
                        dprintf(D_ALWAYS, "\n\ndata block k=%d\n",print_index);//##
                        print_datablock(data[print_index],blocksize);//##
                }
                for(print_index = 0; print_index < m; ++print_index){
                        dprintf(D_ALWAYS, "\n\ncoding block m=%d\n",print_index);//##
                        print_datablock(coding[print_index],blocksize);//##
                }

                /* Choose proper decoding method */
                if (tech == Reed_Sol_Van || tech == Reed_Sol_R6_Op) {
			dprintf(D_ALWAYS, "we are using jerasure_matrix_decode to decode the file\n");//##
                        i = jerasure_matrix_decode(k, m, w, matrix, 1, erasures, data, coding, blocksize);
                }
                else if (tech == Cauchy_Orig || tech == Cauchy_Good || tech == Liberation || tech == Blaum_Roth || tech == Liber8tion) {
                        i = jerasure_schedule_decode_lazy(k, m, w, bitmatrix, erasures, data, coding, blocksize, packetsize, 1);
                }
                else {
                        fprintf(stderr, "Not a valid coding technique.\n");
                        exit(0);
                }

                dprintf(D_ALWAYS, "After Decoding\n");//##
                for(print_index = 0; print_index < k; ++print_index){
                        dprintf(D_ALWAYS, "\n\ndata block k=%d\n",print_index);//##
                        print_datablock(data[print_index],blocksize);//##
                }
                for(print_index = 0; print_index < m; ++print_index){
                        dprintf(D_ALWAYS, "\n\ncoding block m=%d\n",print_index);//##
                        print_datablock(coding[print_index],blocksize);//##
                }

                /* Exit if decoding was unsuccessful */
                if (i == -1) {
                        fprintf(stderr, "Unsuccessful!\n");
                        exit(0);
                }

                /* Create decoded file */
                sprintf(fname, "%s/Coding/%s_decoded%s", curdir, cs1, extension);
                if (n == 1) {
                        fp = fopen(fname, "wb");
                }
                else {
                        fp = fopen(fname, "ab");
                }
                for (i = 0; i < k; i++) {
                        if (total+blocksize <= origsize) {
                                fwrite(data[i], sizeof(char), blocksize, fp);
                                total+= blocksize;
                        }
                        else {
                                for (j = 0; j < blocksize; j++) {
                                        if (total < origsize) {
                                                fprintf(fp, "%c", data[i][j]);
                                                total++;
                                        }
                                        else {
                                                break;
                                        }

                                }
                        }
                }
                n++;
                fclose(fp);
        }

        /* Free allocated memory */
        free(cs1);
        free(extension);
        free(fname);
        free(data);
        free(coding);
        free(erasures);
        free(erased);

        return 0;
}

/* is_prime returns 1 if number if prime, 0 if not prime */
static int is_prime(int w) {
	int prime55[] = {2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,
		73,79,83,89,97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,
		181,191,193,197,199,211,223,227,229,233,239,241,251,257};
	int i;
	for (i = 0; i < 55; i++) {
		if (w%prime55[i] == 0) {
			if (w == prime55[i]) return 1;
			else { return 0; }
		}
	}
	assert(0);
	return 0;
}
