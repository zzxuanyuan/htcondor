/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#ifndef _OrderedSet_H_
#define _OrderedSet_H_


//----------------------------------------------------------------
// Ordered Set Template class
//----------------------------------------------------------------
// Written by Todd Tannenbaum 10/05
//---------------------------------------------------------------------------
// The elements in the set can be of any  class that
// supports assignment operator (=), the equality operator (==), and 
// the less-than opereator (<).
// Interface summary: Exactly the same as class Set, except that the
//    method Add() will add to the set in order instead of at the beginning.
//---------------------------------------------------------------------------

#include "Set.h"

template <class KeyType>
class OrderedSet : public Set<KeyType>
{
public:
  OrderedSet() {};
  ~OrderedSet() {} ;	// Destructor - frees all the memory
  
  void Add(const KeyType& Key);

protected:
  SetElem<KeyType>* Find(const KeyType& Key);

private:
	SetElem<KeyType>* LastFindCurr;
};


// Add to set in proper order
template <class KeyType> 
void OrderedSet<KeyType>::Add(const KeyType& Key) {

  if (Curr==Head || Head==NULL) {
	  Set<KeyType>::Add(Key);
	  return;
  }

  if (Find(Key)) return;

  // If not there, LastFindCurr and LastFindTail
  // are set properly to prevent us from having to 
  // iterate through the whole list again.
  // Save values for Prev and Curr, call Insert()
  // with the hints we got from our Find, then reset values.

  SetElem<KeyType>* saved_Curr = Curr;

  Curr = LastFindCurr;
  Insert(Key);
  Curr = saved_Curr;
}

// Find the element of a key, taking into account that we can stop
// searching the list once we hit an element that is greater than our key.
template <class KeyType>
SetElem<KeyType>* OrderedSet<KeyType>::Find(const KeyType& Key) {
  SetElem<KeyType>* N=Head;
  LastFindCurr = NULL;
  while(N) {
    if (*(N->Key) == *Key ) break;  // found it
	if ( *(N->Key) < *Key ) {	// short cut since it is ordered		
		N=N->Next;	// try the next elem
	} else {
		LastFindCurr = N;	// save this point in case we insert
		N = NULL;	// this elem > key, so we know it isn't in the list

	}
  }
  return N; // could be NULL if failed to find it
}


#endif
