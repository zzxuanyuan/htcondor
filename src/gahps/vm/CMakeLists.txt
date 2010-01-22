
file( GLOB RmvSrcs *_t.cpp )
condor_glob( HeaderFiles SourceFiles "${RmvSrcs}" )

condor_exe( condor_vm-gahp "${HeaderFiles};${SourceFiles}" ${C_SBIN} "${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${LIBVIRT_FOUND};${COREDUMPER_FOUND}")

message(STATUS "TODO: add other VM_elements to release.")