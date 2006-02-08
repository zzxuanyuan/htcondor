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
