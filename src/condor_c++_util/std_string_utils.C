/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
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

// Utilities for working with std::string objects.

#include "std_string_utils.h"
#include "directory.h"

#define ERROR_RTN		(-1)
#ifndef MIN
#define MIN(_a,_b)                  (((_a) < (_b)) ? (_a) : (_b))
#endif

int string_vprintf(std::string & s, const char *format, va_list args)
{
    // get variable string length by writing string to null output device, and
    // counting characters written.
    FILE *null_output = fopen(NULL_FILE, "w");
    if (null_output == NULL) {
        return ERROR_RTN;
    }
    int len = vfprintf(null_output, format, args);
    fclose(null_output);

    // Get a temporary buffer to hold string.
    char *buf = (char *)malloc(len+1);
    if (buf == NULL) {
        return ERROR_RTN;
    }
    if (vsprintf(buf, format, args) != len) {
        free(buf);
        return ERROR_RTN;
    }

    // Copy from the temporary buffer to the string.
    s = buf;
    free(buf);

    return len;
}

int string_printf(std::string & s, const char *format, ...)
{
    va_list args;
    va_start(args, format);
	int nchars = string_vprintf(s, format, args);
    va_end(args);
	return nchars;
}

int string_readfile(const std::string & path, std::string & buf, int maxRead)
{
	int total_bytes_read = 0;
	ssize_t bytes_read;
	size_t bytes_to_read;
	char readbuf[BUFSIZ];

	buf.clear();	// erase the string;

	if (maxRead == 0) {
		return maxRead;
	}

	// For negative values of maxRead, buffer the entire file contents.  Use
	// with caution.
	if (maxRead < 0) {
		StatInfo stat_info( path.c_str() );
		if ( stat_info.Errno() != 0) {
			return ERROR_RTN;
		}
		maxRead = stat_info.GetFileSize();
	}

	int fd = open(path.c_str(), O_RDONLY);
	if (fd < 0) {
		return ERROR_RTN;	// error opening file
	}
	buf.reserve(maxRead+1);	// preallocate memory for entire file, plus null

	while (total_bytes_read < maxRead) {
		bytes_to_read = MIN( (int)sizeof(readbuf), maxRead - total_bytes_read);
		bytes_read = read(fd, readbuf, sizeof(readbuf) );
		if (bytes_read < 0) {
			if (errno == EINTR) {
				continue;	// syscall interrupted, try again.
			} else {
				return ERROR_RTN;	// error reading file
			}
		} else if (bytes_read == 0) {
			return total_bytes_read;
		} else {
			buf += readbuf;
			total_bytes_read += bytes_read;	// works for binary data????
		}
	}

	return total_bytes_read;
}


