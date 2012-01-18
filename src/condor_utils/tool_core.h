#ifndef _TOOL_CORE_H_
#define _TOOL_CORE_H_
#include "basename.h"
typedef void (*USAGE_FUNC)(int);

extern const char *toolname;
extern const char *addr_arg;
extern const char *pool_arg;
extern const char *name_arg;

int tool_parse_command_line(int pos, char *argv[]);
void set_usage(USAGE_FUNC u_func);
void tool_exit(int exit_code = 1);
void tool_usage(int exitcode = 1);
void option_needs_arg(const char* option);
bool tool_is_arg(const char * parg, const char * pval, int must_match_length = 1);
#endif