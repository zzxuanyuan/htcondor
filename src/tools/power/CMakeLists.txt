condor_exe( condor_power "power.cpp" bin "${CONDOR_TOOL_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${CLASSADS_FOUND};${COREDUMPER_FOUND}")

condor_exe( condor_power_state "power_state.cpp" libexec "${CONDOR_TOOL_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${CLASSADS_FOUND};${COREDUMPER_FOUND}")

file( GLOB ROOSTER rooster* )

condor_exe( condor_rooster "${ROOSTER}" libexec "${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${CLASSADS_FOUND};${COREDUMPER_FOUND}")