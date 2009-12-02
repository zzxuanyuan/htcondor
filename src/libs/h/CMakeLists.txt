#set the local target 
set(TargetName syscall_numbers)

#add_custom_command( OUTPUT syscall_numbers.h 
command_target(syscall_h
               bash
               "-c;\";awk;-f;awk_prog.include_file;syscall_numbers.tmpl;>;syscall_numbers.h;\""
               syscall_numbers.h)

command_target(syscall_c
               bash
               "-c;\";awk;-f;awk_prog.name_table;syscall_numbers.tmpl;>;syscall_numbers.c;\""
               syscall_numbers.c)

add_library( syscall_numbers syscall_numbers.h syscall_numbers.c )
add_dependencies(syscall_numbers syscall_c syscall_h )


