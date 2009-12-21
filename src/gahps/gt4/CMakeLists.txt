if (HAVE_EXT_GLOBUS)

	 install(FILES gt4-gahp.jar
			 DESTINATION lib )
			#PERMISSIONS ?

else()

	message(STATUS "NOTE: *gt4* targets will not be built HAVE_EXT_GLOBUS=!DEFINED")

endif (HAVE_EXT_GLOBUS)