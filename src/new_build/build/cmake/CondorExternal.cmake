
MACRO (CONDOR_EXTERNAL _PACKAGE _EXT_VERSION _NAMES _ON_OFF _CHECK_ENV _STRICT)

	string( TOUPPER "${_PACKAGE}" UP_PACKAGE )
	string( TOLOWER "${_PACKAGE}" LOW_PACKAGE )
	option(WITH_${UP_PACKAGE} "Enable or disable package" ${_ON_OFF})

	if (WITH_${UP_PACKAGE})

		message(STATUS "condor_external searching for ${_PACKAGE}")

		if (${_CHECK_ENV})
			FIND_LIBRARY(${_PACKAGE}_FOUND NAMES ${_NAMES} )
		else()
			
			set(_CHECK_DIR ${CONDOR_SOURCE_DIR}/build/externals/bundles/${LOW_PACKAGE}/${_EXT_VERSION})

			if (EXISTS ${_CHECK_DIR})
				set(${_PACKAGE}_FOUND ${_CHECK_DIR})
				#add include and link paths?
			endif()

		endif()


		if (${_PACKAGE}_FOUND)

			set(CONDOR_EXT_LINK_LIBS ${CONDOR_EXT_LINK_LIBS} ${_NAMES})
			set(HAVE_EXT_${UP_PACKAGE} ON)
			set(HAVE_${UP_PACKAGE} ON)
			message(STATUS "condor_external (${_PACKAGE})... found (${${_PACKAGE}_FOUND})")

		else()
			if (${_STRICT})
				message(FATAL_ERROR " condor_external STRICT (${_PACKAGE})... *not* found")
			else()
				message(STATUS "condor_external (${_PACKAGE})... *not* found")
			endif()
		endif()

	endif()

ENDMACRO (CONDOR_EXTERNAL)