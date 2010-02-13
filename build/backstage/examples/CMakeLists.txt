

file( GLOB Examples *config* condor* README)

install(FILES ${Examples}
        DESTINATION "${C_ETC_EXAMPLES}")

install(FILES		condor.boot.vdt
	DESTINATION	"${C_INIT}"
	PERMISSIONS	OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	RENAME		condor)

#Customize configuration files

add_custom_command( OUTPUT		condor_config.generic.new condor_config.submit.generic.new condor_config.local.central.manager.new condor.boot.new
		    WORKING_DIRECTORY	${CMAKE_CURRENT_SOURCE_DIR}
		    COMMAND		perl 
		    ARGS		customize)

add_custom_target( customize_configurations
		   ALL
		   DEPENDS condor_config.generic.new condor_config.submit.generic.new condor_config.local.central.manager.new condor.boot.new)

install(FILES		condor_config.generic.new
        DESTINATION	"${C_ETC_EXAMPLES}"
	RENAME		condor_config.generic)

install(FILES		condor_config.submit.generic.new
        DESTINATION	"${C_ETC_EXAMPLES}"
	RENAME		condor_config.submit.generic)

install(FILES		condor_config.local.central.manager.new
        DESTINATION	"${C_ETC_EXAMPLES}"
	RENAME		condor_config.local.central.manager)

install(FILES		condor.boot.new
        DESTINATION	"${C_ETC_EXAMPLES}"
	RENAME		condor.boot)



#Prepare OS specific config file

if ( ${PLATFORM} STREQUAL "Redhat" )
	#Redhat
	#message(STATUS "On Redhat")

	add_custom_command( OUTPUT condor_config.patched
			    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
			    COMMAND patch 
			    ARGS condor_config.generic condor_config.generic.rpm.patch -o condor_config.patched )

	add_custom_target( patch_configurations
			   ALL
			   DEPENDS condor_config.patched)

	install(FILES		condor_config.patched
		DESTINATION	"${C_ETC}"
		RENAME		condor_config)

elseif ( ${PLATFORM} STREQUAL "Debian" )
	#Debian
	#message(STATUS "On Debian")

	add_custom_command( OUTPUT condor_config.patched
			    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
			    COMMAND patch 
			    ARGS condor_config.generic condor_config.generic.debian.patch -o condor_config.patched )

	add_custom_target( patch_configurations
			   ALL
			   DEPENDS condor_config.patched)

	install(FILES		condor_config.patched
		DESTINATION	"${C_ETC}"
		RENAME		condor_config)
else()
	#Other Platform
	install(FILES		condor_config.generic
		DESTINATION	"${C_ETC}"
		RENAME		condor_config)

endif()


install(FILES		condor_config.local.generic
	DESTINATION	"${C_ETC}"
	RENAME		condor_config.local)




