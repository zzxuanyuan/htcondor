
#ifndef FILENAME_TOOLS_H
#define FILENAME_TOOLS_H

/**
Take an input string in URL form, and split it into its components.
URLs are of the form "method://server:port/filename".  Any component
that is missing will be recorded as the string with one null character.
If the port is not specified, it is recorded as -1.
<p>
The outputs method, server, and path must all point to buffers of length
_POSIX_PATH_MAX.  This function will fill in that many characters and
ignore any remaining data in those input fields.
*/

void filename_url_parse( char *input, char *method, char *server, int *port, char *path );

/** 
Take an input string which looks like this:
"filename = url ; filename = url ; ..."
Search for a filename matching the input string, and copy the 
corresponding url into output.  The output can then be parsed by
filename_url_parse().
<p>
The filename argument may be up to _POSIX_PATH_MAX, and output must
point to a buffer of _POSIX_PATH_MAX*3.
*/

void filename_remap_find( char *input, char *filename, char *output );

#endif
