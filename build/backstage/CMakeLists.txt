

#######################################################################
# examples install targets
file( GLOB EXAMPLE_CFG_FILES examples/condor_config* )
message(STATUS "TODO: TSTCLAIR->Review install targets for examples")

if ( NOT WINDOWS )	
	file ( GLOB EXEC_FILES examples/*script )
	list ( APPEND EXAMPLE_CFG_FILES ${EXEC_FILES})
endif(NOT WINDOWS)

install ( FILES ${EXAMPLE_CFG_FILES}
		  DESTINATION ${C_ETC}
		  #PERMISSIONS # if one wants to set the permissions
		)
# end example install targets
#######################################################################

#######################################################################
if ( WINDOWS )

	# I don't particularly like this, but it gets the job done.
	# check if we want to change this...
	file (MAKE_DIRECTORY log spool execute)

	# windows install targets
	install ( DIRECTORY ./win/examples log spool execute
			  DESTINATION . ) # will add the windows install directory

	install ( FILES ./win/cdmake.exe
				./win/mkisofs.bat
			  DESTINATION ${C_BIN} )

endif( WINDOWS )
# windows install targets
#######################################################################
