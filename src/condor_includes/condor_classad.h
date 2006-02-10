#ifndef _CLASSAD_H
#define _CLASSAD_H

#include <fstream.h>

#include "condor_exprtype.h"
#include "condor_ast.h"
#include "condor_attrlist.h"
#include "condor_debug.h"

#define		CLASSAD_MAX_ADTYPE			50

//for the shipping functions -- added by Lei Cao
#include "stream.h"

class ClassAd : public AttrList
{
    public :

		ClassAd();								// No associated AttrList list
        ClassAd(FILE*,char*,int&,int&,int&);	// Constructor, read from file.
        ClassAd(char *, char);					// Constructor, from string.
		ClassAd(const ClassAd&);				// copy constructor
		ClassAd(const classad::ClassAd&);	 	// copy from new ClassAd
        virtual ~ClassAd();						// destructor

		ClassAd& operator=(const ClassAd& other);

		// Type operations
        void		SetMyTypeName(const char *); /// my type name set.
        const char*	GetMyTypeName();		// my type name returned.
        void 		SetTargetTypeName(const char *);// target type name set.
        const char*	GetTargetTypeName();	// target type name returned.

		// Matching operations
		friend bool operator==(class ClassAd&,class ClassAd&);// same as symmetric match
		friend bool operator>=(class ClassAd&,class ClassAd&);// lhs satisfies rhs
		friend bool operator<=(class ClassAd&,class ClassAd&);// rhs satisifes lhs

        // shipping functions
        int put(Stream& s);
		int initFromStream(Stream& s);

		void		dPrint( int level ) {AttrList::dPrint( level );}
		void		clear( void ) {AttrList::clear( );}

		// poor man's update function until ClassAd Update Protocol  --RR
		void ExchangeExpressions (class ClassAd *);

		friend class ClassAdXMLUnparser;
};

typedef int (*SortFunctionType)(AttrList*,AttrList*,void*);

class ClassAdList : public AttrListList
{
  public:
	ClassAdList() : AttrListList() {}

	ClassAd*	Next() { return (ClassAd*)AttrListList::Next(); }
	void		Rewind() { AttrListList::Open(); }
	int			Length() { return AttrListList::MyLength(); }
	void		Insert(ClassAd* ca) { AttrListList::Insert((AttrList*)ca); }
	int			Delete(ClassAd* ca){return AttrListList::Delete((AttrList*)ca);}
//	ClassAd*	Lookup(const char* name);

	// User supplied function should define the "<" relation and the list
	// is sorted in ascending order.  User supplied function should
	// return a "1" if relationship is less-than, else 0.
	// NOTE: Sort() is _not_ thread safe!
	void   Sort(SortFunctionType,void* =NULL);

	// Count ads in list that meet constraint.
	int         Count( ExprTree *constraint );

  private:
	static int SortCompare(const void*, const void*);
};

#endif
