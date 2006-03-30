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

#ifndef __CONDOR_XML_CLASSADS_H
#define __CONDOR_XML_CLASSADS_H

#include "condor_classad.h"
#include "MyString.h"

#define WANT_NAMESPACES
#include "classad_distribution.h"

class ClassAdXMLParser
{
 public:
    /** Constructor. Creates an object that can be used to parse ClassAds */
	ClassAdXMLParser();

    /** Destructor. */
	~ClassAdXMLParser();

	/** Parses an XML ClassAd. Only the first ClassAd in the buffer is 
	 *  parsed.
	 * @param buffer The string containing the XML ClassAd.
	 * @return A pointer to a new ClassAd, which must be deleted by the caller.
	 */
	ClassAd *ParseClassAd(const char *buffer);

	/** Parses an XML ClassAd from a file. Only the first ClassAd in the 
	 * buffer is parsed.
	 * @param buffer The string containing the XML ClassAd.
	 * @return A pointer to a new ClassAd, which must be deleted by the caller.
	 */
	ClassAd *ParseClassAd(FILE *file);

 private:
	classad::ClassAdXMLParser parser;
};

class ClassAdXMLUnparser
{
 public:
    /** Constructor. Creates an object that can be used to create XML
	 * representations of ClassAds */
	ClassAdXMLUnparser();

    /** Destructor.  */
	~ClassAdXMLUnparser();

	/** Set compact spacing. Compact spacing 
	 * places an entire ClassAd on one line, while non-compact places
	 * each attribute on a separate line. 
	 * @param use_compact_spacing A boolean indicating if compact spacing is desired. */
	void SetUseCompactSpacing(bool use_compact_spacing);

	/** Set whether or not we output the Type attribute */
	void SetOutputType(bool output_type);

	/** Set whether or not we output the Target attribute */
	void SetOutputTargetType(bool output_target_type);

	/** Adds XML header to buffer. This is the information that should appear 
	 * at the beginning of all ClassAd XML files. 
	 * @param buffer The string to append the header to. */
	void AddXMLFileHeader(MyString &buffer);

	/** Adds XML footer to buffer. This is the information that should appear 
	 * at the end of all ClassAd XML files. 
	 * @param buffer The string to append the footer to. */
	void AddXMLFileFooter(MyString &buffer);

	/** Format a ClassAd in XML. Note that this obeys the previously set
	 * parameters for compact spacing and compact names. 
	 * @param classsad The ClassAd to represent in XML.
	 * @param buffer The string to append the ClassAd to.
	 * @see SetUseCompactNames().
	 * @see SetUseCompactSpacing(). */
	void Unparse(ClassAd *classad, MyString &buffer);

 private:
	classad::ClassAdXMLUnParser unparser;
};

#endif /* __CONDOR_XML_CLASSADS_H */
