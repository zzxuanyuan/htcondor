#ifndef _TOOL_CORE_H_
#define _TOOL_CORE_H_

typedef void (*USAGE_FUNC)(int);

extern const char *toolname;
extern const char *addr_arg;
extern const char *pool_arg;
extern const char *name_arg;

void tool_parse_command_line(int argc, char *argv[]);
void set_usage(USAGE_FUNC u_func);
void tool_exit(int exit_code);
void tool_usage(int exitcode);
void option_needs_arg(const char* option);
#endif