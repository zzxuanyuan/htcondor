

if (WANT_LEASE_MANAGER AND HAVE_EXT_CLASSADS)

	file( GLOB rmvLMFiles *test*)

	condor_glob(LMHdrs LMSrcs "${rmvLMFiles}*")

	condor_exe( condor_lease_manager "${LMHdrs};${LMSrcs}" sbin "conversion;${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${CLASSADS_FOUND};${COREDUMPER_FOUND}")

else()

	message(STATUS "NOTE: *lease_manager* targets will not be built WANT_LEASE_MANAGER=!DEFINED OR HAVE_EXT_CLASSADS=!DEFINED")

endif()