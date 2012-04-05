//#define WINVER _WIN32_WINNT_WS0
//#define _WIN32_WINNT _WIN32_WINNT_WS0
#include "condor_common.h"
#include "compat_classad_util.h"
#include "classad_oldnew.h"
//#include <stdio.h>
#include <vector>
#include <iostream>
#include <Windows.h>

std::vector<compat_classad::ClassAd *> jobAds;
std::vector<compat_classad::ClassAd *> machineAds;
std::vector<compat_classad::ClassAd *> matches;

int main(int argc, char *argv[])
{
	FILE *jobsFile = NULL;
	FILE *machsFile = NULL;
	compat_classad::ClassAd *classad = NULL;
	//ULONGLONG startTime = 0;
	//ULONGLONG endTime = 0;
	//ULONGLONG diffTime = 0;
	DWORD startTime = 0;
	DWORD endTime = 0;
	DWORD diffTime = 0;

	int isEOF = 0;
	int error = 0;
	int empty = 0;

	jobsFile = fopen("C:\\Users\\ZWabbit\\workspace\\condor\\classad\\CONDOR_SRC\\src\\test_classad_perf\\jobs.txt", "r");
	if(jobsFile == NULL)
	{
		std::cerr << "Failed to open job file.\n";
		return -1;
	}

	machsFile = fopen("C:\\Users\\ZWabbit\\workspace\\condor\\classad\\CONDOR_SRC\\src\\test_classad_perf\\machines.txt", "r");
	if(!machsFile)
	{
		std::cerr << "Failed to open machine file.\n";
		return -1;
	}

	while(!isEOF && !error && !empty)
	{
		classad = new compat_classad::ClassAd(jobsFile, "\n", isEOF, error, empty);
		jobAds.push_back(classad);
	}

	isEOF = 0;
	error = 0;
	empty = 0;

	while(!isEOF && !error && !empty)
	{
		classad = new compat_classad::ClassAd(machsFile, "\n", isEOF, error, empty);
		machineAds.push_back(classad);
	}

	
	std::vector<compat_classad::ClassAd *>::iterator jobItr;

	std::cout << "Attempting to match " << jobAds.size() << " jobs with " << machineAds.size() << " machines.\n";

	for(jobItr = jobAds.begin(); jobItr < jobAds.end(); jobItr++)
	{
		startTime = GetTickCount();//64();
		bool found = FoundMatches((*jobItr), machineAds, matches);
		endTime = GetTickCount();
		diffTime = endTime - startTime;
		std::cout << "Time match took: " << diffTime;
	}
}