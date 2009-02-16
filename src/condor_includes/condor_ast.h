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
// ast.h
//
// Interface definition for the Abstract Syntax Tree (AST) module. This module
// has an interface to the ClassAd module. It provides the following methods:
//
//     int EvalTree(ClassAdList* list, EvalResult* result)
//     int EvalTree(ClassAd* classad,  EvalResult* result)
//
//         EvalTree() evaluates an expression tree either in a classad or in a
//         classad list. The result of the evaluation and the type of the result
//         is put into "result".
//
//     OldExprTree* MinTree(ClassAdList* list)
//
//         If the result of the expression tree in a classad list is true, a new
//         expression tree which is the "minimum" part of the original
//         expression tree that contributed to the result of the evaluation is
//         returned.
//
//     void Diff(ClassAdList* list, VarTypeTable* table)
//
//         Diff() "subtracts" the resources required by the expression from
//         "list". "table" provides information on whether a variable is a range
//         type variable or a fixed-value type variable.
//
//     void AnalTree(FILE* f, AttrListList* list)
//
//         AnalTree() prints out parts of an expression which can not be
//         satisfied in "list". It also attempts to provide some alternatives
//         for these unsatisfiable parts in "list".
//
//     void SumTree(FILE* f, AttrListList* list)
//
//         SumTree() prints out parts of an expression which can not be
//         satisfied in any of the AttrLists in "list". It also attempts to
//         provide some alternatives for these unsatisfiable parts in the
//         AttrLists in "list". "table" provides statistical information about
//         the AttrLists in "list".
//
//******************************************************************************

#ifndef _AST_H_
#define _AST_H_

#include "condor_exprtype.h"
#include "condor_astbase.h"

////////////////////////////////////////////////////////////////////////////////
// Class EvalResult is passed to OldExprTree::EvalTree() to buffer the result of
// the evaluation. The result can be integer, floating point, string, boolean,
// null or error type. The type field specifies the type of the result.
////////////////////////////////////////////////////////////////////////////////
class EvalResult { };
class OldEvalResult
{
    public :

    OldEvalResult();
  	~OldEvalResult();

		/// copy constructor
	OldEvalResult(const OldEvalResult & copyfrom);
		/// assignment operator
	OldEvalResult & operator=(const OldEvalResult & rhs);

	void fPrintResult(FILE *); // for debugging

   	union
    	{
   	    int i;
   	    float f;
   	    char* s;
        };
  	LexemeType type;

	bool debug;

	private :
	void deepcopy(const OldEvalResult & copyfrom);
};

class Variable : public VariableBase {};
class OldVariable : public OldVariableBase
{
	public :
  
		OldVariable(char*a_name) : OldVariableBase(a_name) {}
		virtual int         CalcPrintToStr(void);
		virtual void        PrintToStr(char*);
		virtual OldExprTree*     DeepCopy(void) const;

	protected:

		virtual int         _EvalTree(const class OldAttrList*, OldEvalResult*);
		virtual int         _EvalTree(const OldAttrList*, const OldAttrList*, OldEvalResult*);
		virtual int         _EvalTreeRecursive(const char *name, const OldAttrList*, const OldAttrList*, OldEvalResult*, bool);
		virtual int         _EvalTreeSimple(const char *name, const OldAttrList*, const OldAttrList*, OldEvalResult*, bool);
};

class Integer : public IntegerBase
{
    public :

  	Integer(int i) : IntegerBase(i) {}

	virtual	int	    operator >(OldExprTree&);
	virtual	int	    operator >=(OldExprTree&);
	virtual	int	    operator <(OldExprTree&);
	virtual	int	    operator <=(OldExprTree&);
 
	virtual int     CalcPrintToStr(void);
    virtual void    PrintToStr(char*);
	virtual OldExprTree*  DeepCopy(void) const;

	protected:

  	virtual int     _EvalTree(const OldAttrList*, OldEvalResult*);
    virtual int     _EvalTree(const OldAttrList*, const OldAttrList*, OldEvalResult*);
};


class Float: public FloatBase
{
    public :

  	Float(float f) : FloatBase(f) {}

	virtual	int	    operator >(OldExprTree&);
	virtual	int	    operator >=(OldExprTree&);
	virtual	int	    operator <(OldExprTree&);
	virtual	int	    operator <=(OldExprTree&);
	virtual int     CalcPrintToStr(void);
    virtual void    PrintToStr(char*);
	virtual OldExprTree*  DeepCopy(void) const;

	protected:

  	virtual int     _EvalTree(const OldAttrList*, OldEvalResult*);
    virtual int     _EvalTree(const OldAttrList*, const OldAttrList*, OldEvalResult*);
};


class String : public StringBase
{
    public :

  	String(char* s) : StringBase(s) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual OldExprTree*     DeepCopy(void) const;

	protected:

  	virtual int         _EvalTree(const OldAttrList*, OldEvalResult*);
    virtual int         _EvalTree(const OldAttrList*, const OldAttrList*, OldEvalResult*);
};


class ISOTime : public ISOTimeBase
{
    public :

  	ISOTime(char* s) : ISOTimeBase(s) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual OldExprTree*     DeepCopy(void) const;

	protected:

  	virtual int         _EvalTree(const OldAttrList*, OldEvalResult*);
    virtual int         _EvalTree(const OldAttrList*, const OldAttrList*, OldEvalResult*);
};

class ClassadBoolean : public BooleanBase
{
    public :

  	ClassadBoolean(int b) : BooleanBase(b) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual OldExprTree*   DeepCopy(void) const;

	protected:

  	virtual int         _EvalTree(const OldAttrList*, OldEvalResult*);
    virtual int         _EvalTree(const OldAttrList*, const OldAttrList*, OldEvalResult*);
};


class Undefined : public UndefinedBase
{
    public :

	Undefined() : UndefinedBase() {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual OldExprTree*   DeepCopy(void) const;

	protected:

  	virtual int         _EvalTree(const OldAttrList*, OldEvalResult*);
    virtual int         _EvalTree(const OldAttrList*, const OldAttrList*, OldEvalResult*);
};

class Error : public ErrorBase
{
    public :

	Error() : ErrorBase() {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual OldExprTree*   DeepCopy(void) const;

	protected:

  	virtual int         _EvalTree(const OldAttrList*, OldEvalResult*);
    virtual int         _EvalTree(const OldAttrList*, const OldAttrList*, OldEvalResult*);
};

class BinaryOp: public BinaryOpBase
{
    public :
};

class AddOp: public AddOpBase
{
    public :
  	AddOp(OldExprTree* l, OldExprTree* r) : AddOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual OldExprTree    *DeepCopy(void) const;
};


class SubOp: public SubOpBase
{
    public :
  	SubOp(OldExprTree* l, OldExprTree* r) : SubOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual OldExprTree    *DeepCopy(void) const;
};


class MultOp: public MultOpBase
{
    public :
  	MultOp(OldExprTree* l, OldExprTree* r) : MultOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual OldExprTree    *DeepCopy(void) const;
};


class DivOp: public DivOpBase
{
    public :
  	DivOp(OldExprTree* l, OldExprTree* r) : DivOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual OldExprTree    *DeepCopy(void) const;
};


class MetaEqOp: public MetaEqOpBase
{
    public :
  	MetaEqOp(OldExprTree* l, OldExprTree* r) : MetaEqOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual OldExprTree    *DeepCopy(void) const;
};


class MetaNeqOp: public MetaNeqOpBase
{
    public :
  	MetaNeqOp(OldExprTree* l, OldExprTree* r) : MetaNeqOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual OldExprTree    *DeepCopy(void) const;
};

class EqOp: public EqOpBase
{
    public :
  	EqOp(OldExprTree* l, OldExprTree* r) : EqOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual OldExprTree    *DeepCopy(void) const;
};


class NeqOp: public NeqOpBase
{
    public :
  	NeqOp(OldExprTree* l, OldExprTree* r) : NeqOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual OldExprTree    *DeepCopy(void) const;
};


class GtOp: public GtOpBase
{
    public :
  	GtOp(OldExprTree* l, OldExprTree* r) : GtOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual OldExprTree    *DeepCopy(void) const;
};


class GeOp: public GeOpBase
{
    public :
  	GeOp(OldExprTree* l, OldExprTree* r) : GeOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual OldExprTree    *DeepCopy(void) const;
};


class LtOp: public LtOpBase
{
    public :
  	LtOp(OldExprTree* l, OldExprTree* r) : LtOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual OldExprTree    *DeepCopy(void) const;
};


class LeOp: public LeOpBase
{
    public :
  	LeOp(OldExprTree* l, OldExprTree* r) : LeOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual OldExprTree    *DeepCopy(void) const;
};

class AndOp: public AndOpBase
{
    public :
  	AndOp(OldExprTree* l, OldExprTree* r) : AndOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual OldExprTree    *DeepCopy(void) const;
};


class OrOp : public OrOpBase
{
    public :
  	OrOp(OldExprTree* l, OldExprTree* r) : OrOpBase(l, r) {}
    virtual void        PrintToStr(char*);
	virtual int         CalcPrintToStr(void);
	virtual OldExprTree    *DeepCopy(void) const;
};

class AssignOp: public AssignOpBase
{
    public :
  	AssignOp(OldExprTree* l, OldExprTree* r) : AssignOpBase(l, r) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual OldExprTree    *DeepCopy(void) const;
	friend		    class OldAttrList;
};

class Function: public FunctionBase
{
    public:
	Function(char*a_name) : FunctionBase(a_name) {}
	virtual int         CalcPrintToStr(void);
    virtual void        PrintToStr(char*);
	virtual OldExprTree    *DeepCopy(void) const;

  	virtual int     _EvalTree(const OldAttrList*, OldEvalResult*);
    virtual int     _EvalTree(const OldAttrList*, const OldAttrList*, OldEvalResult*);

	int EvaluateArgumentToString(OldExprTree *arg, const OldAttrList *attrlist1, 	
						const OldAttrList *attrlist2, OldEvalResult *result) const;

	int FunctionScript(int number_of_arguments, OldEvalResult *arguments, 
					   OldEvalResult *result);
#ifdef HAVE_DLOPEN
	int FunctionSharedLibrary(int number_of_arguments, OldEvalResult *arguments, 
					   OldEvalResult *result);
#endif
	int FunctionGetTime(int number_of_arguments, OldEvalResult *arguments, 
						OldEvalResult *result);
	int FunctionTime(int number_of_arguments, OldEvalResult *arguments, 
						OldEvalResult *result);
	int FunctionInterval(int number_of_arguments, OldEvalResult *arguments, 
						OldEvalResult *result);
    int FunctionRandom(int number_of_arguments, OldEvalResult *arguments, 
						OldEvalResult *result);
	int FunctionIsUndefined(int number_of_args,	OldEvalResult *arguments,
						OldEvalResult *result);
	int FunctionIsError(int number_of_args,	OldEvalResult *arguments,
						OldEvalResult *result);
	int FunctionIsString(int number_of_args,	OldEvalResult *arguments,
						OldEvalResult *result);
	int FunctionIsInteger(int number_of_args,	OldEvalResult *arguments,
						OldEvalResult *result);
	int FunctionIsReal(int number_of_args,	OldEvalResult *arguments,
						OldEvalResult *result);
	int FunctionIsBoolean(int number_of_args,	OldEvalResult *arguments,
						OldEvalResult *result);
	int FunctionIfThenElse(const OldAttrList *attrlist1,
						const OldAttrList *attrlist2, OldEvalResult *result);
    int FunctionClassadDebugFunction(int number_of_args, OldEvalResult *evaluated_args,
						OldEvalResult *result);
	int FunctionString(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionReal(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionInt(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionFloor(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionRound(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionCeiling(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionStrcat(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionSubstr(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionStrcmp(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionStricmp(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionToUpper(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionToLower(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionSize(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionStringlistSize(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionStringlistSum(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionStringlistAvg(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionStringlistMin(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionStringlistMax(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionStringlistMember(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionStringlistIMember(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionStringlistRegexpMember(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionRegexp(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionRegexps(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
	int FunctionFormatTime(int number_of_args, OldEvalResult *evaluated_args, 
						OldEvalResult *result);
};

#endif
