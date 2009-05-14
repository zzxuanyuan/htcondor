/***************************************************************
 *
 * Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
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

#ifndef CABENCH_INST_BASE_H
#define CABENCH_INST_BASE_H

#include "cabench_base.h"
#include "cabench_adwrap_base.h"
#include "cabench_inst_options.h"
using namespace std;
#include <vector>
#include "stdio.h"

class CaBenchInstData
{
  public:
	CaBenchInstData( int lineno, char *linebuf );
	~CaBenchInstData( void );

	enum DataType{ NONE, BOOLEAN, INTEGER, FLOAT, STRING };

	bool isValid( void ) const { return m_type != NONE; };
	const char *getAttr( void ) const { return m_attribute; };
	DataType getType( void ) const { return m_type; };
	double getDupPercent( void ) const { return m_dup_percent; };
	int getDupAds( int ads ) const {
		return (int) rint( ads * m_dup_v );
	};
	bool getValue( bool &v ) const {
		if ( m_type != BOOLEAN ) {
			return false;
		}
		v = m_value_boolean;
		return true;
	};
	bool getValue( int &v ) const {
		if ( m_type != INTEGER ) {
			return false;
		}
		v = m_value_integer;
		return true;
	};
	bool getValue( double &v ) const {
		if ( m_type != FLOAT ) {
			return false;
		}
		v = m_value_float;
		return true;
	};
	bool getValue( const char *&v ) const {
		if ( m_type != STRING ) {
			return false;
		}
		v = m_value_string;
		return true;
	};

  private:
	DataType	 m_type;
	const char	*m_attribute;
	double		 m_dup_percent;
	double		 m_dup_v;
	bool		 m_value_boolean;
	long		 m_value_integer;
	double		 m_value_float;
	const char	*m_value_string;
};

class CaBenchInstBase : public CaBenchBase
{
  public:
	CaBenchInstBase( const CaBenchInstOptions & );
	virtual ~CaBenchInstBase( void );

	// Finish the setup
	bool setup( void );
	bool readDataFile( void );

	// Do real work
	bool runLoops( void );
	bool addAttr( const CaBenchInstData &dp, int adno, int avno );

	// Done; dump final info
	bool finish( void );

	// Pure-Virtual member methods
	virtual bool initAds( int num_ads ) = 0;
	virtual bool addAttr( int adno, const char *attr, bool v ) = 0;
	virtual bool addAttr( int adno, const char *attr, int v ) = 0;
	virtual bool addAttr( int adno, const char *attr, double v ) = 0;
	virtual bool addAttr( int adno, const char *attr, const char *v ) = 0;
	virtual bool deleteAds( void ) = 0;

	const CaBenchInstOptions & Options( void ) const {
		return dynamic_cast<const CaBenchInstOptions &>(m_options);
	};
	
  private:
	list<CaBenchInstData *>	m_avlist;

};

#endif
