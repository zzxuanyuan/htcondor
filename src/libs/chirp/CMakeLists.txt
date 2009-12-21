

add_library( chirp_client STATIC chirp_client.h chirp_client.cpp )

condor_exe( condor_chirp "condor_chirp.cpp" libexec "chirp_client;${CONDOR_TOOL_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND}" )

