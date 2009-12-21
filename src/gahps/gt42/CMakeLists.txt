if (HAVE_EXT_GLOBUS)

	 install(FILES gt42-gahp.jar
			 DESTINATION lib )
			#PERMISSIONS ?

else()

	message(STATUS "NOTE: *gt42* targets will not be built HAVE_EXT_GLOBUS=!DEFINED")

endif (HAVE_EXT_GLOBUS)