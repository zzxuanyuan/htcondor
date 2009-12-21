
condor_glob(kbdHdrs kbdSrcs "")

condor_exe( condor_kbdd "${kbdHdrs};${kbdSrcs}" sbin "${PCRE_FOUND};${CONDOR_LIBS};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND};${HAVE_X11}")