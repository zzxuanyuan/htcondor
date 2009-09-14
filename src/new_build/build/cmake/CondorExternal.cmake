# condor_external - will search for an external based on it's input
#                   params, and set a series of VARS for further the build.
#
#
# IN: 
#     _PACKAGE        - Name of external package
#     _EXT_VERSION    - "version string"
#     _NAMES          - "Names of libraries.lib/.a/.so to search for"
#     _ON_OFF         - Indicates if we should perform the search (ON/OFF)
#     _CHECK_ENV      - Indicates if we should check the env e.g.-PROPER (ON/OFF)
#     _STRICT         - Indicates if we should fail if *not found* (ON/OFF)
#
# OUT:
#     WITH_${UP_PACKAGE}     - Indicated that the option is enabled
#     ${UP_PACKAGE}_FOUND    - Indicates the package has been found
#                            If the package is found the link and include
#                            deps will be appended to.
#  
#

MACRO (CONDOR_EXTERNAL _PACKAGE _EXT_VERSION _NAMES _ON_OFF _CHECK_ENV _STRICT)

	string( TOUPPER "${_PACKAGE}" UP_PACKAGE )
	string( TOLOWER "${_PACKAGE}" LOW_PACKAGE )
	option(WITH_${UP_PACKAGE} "Enable or disable package" ${_ON_OFF})	
	
	# if the option is enabled _ON_OFF,
	if (WITH_${UP_PACKAGE})

		message(STATUS "condor_external searching for ${_PACKAGE}")

        ##############
		if (${_CHECK_ENV})
			FIND_LIBRARY(${UP_PACKAGE}_FOUND NAMES ${_NAMES})
		else()
			
			set (CONDOR_EXTERNAL_LOC ${CONDOR_SOURCE_DIR}/build/externals)
			
			# condor bundles it's externals
			set(_CHECK_DIR ${CONDOR_EXTERNAL_LOC}/bundles/${LOW_PACKAGE}/${_EXT_VERSION})

			if (EXISTS ${_CHECK_DIR})
			
				set(${UP_PACKAGE}_FOUND ${_CHECK_DIR})
				
				# adds a custom prebuild step to the base which is utils.
				add_custom_target(${LOW_PACKAGE} ALL
				                   COMMAND perl -w ${CONDOR_EXTERNAL_LOC}/build_external 
				                   --extern_src=${CONDOR_EXTERNAL_LOC} 
				                   --extern_build=${CONDOR_EXTERNAL_LOC} 
				                   --package_name=${LOW_PACKAGE}-${_EXT_VERSION}
				                   --extern_config=${CONDOR_SOURCE_DIR}/build/config/config.sh			                        				                   
				                   COMMENT "Building ${LOW_PACKAGE}-${_EXT_VERSION}...")				                   
												
                # setup -I & -L directories								
				#if (EXISTS ${_CHECK_DIR}/include)
				    #include_directories(${_CHECK_DIR}/include)
				#elseif (EXISTS ${_CHECK_DIR}/inc32)
				    #include_directories(${_CHECK_DIR}/inc32)
				#endif()
				
				#if (EXISTS ${_CHECK_DIR}/lib)
				    #link_directories(${_CHECK_DIR}/lib)
				#elseif (EXISTS ${_CHECK_DIR}/out32dll)
				    #link_directories(${_CHECK_DIR}/out32dll)
				#endif()
				 
			endif()

		endif()
        ##############
        
        # Only OPENSSL needs this
		#add_definitions(-DWITH_${UP_PACKAGE})
                
        ##############
		if (${UP_PACKAGE}_FOUND)
			set(CONDOR_EXT_LINK_LIBS ${CONDOR_EXT_LINK_LIBS} ${_NAMES})
			set(HAVE_EXT_${UP_PACKAGE} ON)
			set(HAVE_${UP_PACKAGE} ON)
			message(STATUS "condor_external (${_PACKAGE})... found (${${_PACKAGE}_FOUND})")						
		else()
			if (${_STRICT})
				message(FATAL_ERROR " condor_external *STRICT* (${_PACKAGE})... *not* found")
			else()
				message(STATUS "condor_external (${_PACKAGE})... *not* found")
			endif()
		endif()
		##############
		
	endif()

ENDMACRO (CONDOR_EXTERNAL)