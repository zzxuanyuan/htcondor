
if (HAVE_EXT_GLOBUS)

	condor_exe( gahp_server
				"gahp_server.cpp;my_ez.cpp"
				sbin
				"${GS_LINK_LIBS}" )

else()

	message(STATUS "NOTE: *gt2* targets will not be built HAVE_EXT_GLOBUS=!DEFINED")

endif (HAVE_EXT_GLOBUS)