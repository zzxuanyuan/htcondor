/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
  
#ifndef CONDOR_CRONTAB_H
#define CONDOR_CRONTAB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "dc_service.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "MyString.h"
#include "extArray.h"
#include "RegExer.h"

//
// Attributes for a parameter will be separated by the this character
// 
#define CRONTAB_DELIMITER ","
//
// Wildcard Character
// This needs to be a char and not a string
//
#define CRONTAB_WILDCARD '*'
//
// Range Delimiter
//
#define CRONTAB_RANGE "-"
//
// Step Delimiter
//
#define CRONTAB_STEP "/"
//
// cronos.h defined a STAR value to mean the wildcard
//
#define CRONTAB_CRONOS_STAR -1

//
// Time Ranges
//
#define CRONTAB_MINUTE_MIN			0
#define CRONTAB_MINUTE_MAX			59
#define CRONTAB_HOUR_MIN			0
#define CRONTAB_HOUR_MAX			23
#define CRONTAB_DAY_OF_MONTH_MIN	1
#define CRONTAB_DAY_OF_MONTH_MAX	31
#define CRONTAB_MONTH_MIN			1
#define CRONTAB_MONTH_MAX			12
#define CRONTAB_DAY_OF_WEEK_MIN	0	// Note that Sunday is either 0 or 7
#define CRONTAB_DAY_OF_WEEK_MAX	7

//
// Data Field Indices
//
#define CRONTAB_MINUTES_IDX		0
#define CRONTAB_HOURS_IDX		1
#define CRONTAB_DOM_IDX			2
#define CRONTAB_MONTHS_IDX		3
#define CRONTAB_DOW_IDX			4
#define CRONTAB_YEARS_IDX		5 // NOT USED IN CRON CALCULATIONS!!
//
// Number of crontab elements - cleaner loop code!
// Note that we don't care about the year. But we will
// stuff the current year in the current time for convience
//
#define CRONTAB_FIELDS 			5

//
//
//
class CronTab { // : public Service {
protected:
		//
		// Default Constructor
		//
	CronTab();
		//
		//
		//
	bool matchFields( int*, int*, int, bool useFirst = false );
		//
		// 
		//
	bool contains( ExtArray<int>&, const int&  );
		//
		// Initialization method
		//
	void init();
		//
		// 
		//
	void sort( ExtArray<int>& );
		//
		//
		//
	int daysInMonth( int, int );

public:
	
	CronTab( ClassAd* );
	CronTab( int, int, int, int, int );
	CronTab( const char*, const char*, const char*, const char*, const char* );
	~CronTab();
	
	long nextRun();
	long nextRun( long );
	
protected:

		//
		// Given a paramter string with a min/max range, this function
		// will fill the queue with all the possible values for the parameter
		// We need to be passed the parameter name for nice debugging messages!
		//
	int expandParameter( MyString&, int, int, ExtArray<int>&, const char* );

protected:
		//
		// The various scheduling properties of the cron definition
		// These will be pulled from the ClassAd
		//
	MyString *parameters[CRONTAB_FIELDS];

		//
		// After we parse the cron schedule we will have ranges
		// for the different properties.
		//
	ExtArray<int> *cron_ranges[CRONTAB_FIELDS];
	
		//
		// Attribute names
		// Merely here for convience
		//
	static const char* attributes[];
	
		//
		// The regular expresion object we will use to make sure 
		// our parameters are in the proper format.
		// The additional variables are needed in case the regex fails
		//
	RegExer *regex;
	MyString regex_pattern;

}; // END CLASS

#endif // CONDOR_CRONTAB_H
