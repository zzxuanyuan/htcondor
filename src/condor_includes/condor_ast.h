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
//     ExprTree* MinTree(ClassAdList* list)
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

#define WANT_NAMESPACES
#include "classad_distribution.h"

#include "condor_exprtype.h"
#include "condor_astbase.h"

class EvalResult
{
 public:
    EvalResult() { type = LX_UNDEFINED; }
  	~EvalResult() { if(type == LX_STRING) delete [] s; }
	void fPrintResult(FILE *); // for debugging
   	union
	{
		int i;
   	    float f;
   	    char* s;
	};
  	LexemeType type;
};

class Variable : public ExprTree
{
 public:
	Variable(char*);
	~Variable( );
	virtual void        PrintToNewStr(char**);
	virtual int 		CalcPrintToStr( void );
	virtual void        PrintToStr(char*);
	virtual ExprTree*   DeepCopy(void) const;
	char*	const	    Name() { return name; }
 protected:
	virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*);

#ifdef USE_STRING_SPACE_IN_CLASSADS
	int                 stringSpaceIndex;
#endif 
	char*               name;
};

class AssignOp : public ExprTree
{
 public:
	AssignOp(ExprTree*, ExprTree*);
	AssignOp(const char *, classad::ExprTree *);
	~AssignOp( );
	virtual void        PrintToNewStr(char**);
	virtual int 		CalcPrintToStr( void );
    virtual void        PrintToStr(char*);
	virtual ExprTree    *DeepCopy(void) const;
	virtual ExprTree*   LArg()   { return lArg; }
	virtual ExprTree*   RArg()   { return rArg; }
 protected:
	virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*);
	ExprTree* 	        lArg;
	ExprTree* 	        rArg;
};

class OtherExpr : public ExprTree
{
 public:
	OtherExpr( ) { expr == NULL; }
	OtherExpr( classad::ExprTree* );
	~OtherExpr( );
	virtual void        PrintToNewStr(char**);
	int 				CalcPrintToStr( void );
    virtual void        PrintToStr(char*);
	virtual ExprTree    *DeepCopy(void) const;
	void				ReplaceExpr( classad::ExprTree* );

	friend class AttrList;
	friend class AssignOp;
 protected:
	virtual int         _EvalTree(const AttrList*, const AttrList*, EvalResult*);
	classad::ExprTree* expr;
};

class String : public OtherExpr
{
 public:
	String( ) { }
	char * const		Value( );
};

class Integer : public OtherExpr
{
 public:
	Integer( ) { }
	int					Value( );
};

class Float : public OtherExpr
{
 public:
	Float( ) { }
	float				Value( );
};

class ClassadBoolean : public OtherExpr
{
 public:
	ClassadBoolean( ) { }
	int					Value( );
};


extern	int		Parse(const char*, ExprTree*&);

#endif
