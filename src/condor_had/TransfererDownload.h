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

#ifndef TRANSFERER_DOWNLOAD_H
#define TRANSFERER_DOWNLOAD_H

#include "TransfererBase.h"
#include "ReplicatorFile.h"
#include "ReplicatorFileSet.h"

//
// Download-specific file
//
class DownloadFile : public ReplicatorFile
{
  public:
	DownloadFile( void );
	~DownloadFile( void ) { };

	bool init( const ReplicatorFile *remote );
	const ReplicatorFile *getRemote( void ) const {
		return m_remote;
	};
	void skip( void ) {
		m_skipped = true;
	};
	bool isDone( void ) const {
		return m_done;
	};
	void setDone( void ) {
		m_done = true;
	};
	bool isAppend( void ) const {
		return m_append;
	};
	void setAppend( bool append ) {
		m_append = append;
	};
	StatStructOff getOffset( void ) const {
		return m_offset;
	};
	void setOffset( StatStructOff offset ) {
		m_offset = offset;
	};

	// Overload the 'should rotate' operator
	bool shouldRotate( void ) const {
		return !m_append && !m_skipped;
	};

  protected:
	SafeFileRotator *createRotator( void ) const;
	SafeFileCopier  *createCopier( void ) const;

  private:
	bool					 m_done;
	const ReplicatorFile	*m_remote;
	StatStructOff			 m_offset;
	bool					 m_append;
	bool					 m_skipped;
};

//
// Download-specific file set
//
class DownloadTransferer;
class DownloadFileSet : public ReplicatorFileSet
{
  public:

	DownloadFileSet( DownloadTransferer & );
	virtual ~DownloadFileSet( void );

	// Create a new file object
	ReplicatorFile	*createFileObject( void ) const {
		return new DownloadFile( );
	};

	bool startDownload( Stream *, const ReplicatorFileSet * );
	bool startNextFile( void );

  private:
	list <DownloadFile *>	*m_dlist;
	DownloadTransferer		&m_downloader;
	const ReplicatorFileSet	*m_remoteFileset;
};

/* Class      : DownloadTransferer
 * Description: class, encapsulating the downloading 'condor_transferer'
 *              process behaviour
 */
class DownloadTransferer : public BaseTransferer
{
 public:
	/* Function  : DownloadReplicaTransferer constructor
	 * Arguments : pDaemonSinfulString  - uploading daemon sinfull string
	 *             pVersionFilePath     - version string in dot-separated format
	 *             pStateFilesPathsList - list of paths to the state files
	 */
    DownloadTransferer( void );
    DownloadTransferer( const Sinful &sinful );
	~DownloadTransferer( void );

	// Initialization related
	bool preInit( void );
    //bool postInit( void );
    //bool reconfig( void );

	// Shutdown related
	void enableRotate( void ) {
		m_enable_rotate = true;
	};
	bool finish( void );

	// Basic functionality
	bool contactPeerReplicator( const Sinful &sinful );
    bool downloadFile( DownloadFile *file, bool &started );
	bool cancel( void );
	bool cleanupTempFiles( void );
	BaseTransferer::Status getStatus( void ) const;

  private:
    bool download( void );

	// Command handlers
	int handleFilesetData( int command, Stream *stream );
	int handleTransferFileData( int command, Stream *stream );
	int handleTransferNak( int command, Stream *stream );

  private:
	DownloadFileSet		*m_dfs;
	ReplicatorPeer		*m_uploader;

	// Current state info
	DownloadFile		*m_current;

	// For testing
	bool				 m_enable_rotate;
};

#endif // TRANSFERER_DOWNLOAD_H

// Local Variables: ***
// mode:C ***
// comment-column:0 ***
// tab-width:4 ***
// c-basic-offset:4 ***
// End: ***
