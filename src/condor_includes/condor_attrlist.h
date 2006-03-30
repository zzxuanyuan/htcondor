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
//******************************************************************************
// attrlist.h
//
// Definition of AttrList classes and AttrListList class.
//
//******************************************************************************

#ifndef _ATTRLIST_H
#define _ATTRLIST_H

#include "condor_exprtype.h"
#include "condor_astbase.h"
#include "condor_ast.h"
#include "MyString.h"

#include "stream.h"
#include "list.h"

#define		ATTRLIST_MAX_EXPRESSION		10240
#define EXPR_CACHE_SIZE 1024

#define WANT_NAMESPACES
#include "classad_distribution.h"

//typedef classad_hash_map<std::string, AssignOp*, classad::StringCaseIgnHash,
//						 classad::CaseIgnEqStr> ExprCache;
typedef HashTable<std::string, AssignOp*> ExprCache;

int STLStringHashFunc(const std::string &s, int numBuckets);

class AttrListList;

class AttrList
{
    public :
	    void ChainToAd( AttrList * );
		void* unchain( void );
		void RestoreChain(void *);

		// ctors/dtor
		AttrList();							// No associated AttrList list
        AttrList(FILE*,char*,int&,int&,int&);// Constructor, read from file.
        AttrList(char *, char);				// Constructor, from string.
        AttrList(AttrList&);				// copy constructor
        virtual ~AttrList();				// destructor

		AttrList& operator=(const AttrList& other);

		// insert expressions into the ad
        int        	Insert(const char*, 
							bool check_for_dups=true);	// insert at the tail

        int        	Insert(ExprTree*, 
							bool check_for_dups=true);	// insert at the tail

		int			InsertOrUpdate(const char *expr) { return Insert(expr); }

		// The Assign() functions are equivalent to Insert("variable = value"),
		// with string values getting wrapped in quotes.  Strings that happen
		// to contain quotes are handled correctly, as an added plus.
		// AssignExpr() is equivalent to Insert("variable = value") without
		// the value being wrapped in quotes.
		int AssignExpr(char const *variable,char const *value);
		int Assign(char const *variable,char const *value);
		int Assign(char const *variable,int value);
		int Assign(char const *variable,float value);
		int Assign(char const *variable,bool value);

		// deletion of expressions	
        int			Delete(const char*); 	// delete the expr with the name

		// Set or clear the dirty flag for each expression.
		void        SetDirtyFlag(const char *name, bool dirty);
		void        GetDirtyFlag(const char *name, bool *exists, bool *dirty);
		void		ClearAllDirtyFlags();

		// for iteration through expressions
		void		ResetExpr();
		ExprTree*	NextExpr();					// next unvisited expression
		ExprTree*   NextDirtyExpr();

		// for iteration through names (i.e., lhs of the expressions)
		void		ResetName();
		char*		NextName();					// next unvisited name
		const char* NextNameOriginal();
		char*       NextDirtyName();

		// lookup values in classads  (for simple assignments)
		ExprTree   	*Lookup(char *) const;  		// for convenience
        ExprTree	*Lookup(const char*) const;	// look up an expression
		ExprTree	*Lookup(const ExprTree*) const;
		ExprTree   	*LookupElem( const char* name ) const {return Lookup(name);}
		int         LookupString(const char *, char *) const; 
		int         LookupString(const char *, char *, int) const; //uses strncpy
		int         LookupString (const char *name, char **value) const;
		int         LookupString (const char *name, MyString & value) const;
        int         LookupInteger(const char *, int &) const;
        int         LookupFloat(const char *, float &) const;
        int         LookupBool(const char *, int &) const;
        bool        LookupBool(const char *, bool &) const;

		// evaluate values in classads
		int         EvalString (const char *, class AttrList *, char *);
        int         EvalString (const char *, class AttrList *, char **value);
		int         EvalInteger (const char *, class AttrList *, int &);
		int         EvalFloat (const char *, class AttrList *, float &);
		int         EvalBool  (const char *, class AttrList *, int &);

		// output functions
		int			fPrintExpr(FILE*, char*);	// print an expression
		char*		sPrintExpr(char*, unsigned int, const char*); // print expression to buffer
        virtual int	fPrint(FILE*);				// print the AttrList to a file
		int         sPrint(MyString &output);   // put the AttrList in a string. 
		void		dPrint( int );				// dprintf to given dprintf level

        // shipping functions
        int put(Stream& s);
		int initFromStream(Stream& s);
		void clear( void );
		void GetReferences(const char *attribute, 
						   StringList &internal_references, 
						   StringList &external_references) const;
		bool IsExternalReference(const char *name, char **simplified_name) const;

		int			IsInList(AttrListList*);	// am I in this AttrList list?

		friend class AttrListList;
		friend class OtherExpr;

 protected:
		classad::ClassAd *ad;
		class AttrListList *inOneList;
		List<class AttrListList> *inLists;
		classad::ClassAd::iterator *nameItr;
		classad::ClassAd::iterator *exprItr;
		bool nameItrInChain;
		bool exprItrInChain;
		classad::ClassAd::dirtyIterator *dirtyNameItr;
		classad::ClassAd::dirtyIterator *dirtyExprItr;
		ExprCache *exprCache;
		AttrList *chainedAd;

		void Init( );	// initializes ad - used by constructors
		void Copy( const AttrList & );	// copies ad, used by operator=
			                            // and copy constructor
};

class AttrListList
{
    public:
    
        AttrListList();					// constructor
		AttrListList& operator=(const AttrListList& other);
        virtual ~AttrListList();		// destructor

        void 	  	Open();				// set pointer to the head of the queue
        void 	  	Close();			// set pointer to NULL
        AttrList* 	Next();				// return AttrList pointed to by "ptr"

      	void 	  	Insert(AttrList*);	// insert at the tail of the list
      	int			Delete(AttrList*); 	// delete a AttrList
		ExprTree*	Lookup(const char* name, AttrList*& attrList);
		ExprTree*	Lookup(const char* name);

      	void  	  	fPrintAttrListList(FILE *, bool use_xml = false);// print out the list
      	int 	  	MyLength();

    protected:
		List<AttrList> alList;
};

#endif
