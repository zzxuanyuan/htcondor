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
// astbase.h
//
// Interface definition for the basic Abstract Syntax Tree (AST). There
// is no interface between this module and the classad module. The following
// operators are defined:
//
//     AddOpBase	+	Float and Integer
//     SubOpBase	-	Float and Integer
//     MultOpBase	*	Float and Integer
//     DivOpBase	/	Float and Integer
//     EqOpBase		==	Float, Integer, ClassadBoolean(TRUE/FALSE), and String
//     NeqOpBase	!=	Float, Integer, ClassadBoolean(TRUE/FALSE), and String
//     GtOpBase		>	Float, Integer
//     GeOpBase		>=	Float, Integer
//     LtOpBase		<	Float, Integer
//     LeOpBase		<=	Float, Integer
//     AndOpBase	&&	ClassadBoolean
//     OrOpBase		||	ClassadBoolean
//     AssignOpBase	=	Expression (left hand side must be a variable)
//
// The following methods are difined or inherited by all the classes defined in
// this module:
//
//     LexemeType MyType()
//         Return the lexeme type of the node from which this method is called.
//         The types are defined in "type.h".
//
//     ExprTree* LArg()
//         Return the left argument if the node from which this method is
//         called is a binary operator; NULL otherwise.
//
//     ExprTree* RArg()
//         Return the right argument or NULL.
//
//     void Display()
//         Display the expression on stdout.
//
//     operator ==(ExprTree& tree)
//     operator >(ExprTree& tree)
//     operator >=(ExprTree& tree)
//     operator <(ExprTree& tree)
//     operator <=(ExprTree& tree)
//         The comparison operators.
//
//******************************************************************************

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
