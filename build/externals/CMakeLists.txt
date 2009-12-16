
##############################################################
## The following vars need to preceed the 
set (EXTERNAL_DIR ${CONDOR_SOURCE_DIR}/build/externals/install)
   		          
########################################################
## ON/OFF is used to ENABLE/DISABLE search when PROPER
condor_external(DRMAA ON "drmaa;libdrmaa" "drmaa-1.6")
#condor_external(HADOOP ON "hadoop" "hadoop-0.20.0-p2") 
condor_external(CURL ON "curl" "curl-7.19.6-p1")
condor_external(POSTGRESQL ON "pq;libpq" "postgresql-8.0.2") 
condor_external(OPENSSL ON "ssl;libeay32;ssleay32" "openssl-0.9.8l")
condor_external(KRB5 ON "krb5;comerr32.lib;gssapi32.lib;k5sprt32.lib;krb5_32.lib;xpprof32.lib" "krb5-1.4.3-p0") 
condor_external(PCRE ON "pcre;libpcre" "pcre-7.6")   
condor_external(GSOAP ON "gsoapssl++" "gsoap-2.7.10-p4")
 
if (WINDOWS)
	
	set (HAVE_SCHED_SETAFFINITY 1 PARENT_SCOPE)
	set (WANT_CLASSAD_ANALYSIS 1 PARENT_SCOPE)		
	
	set (DRMAA_FOUND "${DRMAA_DIR}/lib/libdrmaa.lib" PARENT_SCOPE)
	#set (HADOOP_FOUND "${HADOOP_DIR}/" PARENT_SCOPE)			
	set (OPENSSL_FOUND "${OPENSSL_DIR}/out32dll/libeay32.lib;${OPENSSL_DIR}/out32dll/ssleay32.lib" PARENT_SCOPE)
	set (POSTGRESQL_FOUND "${POSTGRESQL_DIR}/out32dll/libpqdll.lib" PARENT_SCOPE)
	set (KRB5_FOUND "${KRB5_DIR}/lib/comerr32.lib;${KRB5_DIR}/lib/gssapi32.lib;${KRB5_DIR}/lib/k5sprt32.lib;${KRB5_DIR}/lib/krb5_32.lib;${KRB5_DIR}/lib/xpprof32.lib" PARENT_SCOPE)
	set (PCRE_FOUND "${PCRE_DIR}/lib/libpcre.lib" PARENT_SCOPE)
	set (GSOAP_SRC "${GSOAP_DIR}/src/stdsoap2.cpp" PARENT_SCOPE)
	set (CURL_FOUND "${CURL_DIR}/lib/libcurl.lib" PARENT_SCOPE)
	
	set (EXTERNAL_INCLUDES "${DRMAA_DIR}/include;${OPENSSL_DIR}/inc32;${POSTGRESQL_DIR}/inc32;${KRB5_DIR}/include;${PCRE_DIR}/include;${CURL_DIR}/include;${GSOAP_DIR}/src" PARENT_SCOPE)
				
else()

	condor_external(BLAHP OFF "blahp" "blahp-1.12.2-p6")
	condor_external(CLASSADS ON "classad;classad_ns" "classads-1.0.4")
	condor_external(COREDUMPER ON "coredumper" "coredumper-0.2")
	condor_external(UNICOREGAHP OFF "unicoregahp" "unicoregahp-1.2.0")
	condor_external(VOMS OFF "voms" "voms-1.8.9-p2")
	condor_external(EXPAT OFF "expat" "expat-2.0.1")
	condor_external(SRB OFF "srb" "srb-3.2.1-p2")
	condor_external(GLOBUS OFF "globus" "globus-4.2.1-p4")
	condor_external(ZLIB ON "z" "zlib-1.2.3")
	condor_external(GCB OFF "gcb" "gcb-1.5.6")
	condor_external(LIBVIRT ON "virt" "libvirt-0.6.2")
	condor_external(LIBXML2 ON "xml2" "libxml2-2.7.3")
	
	# CREAM?
	# chat w/matt why this is all weird.
	# condor_external(MAN "" ON "man-current")
	# Below are TBD...	

endif(WINDOWS)


if (PROPER)

	find_program( SOAPCPP2 
				  soapcpp2
		          HINTS /usr/bin )	
		          
	set ( SOAPCPP2 ${SOAPCPP2} PARENT_SCOPE )   
		       
else()
			
	set (SOAPCPP2 "${EXTERNAL_DIR}/gsoap-2.7.10-p4/soapcpp2" PARENT_SCOPE)
			
	include_directories("${OPENSSL_DIR}/inc32")

	#if (NOT WINDOWS)
		# update the other defaults.  
	#endif()
	
	#message(STATUS "DEBUG: EXTERNAL_TARGET_DIRS=${EXTERNAL_TARGET_DIRS}")
	
	## For non-proper builds create a single target which will 
	## call all commands for make condor_externals 
	add_custom_target(
		uw_externals
		ALL
		DEPENDS ${EXTERNAL_TARGET_DIRS} )
	
	if (WINDOWS)		
		set_property(TARGET uw_externals PROPERTY InheritedPropertySheets "./external.vsprops")		
	endif()
			
endif(PROPER)
	


	
	


	