if (HAVE_EXT_GLOBUS)

condor_glob( HeaderFiles SourceFiles "" )

condor_exe( nordugrid_gahp
			"${HeaderFiles};${SourceFiles}"
			sbin
			"${NRDG_LINK_LIBS}" )

else()

	message( STATUS "NOTE: *nordugrid* targets will not be built HAVE_EXT_GLOBUS=!DEFINED" )

endif (HAVE_EXT_GLOBUS)