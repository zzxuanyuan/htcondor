/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#ifndef __STORK_JOB_ID_H__
#define __STORK_JOB_ID_H__

// Class to manipulate Stork job ids, in all various forms
/* input/output formats:
   string key			for direct access to collection
   classad	jobAd
   const char* string	for log messages
   StorkJobId			copy 

   operators
   increment
   initialize

   */

// Includes
#include "condor_common.h"

// New ClassAds.  Define WANT_NAMESPACES whenever both new and old ClassAds
// coexist in the same file.
#define WANT_NAMESPACES
#include "classad_distribution.h"

// Macros

// Typedefs

/// StorkJobId class
class StorkJobId
{
	public:

	/// Constructors
	StorkJobId( void );
	StorkJobId( const std::string& key );
	StorkJobId( const classad::ClassAd& ad);
	//StorkJobId( const char* str );

	/// Destructor
	~StorkJobId( void );

	// Assignment
	void assign( const std::string& key );
	void assign( const classad::ClassAd& ad);
	//void assign( const char* str );

	/// = operator
	StorkJobId& operator=( const std::string& key );
	StorkJobId& operator=( const classad::ClassAd& ad);
	//StorkJobId& operator=( const char* str );

	// other operators
	StorkJobId operator++()
	{
		_id++;
		if ( _id < 0 ) _id = 0;	// do not allow negative  values
		return *this;
	}
	StorkJobId operator--()
	{
		_id--;
		if ( _id < 0 ) _id = 0;	// do not allow negative  values
		return *this;
	}
	int operator<( const StorkJobId& arg2 ) const { return _id < arg2._id; }
	int operator<=( const StorkJobId& arg2 ) const { return _id <= arg2._id; }
	int operator>( const StorkJobId& arg2 ) const { return _id > arg2._id; }
	int operator>=( const StorkJobId& arg2 ) const { return _id >= arg2._id; }
	int operator==( const StorkJobId& arg2 ) const { return _id == arg2._id; }

	// output conversions
	void updateClassAd( classad::ClassAd& ad ) const;
	std::string key( void );
	const char* fmt( void );
	const char* c_str( void );
	void getTuple(int* cluster, int* proc, int* subproc)
	{ *cluster=_id; *proc=-1; *subproc=-1; }

	// Input error indicator.
	bool error(void) const { return _error; }

	// Input error message.  Only valid if error==false.
	const char* errorMsg(void) const { return _errorMsg.c_str(); }

	private:

	// Private member functions ////////////////////////////////////////////////

	/// Class initializer
	void initialize( void );

	// Private member data /////////////////////////////////////////////////////

	/// Job id
	int _id;

	/// ClassAdCollection key
	std::string _key;

	/// Raw, unformatted job id
	std::string _raw;

	/// error indicator
	bool _error;

	/// last error message
	std::string _errorMsg;

}; // class StorkJobId

#endif//__STORK_JOB_ID_H__
