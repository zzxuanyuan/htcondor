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
#include "condor_debug.h"
#include "MyString.h"

#include "stream.h"
//#include "list.h"

#define		ATTRLIST_MAX_EXPRESSION		10240

// Ugly hack for the schedd
void AttrList_setPublishServerTime( bool );

template <class Key, class Value> class HashTable; // forward declaration
class YourString;

enum							// various AttrLists
{
    ATTRLISTENTITY,
    ATTRLISTREP
};
enum {AGG_INSERT, AGG_REMOVE};	// operations on aggregate classes

class AttrListElem { };
class OldAttrListElem
{
    public :

        OldAttrListElem(OldExprTree*);			// constructor
        OldAttrListElem(OldAttrListElem&);		// copy constructor
        ~OldAttrListElem() 
			{
				if (tree != NULL) {
				   	delete tree; 
					tree = NULL;
				}
			}

		bool IsDirty(void)            { return dirty;              }
		void SetDirty(bool new_dirty) { dirty = new_dirty; return; }

        friend class OldAttrList;
        friend class OldClassAd;
        friend class OldAttrListList;
  
    private :

        OldExprTree*		tree;	// the tree pointed to by this element
	    bool			dirty;	// has this element been changed?
		char*			name;	// the name of the tree
        class OldAttrListElem*	next;	// next element in the list
};

// An abstract pair returned by unchain.
struct ChainedPair {
	OldAttrListElem **exprList;
	HashTable<YourString, OldAttrListElem *> *exprHash;
};

class AttrListAbstract { };
class OldAttrListAbstract
{
    public :

		int		Type() { return type; }		// type of the OldAttrList

		friend	class		OldAttrList;
		friend	class		OldAttrListList;
		friend	class		OldClassAd;
		friend	class		OldClassAdList;

    protected :

		OldAttrListAbstract(int);
		virtual ~OldAttrListAbstract() {}

		int					type;		// type of this OldAttrList
		class OldAttrListList*	inList;		// I'm in this OldAttrList list
		class OldAttrListAbstract*	next;		// next OldAttrList in the list
		class OldAttrListAbstract*	prev;		// previous OldAttrList in the list
};

class AttrListRep: public AttrListAbstract { };
class OldAttrListRep: public OldAttrListAbstract
{
    public:

        OldAttrListRep(OldAttrList*, OldAttrListList*);	// constructor

		const OldAttrList* GetOrigOldAttrList() { return attrList; }

		friend	class		OldAttrList;
		friend	class		OldAttrListList;

    private:

        OldAttrList*		attrList;		// the original OldAttrList 
        OldAttrListRep*	nextRep;		// next copy of original OldAttrList 
};

class AttrList : public AttrListAbstract { };
class OldAttrList : public OldAttrListAbstract
{
    public :
	    void ChainToAd( OldAttrList * );
		ChainedPair unchain( void );
		void RestoreChain(const ChainedPair &p);
		void ChainCollapse(bool with_deep_copy = true);

		// ctors/dtor
		OldAttrList();							// No associated OldAttrList list
        OldAttrList(OldAttrListList*);			// Associated with OldAttrList list
        OldAttrList(FILE*,char*,int&,int&,int&);// Constructor, read from file.
//		OldAttrList(class ProcObj*);			// create from a proc object
//		OldAttrList(CONTEXT*);					// create from a CONTEXT
        OldAttrList(const char *, char);		// Constructor, from string.
        OldAttrList(OldAttrList&);				// copy constructor
        virtual ~OldAttrList();				// destructor

		OldAttrList& operator=(const OldAttrList& other);

		// insert expressions into the ad
        int        	Insert(const char*, 
							bool check_for_dups=true);	// insert at the tail

        int        	Insert(OldExprTree*, 
							bool check_for_dups=true);	// insert at the tail

		int			InsertOrUpdate(const char *expr) { return Insert(expr); }

		// The Assign() functions are equivalent to Insert("variable = value"),
		// with string values getting wrapped in quotes.  Strings that happen
		// to contain quotes are handled correctly, as an added plus.
		// AssignExpr() is equivalent to Insert("variable = value") without
		// the value being wrapped in quotes.
		int AssignExpr(char const *variable,char const *value);
		int Assign(char const *variable, MyString const &value);
		int Assign(char const *variable,char const *value);
		int Assign(char const *variable,int value);
		int Assign(char const *variable,unsigned int value);
		int Assign(char const *variable,long value);
		int Assign(char const *variable,unsigned long value);
		int Assign(char const *variable,float value);
		int Assign(char const *variable,double value);
		int Assign(char const *variable,bool value);

			// Copy value of source_attr in source_ad to target_attr
			// in this ad.  If source_ad is NULL, it defaults to this ad.
			// If source_attr is undefined, target_attr is deleted, if
			// it exists.
		void CopyAttribute(char const *target_attr, char const *source_attr, OldAttrList *source_ad=NULL );

			// Copy value of source_attr in source_ad to an attribute
			// of the same name in this ad.  Shortcut for
			// CopyAttribute(target_attr,target_attr,source_ad).
		void CopyAttribute(char const *target_attr, OldAttrList *source_ad );

			// Escape double quotes in a value so that it can be
			// safely turned into a ClassAd string by putting double
			// quotes around it.  This function does _not_ add the
			// surrounding double quotes.
			// Returns the escaped string.
		static char const * EscapeStringValue(char const *val,MyString &buf);

		// Make an expression invisible when serializing the ClassAd.
		// This (hopefully temporary hack) is to prevent the special
		// attributes MyType and MyTargetType from being printed
		// multiple times, once from the hard-coded value, and once
		// from the attrlist itself.
		// Returns the old invisibility state.  If the attribute doesn't
		// exist, it returns the new invisibility state specified by
		// the caller, so a caller wishing to restore state can
		// optimize away the second call if desired, since this
		// function does nothing when the attribute does not exist.
		bool SetInvisible(char const *name,bool make_invisible=true);

		// Returns invisibility state of the specified attribute.
		// (Always returns false if the attribute does not exist.)
		bool GetInvisible(char const *name);

		// Returns true if given attribute is generally known to
		// contain data which is private to the user.  Currently,
		// this is just a hard-coded list global list of attribute
		// names.
		static bool ClassAdAttributeIsPrivate( char const *name );

		// Calls SetInvisible() for each attribute that is generally
		// known to contain private data.  Invisible attributes are
		// excluded when serializing the ClassAd.
		void SetPrivateAttributesInvisible(bool make_invisible);

		// deletion of expressions	
        int			Delete(const char*); 	// delete the expr with the name

		// Set or clear the dirty flag for each expression.
		void        SetDirtyFlag(const char *name, bool dirty);
		void        GetDirtyFlag(const char *name, bool *exists, bool *dirty);
		void		ClearAllDirtyFlags();
#if 0
		// to update expression trees
		int			UpdateExpr(char*, OldExprTree*);	// update an expression
		int			UpdateExpr(OldExprTree*);
#endif

		// for iteration through expressions
		void		ResetExpr() { this->ptrExpr = exprList; this->ptrExprInChain = false; }
		OldExprTree*	NextExpr();					// next unvisited expression
		OldExprTree*   NextDirtyExpr();

		// for iteration through names (i.e., lhs of the expressions)
		void		ResetName() { this->ptrName = exprList; this->ptrNameInChain = false; }
		char*		NextName();					// next unvisited name
		const char* NextNameOriginal();
		char*       NextDirtyName();

		// lookup values in classads  (for simple assignments)
		OldExprTree*   Lookup(char *) const;  		// for convenience
        OldExprTree*	Lookup(const char*) const;	// look up an expression
		OldExprTree*	Lookup(const OldExprTree*) const;
		OldAttrListElem *LookupElem(const char *name) const;
		int         LookupString(const char *, char *) const; 
		int         LookupString(const char *, char *, int) const; //uses strncpy
		int         LookupString (const char *name, char **value) const;
		int         LookupString (const char *name, MyString & value) const;
		int         LookupTime(const char *name, char **value) const;
		int         LookupTime(const char *name, struct tm *time, bool *is_utc) const;
        int         LookupInteger(const char *, int &) const;
        int         LookupFloat(const char *, float &) const;
        int         LookupBool(const char *, int &) const;
        bool        LookupBool(const char *, bool &) const;

		// evaluate values in classads
		int         EvalString (const char *, const class OldAttrList *, char *) const;
        int         EvalString (const char *, const class OldAttrList *, char **value) const;
        int         EvalString (const char *, const class OldAttrList *, MyString & value) const;
		int         EvalInteger (const char *, const class OldAttrList *, int &) const;
		int         EvalFloat (const char *, const class OldAttrList *, float &) const;
		int         EvalBool  (const char *, const class OldAttrList *, int &) const;

		int			IsInList(OldAttrListList*);	// am I in this OldAttrList list?

		// output functions
		int			fPrintExpr(FILE*, char*);	// print an expression
		char*		sPrintExpr(char*, unsigned int, const char*); // print expression to buffer
        virtual int	fPrint(FILE*);				// print the OldAttrList to a file
		int         sPrint(MyString &output);   // put the OldAttrList in a string. 
		void		dPrint( int );				// dprintf to given dprintf level

		// conversion function
//		int         MakeContext (CONTEXT *);    // create a context

        // shipping functions
        int put(Stream& s);
		int initFromStream(Stream& s);

		/*
		 * @param str The newline-delimited string of attribute assignments
		 * @param err_msg Optional buffer for error messages.
		 * @return true on success
		 */
		bool initFromString(char const *str,MyString *err_msg);

		void clear( void );

			// Create a list of all ClassAd attribute references made
			// by the value of the specified attribute.  Note that
			// the attribute itself will not be listed as one of the
			// references--only things that it refers to.
		void GetReferences(const char *attribute, 
						   StringList &internal_references, 
						   StringList &external_references) const;
			// Create a list of all ClassAd attribute references made
			// by the given expression.  Returns false if the expression
			// could not be parsed.
		bool GetExprReferences(const char *expr, 
							   StringList &internal_references, 
							   StringList &external_references) const;
		bool IsExternalReference(const char *name, char **simplified_name) const;

#if defined(USE_XDR)
		int put (XDR *);
		int get (XDR *);
#endif

		friend	class	OldAttrListRep;			// access "next" 
		friend	class	OldAttrListList;			// access "UpdateAgg()"
		friend	class	OldClassAd;

		static bool		IsValidAttrName(const char *);
		static bool		IsValidAttrValue(const char *);

    protected :
	    OldAttrListElem**	chainedAttrs;

		// update an aggregate expression if the OldAttrList list associated with
		// this OldAttrList is changed
      	int				UpdateAgg(OldExprTree*, int);
		// convert a (key, value) pair to an assignment tree. used by the
		// constructor that builds an OldAttrList from a proc structure.
		OldExprTree*		ProcToTree(char*, LexemeType, int, float, char*);
        OldAttrListElem*	exprList;		// my collection of expressions
		OldAttrListList*	associatedList;	// the OldAttrList list I'm associated with
		OldAttrListElem*	tail;			// used by Insert
		OldAttrListElem*	ptrExpr;		// used by NextExpr and NextDirtyExpr
		bool			ptrExprInChain;		// used by NextExpr and NextDirtyExpr
		OldAttrListElem*	ptrName;		// used by NextName and NextDirtyName
		bool			ptrNameInChain;		// used by NextName and NextDirtyName
		int				seq;			// sequence number

		HashTable<YourString, OldAttrListElem *> *hash;
		HashTable<YourString, OldAttrListElem *> *chained_hash;

private:
	bool inside_insert;
};

class AttrListList { };
class OldAttrListList
{
    public:
    
        OldAttrListList();					// constructor
        OldAttrListList(OldAttrListList&);	// copy constructor
        virtual ~OldAttrListList();		// destructor

        void 	  	Open();				// set pointer to the head of the queue
        void 	  	Close();			// set pointer to NULL
        OldAttrList* 	Next();				// return OldAttrList pointed to by "ptr"
        OldExprTree* 	Lookup(const char*, OldAttrList*&);	// look up an expression
      	OldExprTree* 	Lookup(const char*);

      	void 	  	Insert(OldAttrList*);	// insert at the tail of the list
      	int			Delete(OldAttrList*); 	// delete a OldAttrList

      	void  	  	fPrintOldAttrListList(FILE *, bool use_xml = false);// print out the list
      	int 	  	MyLength() { return length; } 	// length of this list
      	OldExprTree* 	BuildAgg(char*, LexemeType);	// build aggregate expr

      	friend	  	class		OldAttrList;
      	friend	  	class		OldClassAd;
  
    protected:

        // update aggregate expressions in associated OldAttrLists
		void				UpdateAgg(OldExprTree*, int);

        OldAttrListAbstract*	head;			// head of the list
        OldAttrListAbstract*	tail;			// tail of the list
        OldAttrListAbstract*	ptr;			// used by NextOldAttrList
        class OldAttrListList*		associatedOldAttrLists;	// associated OldAttrLists
        int					length;			// length of the list
};

#endif
