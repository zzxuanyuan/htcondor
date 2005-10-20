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

#include "condor_crontab.h"
#include "condor_common.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "MyString.h"
#include "extArray.h"
#include "RegExer.h"

#include <iostream>
#include "time.h"

const char* CronTab::attributes[] = {	ATTR_CRON_MINUTES,
										ATTR_CRON_HOURS,
										ATTR_CRON_DAYS_OF_MONTH,
										ATTR_CRON_MONTHS,
										ATTR_CRON_DAYS_OF_WEEK
};

//
// Default Constructor
// 
CronTab::CronTab()
{
	//
	// Do nothing!
	//
}

//
//
//
CronTab::CronTab( ClassAd *ad )
{
		//
		// Pull out the different parameters from the ClassAd
		//
	char buffer[512];
	for ( int ctr = 0; ctr < CRONTAB_FIELDS; ctr++ ) {
			//
			// First get out the parameter value
			//
		if ( ad->LookupString( this->attributes[ctr], buffer ) == 0 ) {
			this->parameters[ctr] = new MyString( buffer );
			//
			// The parameter is empty, we'll use the wildcard
			//
		} else {
			this->parameters[ctr] = new MyString( CRONTAB_WILDCARD );
		}
	} // FOR
		//
		// Initialize
		//
	this->init();
}

//
// Provided to add backwards capabilities for cronos.c
//
CronTab::CronTab(	int minutes,
					int hours,
					int days_of_month,
					int months,
					int days_of_week ) {
		//
		// Simply convert everything to strings
		// If the value is STAR, then use the wildcard
		//
	this->parameters[CRONTAB_MINUTES_IDX] = new MyString( minutes == CRONTAB_CRONOS_STAR ? 
															CRONTAB_WILDCARD : 
															minutes );
	this->parameters[CRONTAB_HOURS_IDX] = new MyString( hours == CRONTAB_CRONOS_STAR ? 
															CRONTAB_WILDCARD : 
															hours );
	this->parameters[CRONTAB_DOM_IDX] = new MyString( days_of_month == CRONTAB_CRONOS_STAR ? 
															CRONTAB_WILDCARD : 
															days_of_month );
	this->parameters[CRONTAB_MONTHS_IDX] = new MyString( months == CRONTAB_CRONOS_STAR ? 
															CRONTAB_WILDCARD : 
															months );
	this->parameters[CRONTAB_DOW_IDX] = new MyString( days_of_week == CRONTAB_CRONOS_STAR ? 
															CRONTAB_WILDCARD : 
															days_of_week );
		//
		// Initialize
		//
	this->init();
}
								
//
//
//
CronTab::CronTab(	const char* minutes,
					const char* hours,
					const char* days_of_month,
					const char* months,
					const char* days_of_week ) {
		//
		// Just save into our object
		//
	this->parameters[CRONTAB_MINUTES_IDX]	= new MyString( minutes );
	this->parameters[CRONTAB_HOURS_IDX]	= new MyString( hours );
	this->parameters[CRONTAB_DOM_IDX]		= new MyString( days_of_month );
	this->parameters[CRONTAB_MONTHS_IDX]	= new MyString( months );
	this->parameters[CRONTAB_DOW_IDX]		= new MyString( days_of_week );
	
		//
		// Initialize
		//
	this->init();
}

//
//
//
void
CronTab::init() {
	
		//
		// Setup the regex we will use to validate the cron
		// parameters. Make sure it's valid after we initialize it
		//
	this->regex = new RegExer( "[^\\/0-9"
							   CRONTAB_DELIMITER
							   CRONTAB_RANGE
							   CRONTAB_STEP
							   "\\/*]" );
	if ( this->regex->getErrno() != 0 ) {
		cout << "FAILED: " << this->regex->getStrerror() << endl;
		EXCEPT( "Failed to instantiate CronTab regex" );
	}
	
		//
		// Now run through all the parameters and create the cron schedule
		//
	int mins[]				= { CRONTAB_MINUTE_MIN,
								CRONTAB_HOUR_MIN,
								CRONTAB_DAY_OF_MONTH_MIN,
								CRONTAB_MONTH_MIN,
								CRONTAB_DAY_OF_WEEK_MIN
	};
	int maxs[]				= { CRONTAB_MINUTE_MAX,
								CRONTAB_HOUR_MAX,
								CRONTAB_DAY_OF_MONTH_MAX,
								CRONTAB_MONTH_MAX,
								CRONTAB_DAY_OF_WEEK_MAX
	};
		//
		// For each attribute field, expand out the crontab parameter
		//
	for ( int ctr = 0; ctr < CRONTAB_FIELDS; ctr++ ) {
		cout << this->attributes[ctr] << ": \"" << *this->parameters[ctr] << "\"\n";
		cout << "-------------------------------------\n";
			//
			// Instantiate our queue
			//
		this->cron_ranges[ctr] = new ExtArray<int>();			
			//
			// Call to expand the parameter
			// The function will modify the queue for us
			//
		this->expandParameter(	*this->parameters[ctr],
								mins[ctr],
								maxs[ctr],
								*this->cron_ranges[ctr],
								this->attributes[ctr] );
		cout << "-------------------------------------\n\n";
	} // FOR
	return;
}  

//
//
//
CronTab::~CronTab() {
	// Oh I'm a stud!
}

//
//
//
long
CronTab::nextRun( ) {
		//
		// Call our own method but using the current time
		//
	return ( this->nextRun( (long) time( NULL ) ) );
}

//
//
//
long
CronTab::nextRun( long timestamp ) {
	long runtime = 0;
	struct tm	*tm;
	const time_t _timestamp = (time_t)timestamp;

		//
		// Load up the time information about the timestamp we
		// were given. The logic for matching the next job is fairly 
		// simple. We just go through the fields backwards and find the next
		// match. This assumes that the ranges are sorted, which they 
		// should be
		//
	tm = localtime( &_timestamp );
	int fields[CRONTAB_FIELDS];
	fields[CRONTAB_MINUTES_IDX]	= tm->tm_min;
	fields[CRONTAB_HOURS_IDX]	= tm->tm_hour;
	fields[CRONTAB_DOM_IDX]		= tm->tm_mday;
	fields[CRONTAB_MONTHS_IDX]	= tm->tm_mon + 1;
	fields[CRONTAB_DOW_IDX]		= tm->tm_wday;
	
		//
		// Set the year here
		//
	int match[CRONTAB_FIELDS + 1];
	match[CRONTAB_YEARS_IDX] = tm->tm_year + 1900;
	
	if ( this->matchFields( fields, match, CRONTAB_FIELDS - 2 ) ) {
		cout << "=====================================\n";
		for ( int ctr = 0; ctr < CRONTAB_FIELDS; ctr++ ) {
			cout << this->attributes[ctr] << ": " << match[ctr] << endl;
		} // FOR
		cout << "CronYear: " << match[CRONTAB_YEARS_IDX] << endl;
	} else {
		cout << "FAILED TO FIND MATCH!!!\n";
	}
	return ( runtime );
}

bool
CronTab::matchFields( int *curTime, int *match, int ctr, bool useFirst )
{
	MyString tabs;
	for ( int i = CRONTAB_FIELDS - 2; i > ctr; i-- ) {
		tabs += "   ";
	}
	cout << tabs << this->attributes[ctr] << " [" <<
			(useFirst ? "TRUE" : "FALSE") << "][" <<
			curTime[ctr] << "]:\n";
			
	
		//
		// Whether we need to tell the next level above that they
		// should use the first element in their range. If we were told
		// to then certainly the next level will as well
		//
	bool nextUseFirst = useFirst;
		//
		// First initialize ourself to know that we don't have a match
		//
	match[ctr] = -1;

		//
		// Find the first match for this field
		// If our value isn't in the list, then we'll take the next one
		//
	bool ret = false;
	for ( int rangeIdx = 0, cnt = this->cron_ranges[ctr]->getlast();
		  rangeIdx <= cnt;
		  rangeIdx++ ) {
			//
			// Two ways to make a match:
			//
			//	1) The level below told us that we need to just 
			//	   the first value in our range if we can. 
			//	2) We need to select the first value in our range
			//	   that is greater than or equal to the current
			//	   time value for this field. This is usually
			//	   what we will do on the very first call to
			//	   us. If we fail and return back to the previous
			//	   level, when they call us again they'll ask
			//	   us to just use the first value that we can
			//
		int value = this->cron_ranges[ctr]->getElementAt( rangeIdx );
		cout << value << ", ";
		if ( useFirst || value >= curTime[ctr] ) {
				//
				// If this value is greater than the current time value,
				// ask the next level to useFirst
				//
			if ( value > curTime[ctr] ) nextUseFirst = true;
				//
				// Day of Month Check!
				// If this day doesn't exist in this month for
				// the current year in our search, we have to skip it
				//
			if ( ctr == CRONTAB_DOM_IDX ) {
				int maxDOM = this->daysInMonth( match[CRONTAB_MONTHS_IDX],
												match[CRONTAB_YEARS_IDX] );
				if ( value > maxDOM ) {
					cout << "FAILED DAYS OF MONTH: " << maxDOM << endl;
					continue;
				}
			}
			match[ctr] = value;
				//
				// If we're the last field for the crontab (i.e. minutes),
				// then we should have a valid time now!
				//
			if ( ctr == CRONTAB_MINUTES_IDX ) {
				ret = true;
				break;
				
				//
				// Now that we have a current value for this field, call to the
				// next field level and see if they can find a match using our
				// If we roll back then we'll want the next time around for the
				// next level to just use the first parameter in their range
				// if they can
				//
			} else {
				cout << tabs << value << endl;
				ret = this->matchFields( curTime, match, ctr - 1, nextUseFirst );
				nextUseFirst = true;
				if ( ret ) break;
			}
		}
	} // FOR
		//
		// If the next level up failed, we need to have
		// special handling for months so we can roll over the year
		// While this may seem trivial it's needed so that we 
		// can change the year in the matching for leap years
		// We will call ourself to make a nice little loop!
		//
	if ( !ret && ctr == CRONTAB_MONTHS_IDX ) {
		match[CRONTAB_YEARS_IDX]++;	
		ret = this->matchFields( curTime, match, ctr, true );
	}
	return ( ret );
}

//
//
//
int
CronTab::expandParameter( MyString &param, int min, int max, 
						  ExtArray<int> &list, const char *attribute )
{
		//
		// First make sure there are only valid characters 
		// in the parameter string
		//
	if ( this->regex->match( (char*)param.Value() ) ) {
			//
			// Bums! So let's print an error message and quit
			//
		cout << "CronTab: Invalid parameter value '" << param.Value();
		cout << "' for " << attribute << endl;
		
		dprintf( D_ALWAYS, "CronTab: Invalid parameter value '%s' for %s\n",
															param.Value(),
															attribute );  
		return ( false );
	}
		//
		// Remove any spaces
		//
	param.replaceString(" ", "");
	
		//
		// Now here's the tricky part! We need to expand their parameter
		// out into a range that can be put in array of integers
		// First start by spliting the string by commas
		//
	param.Tokenize();
	const char *_token;
	while ( ( _token = param.GetNextToken( CRONTAB_DELIMITER, true ) ) != NULL ) {
		MyString token( _token );
		int cur_min = min, cur_max = max, cur_step = 1;
		
			// -------------------------------------------------
			// STEP VALUES
			// The step value is independent of whether we have
			// a range, the wildcard, or a single number.
			// -------------------------------------------------
		if ( token.find( CRONTAB_STEP ) > 0 ) {
				//
				// Just look for the step value to replace 
				// the current step value. The other code will
				// handle the rest
				//
			token.Tokenize();
			const char *_temp;
				//
				// Take out the numerator, keep it for later
				//
			const char *_numerator = token.GetNextToken( CRONTAB_STEP, true );
			if ( ( _temp = token.GetNextToken( CRONTAB_STEP, true ) ) != NULL ) {
				MyString stepStr( _temp );
				stepStr.trim();
				cur_step = atoi( stepStr.Value() );
			}
				//
				// Now that we have the denominator, put the numerator back
				// as the token. This makes it easier to parse later on
				//
			token = *new MyString( _numerator );
		} // STEP
		
			// -------------------------------------------------
			// RANGE
			// If it's a range, expand the range but make sure we 
			// don't go above/below our limits
			// Note that the find will ignore the token if the
			// range delimiter is in the first character position
			// -------------------------------------------------
		if ( token.find( CRONTAB_RANGE ) > 0 ) {
				//
				// Split out the integers
				//
			token.Tokenize();
			MyString *_temp;
			int value;
			
				//
				// Min
				//
			_temp = new MyString( token.GetNextToken( CRONTAB_RANGE, true ) );
			_temp->trim();
			value = atoi( _temp->Value() );
			cur_min = ( value >= min ? value : min );
			delete _temp;
				//
				// Max
				//
			_temp = new MyString( token.GetNextToken( CRONTAB_RANGE, true ) );
			_temp->trim();
			value = atoi( _temp->Value() );
			cur_max = ( value <= max ? value : max );
			delete _temp;
			
			// -------------------------------------------------
			// WILDCARD
			// This will select all values for the given range
			// -------------------------------------------------
		} else if ( token.FindChar( CRONTAB_WILDCARD ) >= 0 ) {
				//
				// For this we do nothing since it will just 
				// be the min-max range
				//
			// -------------------------------------------------
			// SINGLE VALUE
			// They just want a single value to be added
			// Note that a single value with a step like "2/3" will
			// work in this code but its meaningless unless its whole number
			// -------------------------------------------------
		} else {
				//
				// Replace the range to be just this value only if it
				// fits in the min/max range we were given
				//
			// Need something
			int value = atoi(token.Value());
			if ( value >= min && value <= max ) {
				cur_min = cur_max = value;
			}
		}
		
			//
			// Fill out the numbers based on the range using
			// the step value
			//
		//cout << "RANGE: " << cur_min << " - " << cur_max << "\n";
		//cout << "STEP:  " << cur_step << "\n";
			
		for ( int ctr = cur_min; ctr <= cur_max; ctr++ ) {
				//
		 		// Make sure this value isn't alreay added and 
		 		// that it falls in our step listing for the range
		 		//
			if ( ( ( ctr % cur_step ) == 0 ) && !this->contains( list, ctr ) ) {
				list.add( ctr );
		 	}
		} // FOR
	} // WHILE
		//
		// Sort! Makes life easier later on
		//
	this->sort( list );
	
		//
		// Debug
		//
	cout << "CRON: " << attribute << endl;
	cout << "\tORIG:   " << param.Value() << endl;
	cout << "\tMIN:    " << min << endl;
	cout << "\tMAX:    " << max << endl;
	cout << "\tPARSED: ";
		
	/*
	dprintf( D_FULLDEBUG, "CRON: %s\n", attribute );
	dprintf( D_FULLDEBUG, "\tORIG: %s\n", param.Value() );
	dprintf( D_FULLDEBUG, "\tMIN:  %d\n", min );
	dprintf( D_FULLDEBUG, "\tMAX:  %d\n", max );
	dprintf( D_FULLDEBUG, "\tPARSED: " );
	*/
	for ( int ctr = 0, cnt = list.getlast(); ctr <= cnt; ctr++ ) {
    	//dprintf( D_FULLDEBUG, "%d, ", list[ctr] );
    	cout << list[ctr] << ", ";
	} // FOR
	cout << "\n\n";
    //dprintf( D_FULLDEBUG, "\n\n" );
	
	return ( true );
}

//
//
//
bool
CronTab::contains( ExtArray<int> &list, const int &elt ) 
{
		//
		// Just run through our array and look for the 
		// the element
		//
	bool ret = false;
	for ( int ctr = 0; ctr <= list.getlast(); ctr++ ) {
			//
			// All we can really do is do a simple comparison
			//
		if ( elt == list[ctr] ) {
			ret = true;
			break;	
		}
	} // FOR
	return ( ret );
}

//
// Ye' Olde Insertion Sort!
//
void
CronTab::sort( ExtArray<int> &list )
{
	int ctr, ctr2, value;
	for ( ctr = 1; ctr <= list.getlast(); ctr++ ) {
		value = list[ctr];
    	ctr2 = ctr;
		while ( ( ctr2 > 0 ) && ( list[ctr2 - 1] > value ) ) {
			list[ctr2] = list[ctr2 - 1];
			ctr2--;
		} // WHILE
		list[ctr2] = value;
	} // FOR
	return;
}

//
// Returns the # of days for a month in a given year
//
int
CronTab::daysInMonth( int month, int year ) 
{
	const unsigned char daysInMonth[13] = {	0, 31, 28, 31, 30, 31,
  											30, 31, 31, 30, 31, 30, 31};
  		//
  		// Check if it's a leap year
  		//
	bool leap = ( ( !(year % 4) && (year % 100) ) || (!(year % 400)) );
	return ( month > 0 && month <= 12 ? 
  				daysInMonth[month] + (month == 2 && leap ) :
  				0 );
}
