#Prepare empty directory


#Local dir
file (MAKE_DIRECTORY execute spool empty )

add_custom_target( change_execute_folder_permission
		   ALL
		   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		   COMMAND chmod 1777 execute
	           DEPENDS execute)

install(DIRECTORY	execute
	DESTINATION	"${C_LOCAL_DIR}"
	DIRECTORY_PERMISSIONS	USE_SOURCE_PERMISSIONS)

install(DIRECTORY	spool
	DESTINATION	"${C_LOCAL_DIR}")


#Create empty dir

install(DIRECTORY	empty/
	DESTINATION	"${C_LOCK_DIR}")

install(DIRECTORY	empty/
	DESTINATION	"${C_LOG_DIR}")

install(DIRECTORY	empty/
	DESTINATION	"${C_RUN_DIR}")

#install(DIRECTORY	empty/
#	DESTINATION	"${C_MAN}")

install(DIRECTORY	empty/
	DESTINATION	"${C_INCLUDE}")

install(DIRECTORY	empty/
	DESTINATION	"${C_LIB}")



#if HAS_STATIC
#simple_dir_target(static_dir)
#simple_dir_target(static_dir/bin)
#simple_dir_target(static_dir/lib)
#simple_dir_target(static_dir/libexec)
#simple_dir_target(static_dir/libexec/glite)
#simple_dir_target(static_dir/sbin)
#simple_dir_target(static_dir/etc)
#simple_dir_target(static_dir/etc/examples)
#simple_dir_target(static_dir/include)
#simple_dir_target(static_dir/src)
#simple_dir_target(static_dir/src/chirp)
#simple_dir_target(static_dir/src/drmaa)
#simple_dir_target(static_dir/src/startd_factory)
#simple_dir_target(static_dir/src/startd_factory/BlueGeneP)
#simple_dir_target(static_dir/man)
#simple_dir_target(static_dir/man/man1)
#simple_dir_target(static_dir/lib/webservice)
#simple_dir_target(static_dir/sql)
#simple_dir_target(static_contrib)
#simple_dir_target(static_contrib/bin)
#simple_dir_target(static_contrib/lib)
#simple_dir_target(static_contrib/libexec)
#simple_dir_target(static_contrib/sbin)
#simple_dir_target(static_contrib/etc)
#simple_dir_target(static_contrib/etc/examples)
#simple_dir_target(static_contrib/include)
#simple_dir_target(static_contrib/src)
#simple_dir_target(static_contrib/sql)