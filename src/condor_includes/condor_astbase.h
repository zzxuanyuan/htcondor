#ifndef _ASTBASE_H_
#define _ASTBASE_H_

#include "condor_exprtype.h"
#include "string_list.h"

//#define USE_STRING_SPACE_IN_CLASSADS

#ifdef USE_STRING_SPACE_IN_CLASSADS
#include "stringSpace.h"
#else
#include "HashTable.h"
#endif

class AttrList;
class EvalResult;

class ExprTree
{
    public :

		virtual int	    	operator ==(ExprTree&);
//		virtual int	    	operator >(ExprTree&);
//		virtual int	    	operator >=(ExprTree&);
//		virtual int	    	operator <(ExprTree&);
//		virtual int	    	operator <=(ExprTree&);

        LexemeType			MyType() { return type; }
		virtual ExprTree*   LArg()   { return NULL; }
		virtual ExprTree*   RArg()   { return NULL; }
		virtual ExprTree*   DeepCopy(void) const = 0;
		virtual void        PrintToNewStr(char **str) {}
		virtual void        PrintToStr(char*) {} // print the expr to a string
		virtual int			CalcPrintToStr( void ) {return 0;}
		int         		EvalTree(const AttrList*, EvalResult*);
		int         		EvalTree(const AttrList*, const AttrList*, EvalResult*);
		char                unit;         // unit of the expression

		ExprTree::ExprTree():unit('\0')
		{
#ifdef USE_STRING_SPACE_IN_CLASSADS
			if (string_space_references == 0) {
				string_space = new StringSpace;
			}
			string_space_references++;
#endif
			return;
		}
		virtual ~ExprTree()
		{
#ifdef USE_STRING_SPACE_IN_CLASSADS
			string_space_references--;
			if (string_space_references == 0) {
				delete string_space;
				string_space = NULL;
			}
#endif
			return;
		}

    protected :
		virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*) = 0;
		LexemeType	    	type;         // lexeme type of the node

#ifdef USE_STRING_SPACE_IN_CLASSADS
		static StringSpace  *string_space;
		static int          string_space_references;
#endif

};

#endif
