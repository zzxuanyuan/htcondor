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

#include "condor_common.h"
#include <stdio.h>
#include "binary_search.h"

template class BinarySearch<int>;
template class BinarySearch<float>;

int		debug = 0;
const int maxPrint = 10;

void
PrintArray( int array[], int length )
{
	printf( "{ " );
	if ( length <= maxPrint ) {
		for ( int i = 0; i < length; i++ ) {
			printf( "%d ", array[i] );
		}
	} else {
		printf( "...%d elements... ", length );
	}
	printf( "}" );
}

void
PrintArray( float array[], int length )
{
	printf( "{ " );
	if ( length <= maxPrint ) {
		for ( int i = 0; i < length; i++ ) {
			printf( "%f ", array[i] );
		}
	} else {
		printf( "...%d elements... ", length );
	}
	printf( "}" );
}

int
TestSearch( int array[], int length, int key, int expectedIndex )
{
	int		result = 0;

	int		index = BinarySearch<int>::Search( array,
				length, key );

	if ( debug >= 1 ) {
		printf( "Search for %d in ", key );
		PrintArray( array, length );
		printf( " returned %d\n", index );
	}

	bool	match = index >= 0 && index < length && array[index] == key;
	if ( !match && index != expectedIndex ) {
		fprintf( stderr, "Error: search for %d returned %d; "
					"should have returned %d\n", key, index, expectedIndex );
		printf( "  Array is: " );
		PrintArray( array, length );
		printf( "\n" );
		result = 1;
	}

	return result;
}

int
TestSearch( float array[], int length, float key, int expectedIndex )
{
	int		result = 0;

	int		index = BinarySearch<float>::Search( array,
				length, key );

	if ( debug >= 1 ) {
		printf( "Search for %f in ", key );
		PrintArray( array, length );
		printf( " returned %d\n", index );
	}

	if ( index != expectedIndex ) {
		fprintf( stderr, "Error: search for %f returned %d; "
					"should have returned %d\n", key, index, expectedIndex );
		printf( "  Array is: " );
		PrintArray( array, length );
		printf( "\n" );
		result = 1;
	}

	return result;
}

int test0()
{
	printf( "Testing an integer binary search...\n" );
	int		result = 0;

	int		array[] = { 1, 2, 3 };

	result |= TestSearch( array, sizeof(array)/sizeof(int), 0, -1 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 1, 0 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 2, 1 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 3, 2 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 4, -4 );

	printf( "...%s\n", result == 0 ? "OK" : "Failed" );
	return result;
}

int test1()
{
	printf( "Testing an integer binary search...\n" );
	int		result = 0;

	int		array[] = { 1, 3, 4, 7 };

	result |= TestSearch( array, sizeof(array)/sizeof(int), 0, -1 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 1, 0 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 2, -2 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 3, 1 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 4, 2 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 5, -4 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 6, -4 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 7, 3 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 8, -5 );

	printf( "...%s\n", result == 0 ? "OK" : "Failed" );
	return result;
}

int test2()
{
	printf( "Testing an integer binary search with duplicates...\n" );
	int		result = 0;

	int		array[] = { 1, 3, 3, 3, 7, 7 };

	result |= TestSearch( array, sizeof(array)/sizeof(int), 0, -1 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 1, 0 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 2, -2 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 3, 1 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 4, -5 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 5, -5 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 6, -5 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 7, 4 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 8, -7 );

	printf( "...%s\n", result == 0 ? "OK" : "Failed" );
	return result;
}

int test3()
{
	printf( "Testing a float binary search...\n" );
	int		result = 0;

	float	array[] = { -2349.67, -1005.39, 0.0, 55.72, 55.74, 100.9 };

	result |= TestSearch( array, sizeof(array)/sizeof(float), -3333.0, -1 );
	result |= TestSearch( array, sizeof(array)/sizeof(float), -2349.67, 0 );
	result |= TestSearch( array, sizeof(array)/sizeof(float), -2000.0, -2 );
	result |= TestSearch( array, sizeof(array)/sizeof(float), -1000.0, -3);
	result |= TestSearch( array, sizeof(array)/sizeof(float), 0.0, 2 );
	result |= TestSearch( array, sizeof(array)/sizeof(float), 50.0, -4 );
	result |= TestSearch( array, sizeof(array)/sizeof(float), 55.73, -5 );
	result |= TestSearch( array, sizeof(array)/sizeof(float), 55.75, -6 );
	result |= TestSearch( array, sizeof(array)/sizeof(float), 101.0, -7 );

	printf( "...%s\n", result == 0 ? "OK" : "Failed" );
	return result;
}

int test4()
{
	printf( "Testing a float binary search with an empty array...\n" );
	int		result = 0;

	float	array[] = {};

	result |= TestSearch( array, sizeof(array)/sizeof(float), -3333.0, -1 );

	printf( "...%s\n", result == 0 ? "OK" : "Failed" );
	return result;
}

int test5()
{
	printf( "Testing an integer binary search with a large array...\n" );
	int		result = 0;

	const int	size = 10000000;
	int		*array = new int[size];

	for ( int i = 0; i < size; i++ ) {
		array[i] = i;
	}

	result |= TestSearch( array, size, -1, -1 );
	result |= TestSearch( array, size, 0, 0 );
	result |= TestSearch( array, size, size/2, size/2 );
	result |= TestSearch( array, size, size-1, size-1 );
	result |= TestSearch( array, size, size, -(size+1) );

	printf( "...%s\n", result == 0 ? "OK" : "Failed" );
	return result;
}

int
main(int argc, char *argv[])
{
	int		result = 0;

	printf( "Testing BinarySearch template\n" );

	if ( argc > 1 && !strcmp( argv[1], "-d" ) ) {
		debug = 1;
	}

	result |= test0();
	result |= test1();
	result |= test2();
	result |= test3();
	result |= test4();
	result |= test5();

	if ( result == 0 ) {
		printf( "Test succeeded\n" );
	} else {
		printf( "Test failed\n" );
	}
	return result;
}
