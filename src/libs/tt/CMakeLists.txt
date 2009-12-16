##############
if (HAVE_EXT_POSTGRESQL)

	file( GLOB ttLibSrcs condor_ttdb* jobqueuecollection* )

	condor_glob( ttHdrs ttSrcs "${ttLibSrcs}" )

	add_library( tt STATIC ${ttLibSrcs} )

	condor_exe(quill "${ttHdrs};${ttSrcs}" sbin "tt;${PCRE_FOUND};${CONDOR_LIBS};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND};${POSTGRESQL_FOUND}")

endif()