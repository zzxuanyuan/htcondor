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

#ifndef TRANSFERER_UPLOAD_H
#define TRANSFERER_UPLOAD_H

#include "TransfererBase.h"
#include "ReplicatorFile.h"
#include "ReplicatorFileSet.h"

//
// Upload-specific file
//
class UploadFile : public ReplicatorFile
{
  public:
	UploadFile( void );
	~UploadFile( void ) { };

	bool copyFile( filesize_t &size ) const;

  protected:
	SafeFileRotator *createRotator( void ) const;
	SafeFileCopier  *createCopier( void ) const;
};

//
// Upload-specific file set
//
class UploadTransferer;
class UploadFileSet : public ReplicatorFileSet
{
  public:
	UploadFileSet( UploadTransferer & );
	virtual ~UploadFileSet( void ) { };

	// Create a new file object
	ReplicatorFile	*createFileObject( void ) const {
		return new UploadFile( );
	};

 private:
	UploadTransferer	&m_uploader;

};

/* Class      : UploadTransferer
 * Description: class, encapsulating the uploading 'condor_transferer'
 *              process behaviour
 */
class UploadTransferer : public BaseTransferer
{
  public:
    /* Function  : UploadTransferer constructor
     * Arguments : sinful  - downloading daemon sinfull string
     */
    UploadTransferer( void );

	// Initialization related
	bool preInit( void );
    //bool postInit( void );
    //bool reconfig( void );
	void runForever( void ) {
		m_doExit = false;
	};

	// Basic functionality
	bool cancel( void );
	bool cleanupTempFiles( void );
	bool finish( void );
	bool sendFileList( const Sinful &sinful );
	bool sendFileList( const ReplicatorPeer &downloader );

	// Command handlers
	int handleTransferFile( int command, Stream *stream );
	int handleTransferRequest( int command, Stream *stream );
	int handleTransferComplete( int command, Stream *stream );

  private:
	bool upload( void );
	bool uploadFile(MyString& filePath, MyString& extension);

  private:
	bool	m_doExit;

};

#endif // TRANSFERER_UPLOAD_H

// Local Variables: ***
// mode:C ***
// comment-column:0 ***
// tab-width:4 ***
// c-basic-offset:4 ***
// End: ***
