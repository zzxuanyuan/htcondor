#define ITERATE 32
#include "condor_common.h"
#include "compat_classad_util.h"
#include "classad_oldnew.h"
#include <vector>
#include <iostream>
#ifdef WIN32
#include <Windows.h>
#else
#include <time.h>
#endif

std::vector<compat_classad::ClassAd *> jobAds;
std::vector<compat_classad::ClassAd *> machineAds;
std::vector<compat_classad::ClassAd *> matches;

int main(int argc, char *argv[])
{
	FILE *jobsFile = NULL;
	FILE *machsFile = NULL;
	compat_classad::ClassAd *classad = NULL;
	std::vector<compat_classad::ClassAd *> emptyList;
#ifdef WIN32
	LARGE_INTEGER startTime;
	LARGE_INTEGER endTime;
	LARGE_INTEGER pdTime;
	LARGE_INTEGER sdTime;
	LARGE_INTEGER perfFreq;
#else
	timespec startTime;
	timespec endTime;
	long diffTime;
#endif

	int isEOF = 0;
	int error = 0;
	int empty = 0;

	machineAds.reserve(ITERATE * 1024);
#ifdef WIN32
	QueryPerformanceFrequency(&perfFreq);
#endif

	jobsFile = fopen("C:\\Users\\ZWabbit\\workspace\\condor\\classad\\CONDOR_SRC\\src\\test_classad_perf\\jobs.txt", "r");
	if(jobsFile == NULL)
	{
		std::cerr << "Failed to open job file.\n";
		return -1;
	}

	for(int index = 0; index < ITERATE; index++)
	{
		machsFile = fopen("C:\\Users\\ZWabbit\\workspace\\condor\\classad\\CONDOR_SRC\\src\\test_classad_perf\\machines.txt", "r");
		if(!machsFile)
		{
			std::cerr << "Failed to open machine file.\n";
			return -1;
		}

		for(int inner = 0; inner < 1024; inner++)
		{
			classad = new compat_classad::ClassAd(machsFile, "\n", isEOF, error, empty);
			machineAds.push_back(classad);
		}

		fclose(machsFile);

		machsFile = NULL;
	}

//	while(!isEOF && !error && !empty)
//	{
		classad = new compat_classad::ClassAd(jobsFile, "\n", isEOF, error, empty);
		jobAds.push_back(classad);
//	}

	fclose(jobsFile);

	/*
	FoundMatches(classad, emptyList, matches);
	matches.clear();
	FoundMatches(classad, machineAds, matches);
	std::cout << "Flush run: " << matches.size() << std::endl;
	matches.clear();
	*/
#ifdef WIN32
	QueryPerformanceCounter(&startTime);
#else
	clock_gettime(CLOCK_REALTIME, &startTime);
#endif
	FoundMatches(classad, machineAds, matches);
#ifdef WIN32
	QueryPerformanceCounter(&endTime);
	pdTime.QuadPart = endTime.QuadPart - startTime.QuadPart;
	std::cout << "Parallel matching against " << machineAds.size() << " machines: " << pdTime.QuadPart << std::endl;
#else
	clock_gettime(CLOCK_REALTIME, &endTime);
	diffTime = endTime.tv_nsec - startTime.tv_nsec;
	std::cout << "Parallel matching against " << machineAds.size() << " machines: " << diffTime << std::endl;
#endif
	std::cout << "Parallel Matches: " << matches.size() << std::endl;
	matches.clear();

	std::vector<compat_classad::ClassAd *>::iterator machItr;
#ifdef WIN32
	QueryPerformanceCounter(&startTime);
#else
	clock_gettime(CLOCK_REALTIME, &startTime);
#endif
	for(machItr = machineAds.begin(); machItr < machineAds.end(); machItr++)
	{
		if(IsAMatch(classad, (*machItr)))
			matches.push_back((*machItr));
	}
#ifdef WIN32
	QueryPerformanceCounter(&endTime);
	pdTime.QuadPart = endTime.QuadPart - startTime.QuadPart;
	std::cout << "Sequential matching against " << machineAds.size() << " machines: " << pdTime.QuadPart << std::endl;
#else
	clock_gettime(CLOCK_REALTIME, &endTime);
	diffTime = endTime.tv_nsec - startTime.tv_nsec;
	std::cout << "Parallel matching against " << machineAds.size() << " machines: " << diffTime << std::endl;
#endif
	std::cout << "Sequential Matches: " << matches.size() << std::endl;

	int temp;

	std::cin >> temp;
	return 0;
}