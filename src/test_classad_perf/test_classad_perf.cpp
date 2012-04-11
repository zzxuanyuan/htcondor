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

	LARGE_INTEGER sTime;
	LARGE_INTEGER eTime;
	LARGE_INTEGER pdTime;
	LARGE_INTEGER sdTime;
	LARGE_INTEGER perfFreq;

	int isEOF = 0;
	int error = 0;
	int empty = 0;

	QueryPerformanceFrequency(&perfFreq);

	std::cout << "Performance frequency: " << perfFreq.QuadPart << std::endl;

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

//	while(!isEOF && !error && !empty)
//	{
		classad = new compat_classad::ClassAd(jobsFile, "\n", isEOF, error, empty);
		jobAds.push_back(classad);
//	}

	compat_classad::ClassAd *classad2 = new compat_classad::ClassAd(*classad);
	jobAds.push_back(classad2);

	fclose(jobsFile);

	//FoundMatches(classad, machineAds, matches);

	isEOF = 0;
	error = 0;
	empty = 0;
	for(int index = 0; index < 4; index++)
	{
		classad = new compat_classad::ClassAd(machsFile, "\n", isEOF, error, empty);
		machineAds.push_back(classad);
	}

	FoundMatches(classad, machineAds, matches);

	//startTime = GetTickCount();
	QueryPerformanceCounter(&sTime);
	FoundMatches(classad, machineAds, matches);
	QueryPerformanceCounter(&eTime);
	//endTime = GetTickCount();
	//diffTime = endTime - startTime;
	pdTime.QuadPart = eTime.QuadPart - sTime.QuadPart;

	std::cout << "Parallel matching against 4 machines: " << pdTime.QuadPart << std::endl;

	//startTime = GetTickCount();
	std::vector<compat_classad::ClassAd *>::iterator machItr;
	QueryPerformanceCounter(&sTime);
	for(machItr = machineAds.begin(); machItr < machineAds.end(); machItr++)
	{
		IsAMatch(classad, (*machItr));
	}
	//endTime = GetTickCount();
	QueryPerformanceCounter(&eTime);
	sdTime.QuadPart = eTime.QuadPart - sTime.QuadPart;
	matches.clear();
	//diffTime = endTime - startTime;
	std::cout << "Sequential matching against 4 machines: " << sdTime.QuadPart << std::endl;
	std::cout << "Diff between two: " << sdTime.QuadPart - pdTime.QuadPart << endl;

	//while(!isEOF && !error && !empty)
	int machCount = 4;
	while(machCount < 1024)
	{
		for(int index = 0; index < machCount; index++)
		{
			classad = new compat_classad::ClassAd(machsFile, "\n", isEOF, error, empty);
			machineAds.push_back(classad);
		}

		machCount *= 2;

		//startTime = GetTickCount();
		QueryPerformanceCounter(&sTime);
		FoundMatches(classad, machineAds, matches);
		//endTime = GetTickCount();
		//diffTime = endTime - startTime;
		QueryPerformanceCounter(&eTime);
		pdTime.QuadPart = eTime.QuadPart - sTime.QuadPart;

		std::cout << "Parallel matching against " << machCount << " machines: " << pdTime.QuadPart << std::endl;

		//startTime = GetTickCount();
		QueryPerformanceCounter(&sTime);
		std::vector<compat_classad::ClassAd *>::iterator machItr;
		for(machItr = machineAds.begin(); machItr < machineAds.end(); machItr++)
		{
			IsAMatch(classad, (*machItr));
		}
		//endTime = GetTickCount();
		//diffTime = endTime - startTime;
		QueryPerformanceCounter(&eTime);
		sdTime.QuadPart = eTime.QuadPart - sTime.QuadPart;
		std::cout << "Sequential matching against " << machCount << " machines: " << sdTime.QuadPart << std::endl;
		std::cout << "Diff between two: " << sdTime.QuadPart - pdTime.QuadPart << endl;
		matches.clear();
	}

	for(int a = 0; a < 10; a++)
	{
		machCount *= 2;
		machineAds.insert(machineAds.end(), machineAds.begin(), machineAds.end());

		//startTime = GetTickCount();
		QueryPerformanceCounter(&sTime);
		FoundMatches(classad, machineAds, matches);
		//endTime = GetTickCount();
		//diffTime = endTime - startTime;
		QueryPerformanceCounter(&eTime);
		pdTime.QuadPart = eTime.QuadPart - sTime.QuadPart;

		std::cout << "Parallel matching against " << machCount << " machines: " << pdTime.QuadPart << std::endl;

		//startTime = GetTickCount();
		QueryPerformanceCounter(&sTime);
		std::vector<compat_classad::ClassAd *>::iterator machItr;
		for(machItr = machineAds.begin(); machItr < machineAds.end(); machItr++)
		{
			IsAMatch(classad, (*machItr));
		}
		//endTime = GetTickCount();
		//diffTime = endTime - startTime;
		QueryPerformanceCounter(&eTime);
		sdTime.QuadPart = eTime.QuadPart - sTime.QuadPart;
		std::cout << "Sequential matching against " << machCount << " machines: " << sdTime.QuadPart << std::endl;
		std::cout << "Diff between two: " << sdTime.QuadPart - pdTime.QuadPart << endl;
		matches.clear();
	}
	/*
	
	std::vector<compat_classad::ClassAd *>::iterator jobItr;

	std::cout << "Attempting to match " << jobAds.size() << " jobs with " << machineAds.size() << " machines.\n";

/*	for(jobItr = jobAds.begin(); jobItr < jobAds.end(); jobItr++)
	{
		std::vector<compat_classad::ClassAd *>::iterator machItr;
		startTime = GetTickCount();//64();
		bool found = FoundMatches((*jobItr), machineAds, matches);
		endTime = GetTickCount();
		diffTime = endTime - startTime;
		std::cout << "Time parallel match took: " << diffTime << std::endl;
		std::cout << "Parallel match found: " << found << std::endl;
		startTime = GetTickCount();
		bool matched = false;
		for(machItr = machineAds.begin(); machItr < machineAds.end(); machItr++)
		{
			matched |= IsAMatch((*jobItr), (*machItr));
		}
		endTime = GetTickCount();
		diffTime = endTime - startTime;
		std::cout << "Time sequential match took: " << diffTime << std::endl;
		std::cout << "Sequential match found: " << matched << std::endl;
		matches.clear();
	}
*/
	int temp;

	std::cin >> temp;
	return 0;
}