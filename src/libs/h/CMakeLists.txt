#set the local target 
set(TargetName syscall_numbers)

add_custom_command( OUTPUT syscall_numbers.h 
					COMMAND awk
					ARGS -f awk_prog.include_file syscall_numbers.tmpl > syscall_numbers.h )
					
add_custom_command( OUTPUT syscall_numbers.c 
					COMMAND awk
					ARGS -f awk_prog.name_table syscall_numbers.tmpl > syscall_numbers.c )
					
add_library( syscall_numbers syscall_numbers.c syscall_numbers.h )



