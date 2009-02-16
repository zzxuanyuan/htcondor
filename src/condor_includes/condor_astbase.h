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
class StringList;
template <class Item> class List; // forward declaration

#define USE_STRING_SPACE_IN_CLASSADS

#ifdef USE_STRING_SPACE_IN_CLASSADS
class StringSpace;
#endif

class OldAttrList;
class OldEvalResult;
class MyString;

class ExprTree { };
class OldExprTree
{
    public :

		OldExprTree();
		virtual ~OldExprTree();
		virtual int	    	operator ==(OldExprTree&);
		virtual int	    	operator >(OldExprTree&);
		virtual int	    	operator >=(OldExprTree&);
		virtual int	    	operator <(OldExprTree&);
		virtual int	    	operator <=(OldExprTree&);

        LexemeType			MyType() { return type; }
		virtual OldExprTree*   LArg()   { return NULL; }
		virtual OldExprTree*   RArg()   { return NULL; }
		virtual OldExprTree*   DeepCopy(void) const = 0;
        virtual void        Display();    // display the expression
		virtual int         CalcPrintToStr(void) {return 0;}
		virtual void        PrintToNewStr(char **str);
		virtual void        PrintToStr(char*) {} // print the expr to a string
		virtual void        PrintToStr(MyString &);

		int         		EvalTree(const OldAttrList*, OldEvalResult*);
		int         		EvalTree(const OldAttrList*, const OldAttrList*, OldEvalResult*);
		virtual void        GetReferences(const OldAttrList *base_attrlist,
										  StringList &internal_references,
										  StringList &external_references) const;

		char                unit;         // unit of the expression

		bool                invisible;    // true for MyType, MyTargetType

    protected :
		virtual void        CopyBaseOldExprTree(class OldExprTree *const recipient) const;
		virtual int         _EvalTree(const class OldAttrList*, OldEvalResult*) = 0;
		virtual int         _EvalTree(const OldAttrList*, const OldAttrList*, OldEvalResult*) = 0;

		LexemeType	    	type;         // lexeme type of the node
		bool				evalFlag;	  // to check for circular evaluation

#ifdef USE_STRING_SPACE_IN_CLASSADS
		static StringSpace  *string_space;
		static int          string_space_references;
#endif

};

class VariableBase : public OldExprTree { };
class OldVariableBase : public OldExprTree
{
    public :
  
	  	OldVariableBase(char*);
	    virtual ~OldVariableBase();

		virtual int	    operator ==(OldExprTree&);

		virtual void	Display();
		char*	const	Name() const { return name; }
		virtual void    GetReferences(const OldAttrList *base_attrlist,
									  StringList &internal_references,
									  StringList &external_references) const;

		friend	class	OldExprTree;

    protected :
		virtual int         _EvalTree(const class OldAttrList*, OldEvalResult*) = 0;
		virtual int         _EvalTree(const OldAttrList*, const OldAttrList*, OldEvalResult*) = 0;

#ifdef USE_STRING_SPACE_IN_CLASSADS
        int                 stringSpaceIndex;
#endif 
  		char*               name;
};

class IntegerBase : public OldExprTree
{
    public :

  		IntegerBase(int);

		virtual int	    operator ==(OldExprTree&);
		virtual int	    operator >(OldExprTree&);
		virtual int	    operator >=(OldExprTree&);
		virtual int	    operator <(OldExprTree&);
		virtual int	    operator <=(OldExprTree&);

		virtual void	Display();
		int		    	Value();

    protected :
		virtual int         _EvalTree(const class OldAttrList*, OldEvalResult*) = 0;
		virtual int         _EvalTree(const OldAttrList*, const OldAttrList*, OldEvalResult*) = 0;

  		int	 	    value;
};

class FloatBase : public OldExprTree
{
    public :

  	FloatBase(float);

	virtual int	    operator ==(OldExprTree&);
	virtual int	    operator >(OldExprTree&);
	virtual int	    operator >=(OldExprTree&);
	virtual int	    operator <(OldExprTree&);
	virtual int	    operator <=(OldExprTree&);
	
	virtual void    Display();
	float		    Value();

    protected :
		virtual int         _EvalTree(const class OldAttrList*, OldEvalResult*) = 0;
		virtual int         _EvalTree(const OldAttrList*, const OldAttrList*, OldEvalResult*) = 0;

  	float	 	    value;
};

class StringBase : public OldExprTree
{
    public :

	StringBase(char*);
	virtual ~StringBase();

	virtual int	    operator ==(OldExprTree&);

	virtual void	Display();
	const	char*	Value();

	friend	class	OldExprTree;

    protected :
		virtual int         _EvalTree(const class OldAttrList*, OldEvalResult*) = 0;
		virtual int         _EvalTree(const OldAttrList*, const OldAttrList*, OldEvalResult*) = 0;

#ifdef USE_STRING_SPACE_IN_CLASSADS
        int                 stringSpaceIndex;
#endif 
		char*           value;
};

class ISOTimeBase : public OldExprTree
{
    public :

	ISOTimeBase(char*);
	virtual ~ISOTimeBase();

	virtual int	    operator ==(OldExprTree&);

	virtual void	Display();
	const	char*	Value();

	friend	class	OldExprTree;

    protected :
		virtual int         _EvalTree(const class OldAttrList*, OldEvalResult*) = 0;
		virtual int         _EvalTree(const OldAttrList*, const OldAttrList*, OldEvalResult*) = 0;

#ifdef USE_STRING_SPACE_IN_CLASSADS
        int                 stringSpaceIndex;
#endif 
		char                *time;
};

class BooleanBase : public OldExprTree
{
    public :
	  
  	BooleanBase(int);
	virtual int	    operator ==(OldExprTree&);

	virtual void        Display();
	int                 Value();

    protected :
		virtual int         _EvalTree(const class OldAttrList*, OldEvalResult*) = 0;
		virtual int         _EvalTree(const OldAttrList*, const OldAttrList*, OldEvalResult*) = 0;

   	int                 value;
};


class UndefinedBase : public OldExprTree
{
    public :

  	UndefinedBase();
	virtual void        Display();

    protected :
		virtual int         _EvalTree(const class OldAttrList*, OldEvalResult*) = 0;
		virtual int         _EvalTree(const OldAttrList*, const OldAttrList*, OldEvalResult*) = 0;
};

class ErrorBase : public OldExprTree
{
    public :

  	ErrorBase();

	virtual void        Display();

    protected :
		virtual int         _EvalTree(const class OldAttrList*, OldEvalResult*) = 0;
		virtual int         _EvalTree(const OldAttrList*, const OldAttrList*, OldEvalResult*) = 0;
};

class BinaryOpBase : public OldExprTree
{
    public :

		virtual ~BinaryOpBase();
		virtual int		      operator ==(OldExprTree&);
		
		virtual OldExprTree*     LArg()   { return lArg; }
		virtual OldExprTree*     RArg()   { return rArg; }

		virtual void            GetReferences(const OldAttrList *base_attrlist,
											  StringList &internal_references,
											  StringList &external_references) const;
		friend  class         OldExprTree;
		friend	class	      OldAttrList;
		friend	class	      AggOp;

    protected :
		virtual int         _EvalTree(const OldAttrList*, OldEvalResult*);
		virtual int         _EvalTree(const OldAttrList*, const OldAttrList*, OldEvalResult*);

		OldExprTree* 	      lArg;
        OldExprTree* 	      rArg;
};

class AddOpBase : public BinaryOpBase
{
    public :
  	AddOpBase(OldExprTree*, OldExprTree*);
  	virtual void        Display();
};

class SubOpBase : public BinaryOpBase
{
    public :
  	SubOpBase(OldExprTree*, OldExprTree*);
  	virtual void        Display();
};

class MultOpBase : public BinaryOpBase
{
    public :
  	MultOpBase(OldExprTree*, OldExprTree*);
  	virtual void        Display();
};

class DivOpBase : public BinaryOpBase
{
    public :
  	DivOpBase(OldExprTree*, OldExprTree*);
  	virtual void        Display();
};

class MetaEqOpBase : public BinaryOpBase
{
    public :
  	MetaEqOpBase(OldExprTree*, OldExprTree*);
  	virtual void        Display();
};

class MetaNeqOpBase : public BinaryOpBase
{
    public :
  	MetaNeqOpBase(OldExprTree*, OldExprTree*);
  	virtual void        Display();
};

class EqOpBase : public BinaryOpBase
{
    public :
  	EqOpBase(OldExprTree*, OldExprTree*);
  	virtual void        Display();
};

class NeqOpBase : public BinaryOpBase
{
    public :
  	NeqOpBase(OldExprTree*, OldExprTree*);
  	virtual void        Display();
};

class GtOpBase : public BinaryOpBase
{
    public :
  	GtOpBase(OldExprTree*, OldExprTree*);
  	virtual void        Display();
};

class GeOpBase : public BinaryOpBase
{
    public :
  	GeOpBase(OldExprTree*, OldExprTree*);
  	virtual void        Display();
};

class LtOpBase : public BinaryOpBase
{
    public :
  	LtOpBase(OldExprTree*, OldExprTree*);
  	virtual void        Display();
};

class LeOpBase : public BinaryOpBase
{
    public :
  	LeOpBase(OldExprTree*, OldExprTree*);
  	virtual void        Display();
};

class AndOpBase : public BinaryOpBase
{
    public :
  	AndOpBase(OldExprTree*, OldExprTree*);
  	virtual void        Display();
};

class OrOpBase : public BinaryOpBase
{
    public :
  	OrOpBase(OldExprTree*, OldExprTree*);
  	virtual void        Display();
};

class AssignOpBase : public BinaryOpBase
{
    public :
  	AssignOpBase(OldExprTree*, OldExprTree*);
  	virtual void        Display();
	virtual void        GetReferences(const OldAttrList *base_attlrlist,
									  StringList &internal_references,
									  StringList &external_references) const;
};

class FunctionBase : public OldExprTree
{
    public :

		FunctionBase(char *);
		virtual         ~FunctionBase();
		virtual int	    operator==(OldExprTree&);
		
		virtual void    GetReferences(const OldAttrList *base_attrlist,
									  StringList &internal_references,
									  StringList &external_references) const;
		friend  class   OldExprTree;
		friend	class	OldAttrList;

		void AppendArgument(OldExprTree *argument);
		
		void EvaluateArgument(OldExprTree *arg, const OldAttrList *attrlist1, 
			const OldAttrList *attrlist2, OldEvalResult *result) const;

    protected :

		List<OldExprTree>     *arguments;

#ifdef USE_STRING_SPACE_IN_CLASSADS
        int                 stringSpaceIndex;
#endif 
  		char*               name;
};

#endif
