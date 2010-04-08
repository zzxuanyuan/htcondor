

#######################################################################
# examples install targets
file( GLOB EXAMPLE_CFG_FILES examples/condor_config* )
message(STATUS "TODO: TSTCLAIR->Review install targets for examples")

if ( NOT WINDOWS )	
	#file ( GLOB EXEC_FILES examples/*script )
	#list ( APPEND EXAMPLE_CFG_FILES ${EXEC_FILES})
	
	add_subdirectory(examples)
	add_subdirectory(release_notes)

	#This will install empty folder such as executeand spool 
	add_subdirectory(misc)

else (NOT WINDOWS)

install ( FILES ${EXAMPLE_CFG_FILES}
		  DESTINATION ${C_ETC}
		  #PERMISSIONS # if one wants to set the permissions
		)
endif (NOT WINDOWS)

# end example install targets
#######################################################################

#######################################################################
if ( WINDOWS )

	# I don't particularly like this, but it gets the job done.
	# check if we want to change this...
	#file (MAKE_DIRECTORY log spool execute)

	# windows install targets <- could have installer script create these...
	#install ( DIRECTORY log spool execute
	#		  DESTINATION . ) # will add the windows install directory

	install ( FILES ./win/cdmake.exe
				./win/mkisofs.bat
			  DESTINATION ${C_BIN} )

	# the following will create a config file for windows installer.
    configure_file(${CONDOR_SOURCE_DIR}/build/backstage/win/condor_config_win.rht ${CONDOR_SOURCE_DIR}/build/backstage/win/condor_config_win @ONLY)
	install ( FILES win/condor_config_win
			  DESTINATION .
			  RENAME condor_config )

endif( WINDOWS )
# windows install targets
#######################################################################
