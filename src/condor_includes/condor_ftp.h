#ifndef _CONDOR_FTP_
#define _CONDOR_FTP_

/* This file describes the enumeration to specify the file transfer
protocol used between a submitting client and the transferd. Please keep
these in the order you find, appending new ones only at the end. */

enum FTPMode /* File Transfer Protocol Mode */
{
	FTP_UNKNOWN = 0,	/* I don't know what file transfer protocol to use */
	FTP_CFTP,			/* Use the internal condor FileTansfer Object */
};

enum FTPDirection
{
	FTPD_UNKNOWN = 0,	/* Don't know what direction I should use */
	FTPD_UPLOAD,		/* upload from the perspective of the calling process */
	FTPD_DOWNLOAD,		/* download from the per. of the calling process. */
};

#endif 
