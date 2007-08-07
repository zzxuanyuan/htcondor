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


#ifndef _BINARY_SEARCH_H_
#define _BINARY_SEARCH_H_

template <class ObjType>
class BinarySearch
{
  public:
	/** Binary search.  Array must be sorted lowest-to-highest.
	TEMPTEMP
		@param array The array to search.
		@param length The length of the array.
		@param key The value to search for
		@return If key is found, the index at which it is found;
			if key is not found, TEMPTEMP
	*/
	static int Search(const ObjType array[], int length, ObjType key);
};

template <class ObjType>
int
BinarySearch<ObjType>::Search(const ObjType array[], int length, ObjType key)
{
	int		low = 0;
	int		high = length - 1;

	while ( low <= high ) {
			// Avoid possibility of low + high overflowing.
		int		mid = low + ((high - low) / 2);
		ObjType	midVal = array[mid];

		if ( midVal < key ) {
			low = mid + 1;
		} else if ( midVal > key ) {
			high = mid - 1;
		} else {
			return mid; // key found
		}
	}

	return -(low + 1); // key not found
}

#endif // _BINARY_SEARCH_H_
