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

#ifndef SAFE_FILE_OPS_H 
#define SAFE_FILE_OPS_H

#include "MyString.h"

class SafeFileOp
{
  public:
	// C-Tor, D-Tor, initializers
	SafeFileOp( bool auto_cleanup );
	SafeFileOp( const char *filePath, const char *ext, int pid,
				 bool auto_cleanup );
	virtual ~SafeFileOp( void );
	bool init( const char *filePath, const char *ext, int pid );
	bool init( const MyString &filePath, const char *ext, int pid );
	bool setPid( int pid );
	
	bool UnlinkTmp( void ) const;

	// Accessors
	const char *getFilePath( void ) const {
		return m_filePath.Value();
	};
	const char *getTmpFilePath( void ) const {
		return m_tmpFilePath.Value();
	};

  protected:
	bool doOperation( void );
	virtual bool Operate( void ) = 0;
	virtual const char *OpName( void ) = 0;

  private:
	bool buildPath( int pid );

  protected:
	MyString	m_filePath;
	MyString	m_tmpFilePath;
	MyString	m_ext;
	bool		m_auto_cleanup;
};

class SafeFileNop : public SafeFileOp
{
  public:
	SafeFileNop( bool auto_cleanup );
	SafeFileNop( const char *filePath, const char *ext,
				 int pid, bool auto_cleanup );
	SafeFileNop( const MyString &filePath, const char *ext,
				 int pid, bool auto_cleanup );

  private:
	bool Operate( void ) {
		return true;
	};
	const char *OpName( void ) {
		return "NOP";
	};
};

class SafeFileRotator : public SafeFileOp
{
  public:
	SafeFileRotator( bool auto_cleanup );
	SafeFileRotator( const char *filePath, const char *ext,
					 int pid, bool auto_cleanup );
	SafeFileRotator( const MyString &filePath, const char *ext,
					 int pid, bool auto_cleanup );

	bool Rotate( void ) {
		return doOperation( );
	};

  private:
	bool Operate( void );
	const char *OpName( void ) {
		return "Rotation";
	};
};

class SafeFileCopier : public SafeFileOp
{
  public:
	SafeFileCopier( bool auto_cleanup );
	SafeFileCopier( const char *filePath, const char *ext,
					int pid, bool auto_cleanup );
	SafeFileCopier( const MyString &filePath, const char *ext,
					int pid, bool auto_cleanup );

	bool Copy( void ) {
		return doOperation( );
	};

  private:
	bool Operate( void );
	const char *OpName( void ) {
		return "Copy";
	};
};

#endif // SAFE_FILE_OPS_H
