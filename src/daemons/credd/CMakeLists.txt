if (WINDOWS)

	condor_exe( condor_credd
				"credd.windows.cpp;credd.windows.h"
				${C_SBIN}
				"${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND}" )
				
else(WINDOWS)

	if ( HAVE_EXT_CLASSADS AND HAVE_EXT_GLOBUS )

		file( GLOB CREDDSRCS credd_main.* credd.* classadUtil.* X509credentialWrapper.* )
		condor_exe( condor_credd
					"${CREDDSRCS}"
					${C_SBIN}
					"${CONDOR_TOOL_LIBS};${PCRE_FOUND};${OPENSSL_FOUND}" )

		message(STATUS "TODO: TSTCLAIR credd->stork targets?")
		
	else ()

		message(STATUS "NOTE: *credd* targets will not be built HAVE_EXT_GLOBUS=!DEFINED and HAVE_EXT_CLASSADS=!DEFINED")

	endif()
	
endif (WINDOWS)