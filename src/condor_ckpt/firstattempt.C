////////////////////////////////////////////////////////////////////////////////////////////////////////
// FILE:           firstattempt.C<2>
// AUTHOR:  Gregory R. Bronner
// DATE:        Thu Mar 16 15:19:27 2000
// Copyright 1999 Gregory R. Bronner.  All rights reserved.
// DESCRIPTION:  
//
//
//
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#include "condor_common.h"
#include "condor_debug.h"
#include "image.h"
#include "firstattempt.h"
char * firstfile;
char * secondfile;

int main(int argc, char** argv) {
	printf("Usage: xxx <condor crap arguments> -first <filename> -second <filename>\n");
	for (int i=1	; i<argc-1; i++ )	{		
		if (!strcmp(	 argv[i], "-first")	) {
			firstfile=argv[i+1];		
		}			
		else 	if (!strcmp(argv[i], "-second") ) {
			secondfile=argv[i+1];
		}	
	}	
	if (!firstfile ||!secondfile) {
		printf("Incorrect arguments\n");
		return 0;
	}
	firstattempt_main(argc, argv);
}

int firstattempt_main(int argc, char*argv[]) {
	Image firstImage, secondImage;
	
	firstImage.SetFileName(firstfile);
	secondImage.SetFileName(secondfile);
	firstImage.Read();
	secondImage.Read();
	firstImage.Display();
	secondImage.Display();
	int retval= firstImage.Compare(secondImage);
	printf("result of comparison: %d", retval);
	
	return 0;
}
