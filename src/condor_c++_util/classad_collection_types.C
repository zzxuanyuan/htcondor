#include "condor_common.h"
#include "condor_debug.h"
#include "condor_attributes.h"

#include "classad_collection_types.h"
#include "classad_collection.h"

static char *_FileName_ = __FILE__;

BaseCollection::
BaseCollection(BaseCollection* parent, const MyString& rank) : childItors( 4 ), contentItors( 4 )
{
    Parent=parent;
    ClassAd *ad = new ClassAd( );
    if( ad ) {
        if( rank.Value() && strcmp( rank.Value(), "" ) != 0 ) {
            ad->Insert( ATTR_RANK, rank.Value( ) );
        } else {
            ad->InsertAttr( ATTR_RANK, 0 );
        }
    }
    rankCtx.ReplaceLeftAd( ad );
	childItors.fill( NULL );
	contentItors.fill( NULL );
}

BaseCollection::
BaseCollection(BaseCollection* parent, ExprTree *tree) : childItors( 4 ), contentItors( 4 )
{
    Parent=parent;
    ClassAd *ad = new ClassAd( );
    if( ad ) {
        if( tree ) {
            ad->Insert( ATTR_RANK, tree->Copy( ) );
        } else {
            ad->InsertAttr( ATTR_RANK, 0 );
        }
    }
    rankCtx.ReplaceLeftAd( ad );
	childItors.fill( NULL );
	contentItors.fill( NULL );
}

BaseCollection::
~BaseCollection( )
{
}

ExprTree *BaseCollection::
GetRankExpr()
{
    ClassAd *ad = rankCtx.GetLeftAd( );
    return( ad ? ad->Lookup( ATTR_RANK ) : NULL );
}

double BaseCollection::
GetRankValue( ClassAd* ad )
{
    double d;
	rankCtx.ReplaceRightAd( ad );
    if( !rankCtx.EvaluateAttrNumber( "leftrankvalue", d ) ) {
        rankCtx.RemoveRightAd( );
        return -1;
    }
    rankCtx.RemoveRightAd( );
    return d;
}


void BaseCollection::
RegisterChildItor( CollChildIterator *itor )
{
	int last = childItors.getlast( );
	for( int i = 0 ; i <= last ; i++ ) {
		if( childItors[i] == itor ) return;
	}
	childItors[last+1] = itor;
}


void BaseCollection::
UnregisterChildItor( CollChildIterator *itor )
{
	int last = childItors.getlast( );
	for( int i = 0; i <= last; i++ ) {
		if( childItors[i] == itor ) {
			childItors[i] = childItors[last];
			childItors[last] = NULL;
		}
	}
}


void BaseCollection::
RegisterContentItor( CollContentIterator *itor )
{
	int last = contentItors.getlast( );
	for( int i = 0 ; i <= last ; i++ ) {
		if( contentItors[i] == itor ) return;
	}
	contentItors[last+1] = itor;
}


void BaseCollection::
UnregisterContentItor( CollContentIterator *itor )
{
	int last = contentItors.getlast( );
	for( int i = 0; i <= last; i++ ) {
		if( contentItors[i] == itor ) {
			contentItors[i] = contentItors[last];
			contentItors[last] = NULL;
		}
	}
}


ConstraintCollection::
ConstraintCollection(BaseCollection* parent, const MyString& rank, const MyString& constraint)
	: BaseCollection(parent,rank)
{
	ClassAd *ad = new ClassAd( );
	ad->Insert( ATTR_REQUIREMENTS, constraint.Value( ) );
	constraintCtx.ReplaceLeftAd( ad );
}

ConstraintCollection::
ConstraintCollection(BaseCollection* parent,ExprTree *rank, ExprTree *constraint)
	: BaseCollection(parent,rank)
{
	if( constraint && rank ) {
		ClassAd *ad = new ClassAd( );
		ad->Insert( ATTR_REQUIREMENTS, constraint->Copy( ) );
		constraintCtx.ReplaceLeftAd( ad );
	} else {
		constraint = rank = NULL;
	}
}

bool ConstraintCollection::
CheckClassAd(ClassAd* Ad)
{
	bool rval, b;
	Value val;
	constraintCtx.ReplaceRightAd( Ad );
	rval = ( constraintCtx.EvaluateAttr( "rightmatchesleft", val ) &&
		 val.IsBooleanValue( b ) && b );
	constraintCtx.RemoveRightAd( );
	return( rval );
}

bool PartitionChild::
CheckClassAd(ClassAd* Ad)
{
	return true;
}

