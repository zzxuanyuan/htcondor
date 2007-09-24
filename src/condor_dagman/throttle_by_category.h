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

#ifndef _THROTTLE_BY_CATEGORY_H
#define _THROTTLE_BY_CATEGORY_H

#include "HashTable.h"

//TEMP -- document -- some fanciness is to avoid duplicating strings; some is to allow either MAXJOBS or NODECATEGORY to come first; HashTable doesn't provide a way to get an index

class ThrottleByCategory {
public:
	static const int	noThrottleSetting = -1;

	struct ThrottleInfo {
		const MyString *_category;
		int				_maxJobs;
		int				_currentJobs;
	};

		//TEMP -- document
	ThrottleByCategory();

		//TEMP -- document
	~ThrottleByCategory();

		//TEMP -- document
	ThrottleInfo *AddCategory( const MyString *category,
				int maxJobs = noThrottleSetting );

		//TEMP -- document
	void	SetThrottle( const MyString *category, int maxJobs );

		//TEMP -- document
	ThrottleInfo *	GetThrottleInfo( const MyString *category );

		//TEMP -- document
	void		PrintThrottles( FILE *fp ) /* const */;

private:
	HashTable<MyString, ThrottleInfo *>	_throttles;
};

#endif	// _THROTTLE_BY_CATEGORY_H
