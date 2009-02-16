/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

// classad.h
//
// Definition of ClassAd classes and ClassAdList class. They are derived from
// AttrList class and AttrListList class respectively.
//

#ifndef _CLASSAD_H
#define _CLASSAD_H

#include "condor_exprtype.h"
#include "condor_ast.h"
#include "condor_attrlist.h"
#include "condor_parser.h"

#define		CLASSAD_MAX_ADTYPE			50

//for the shipping functions -- added by Lei Cao
#include "stream.h"

struct AdType                   // type of a ClassAd.
{
    int		number;             // type number, internal thing.
    char*	name;               // type name.
    
    AdType(const char * = NULL);      // constructor.
    ~AdType();                  // destructor.
};

class ClassAd { };

class OldClassAd : public OldAttrList
{
    public :

		OldClassAd();								// No associated AttrList list
//		ClassAd(ProcObj*);						// create from a proc object
//		ClassAd(const CONTEXT*);				// create from a CONTEXT
        OldClassAd(FILE*,char*,int&,int&,int&);	// Constructor, read from file.
        OldClassAd(char *, char);					// Constructor, from string.
		OldClassAd(const OldClassAd&);				// copy constructor
        virtual ~OldClassAd();						// destructor

		OldClassAd& operator=(const OldClassAd& other);

		// Type operations
        void		SetMyTypeName(const char *); /// my type name set.
        const char*	GetMyTypeName();		// my type name returned.
        void 		SetTargetTypeName(const char *);// target type name set.
        const char*	GetTargetTypeName();	// target type name returned.
        int			GetMyTypeNumber();			// my type number returned.
        int			GetTargetTypeNumber();		// target type number returned.

		// Requirement operations
#if 0
		int			SetRequirements(char *);
		void        SetRequirements(OldExprTree *);
#endif
		OldExprTree	*GetRequirements(void);

		// Ranking operations
#if 0
		int 		SetRankExpr(char *);
		void		SetRankExpr(OldExprTree *);
#endif
		OldExprTree	*GetRankExpr(void);

		// Sequence numbers
		void		SetSequenceNumber(int);
		int			GetSequenceNumber(void);

		// Matching operations
        int			IsAMatch(class OldClassAd*);			  // tests symmetric match
		friend bool operator==(class OldClassAd&,class OldClassAd&);// same as symmetric match
		friend bool operator>=(class OldClassAd&,class OldClassAd&);// lhs satisfies rhs
		friend bool operator<=(class OldClassAd&,class OldClassAd&);// rhs satisifes lhs

        // shipping functions
        int put(Stream& s);
		int initFromStream(Stream& s);

		/*
		 * @param str The newline-delimited string of attribute assignments
		 * @param err_msg Optional buffer for error messages.
		 * @return true on success
		 */
		bool initFromString(char const *str,MyString *err_msg=NULL);

#if defined(USE_XDR)
		// xdr shipping
		int put (XDR *);
		int get (XDR *);
#endif

		// misc
		class OldClassAd*	FindNext();
			// When printing as_XML, the XML headers and footers
			// are NOT added.  This is so you can wrap a whole set
			// of classads in a single header.  
			// Callers will want to use something like the following:
			//
			// #include "condor_xml_classads.h"
			// ClassAdXMLUnparser unparser;
			// MyString out;
			// unparser.AddXMLFileHeader(out);
			// classad.sPrint(out);
			// unparser.AddXMLFileFooter(out);
        virtual int	fPrint(FILE*);				// print the AttrList to a file
        int	fPrintAsXML(FILE* F);
		int         sPrint(MyString &output);   
		int         sPrintAsXML(MyString &output);
		void		dPrint( int );				// dprintf to given dprintf level

		void		clear( void );				// clear out all attributes

		// poor man's update function until ClassAd Update Protocol  --RR
		 void ExchangeExpressions (class OldClassAd *);

    private :

		AdType*		myType;						// my type field.
        AdType*		targetType;					// target type field.
		// (sequence number is stored in attrlist)

		// This function is called to update myType and targetType
		// variables.  It should be called whenever the attributes that
		// these variables are "bound" to may have been updated.
		void updateBoundVariables();
};

typedef int (*SortFunctionType)(OldAttrList*,OldAttrList*,void*);

class ClassAdList { };
class OldClassAdList : public OldAttrListList
{
  public:
	OldClassAdList() : OldAttrListList() {}

	OldClassAd*	Next() { return (OldClassAd*)OldAttrListList::Next(); }
	void		Rewind() { OldAttrListList::Open(); }
	int			Length() { return OldAttrListList::MyLength(); }
	void		Insert(OldClassAd* ca) { OldAttrListList::Insert((OldAttrList*)ca); }
	int			Delete(OldClassAd* ca){return OldAttrListList::Delete((OldAttrList*)ca);}
	OldClassAd*	Lookup(const char* name);

	// User supplied function should define the "<" relation and the list
	// is sorted in ascending order.  User supplied function should
	// return a "1" if relationship is less-than, else 0.
	// NOTE: Sort() is _not_ thread safe!
	void   Sort(SortFunctionType,void* =NULL);

	// Count ads in list that meet constraint.
	int         Count( OldExprTree *constraint );

  private:
	void	Sort(SortFunctionType,void*,OldAttrListAbstract*&);
	static int SortCompare(const void*, const void*);
};

#endif
