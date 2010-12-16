 ###############################################################
 #
 # Copyright (C) 1990-2010, Redhat.
 #
 # Licensed under the Apache License, Version 2.0 (the "License"); you
 # may not use this file except in compliance with the License.  You may
 # obtain a copy of the License at
 #
 #    http://www.apache.org/licenses/LICENSE-2.0
 #
 # Unless required by applicable law or agreed to in writing, software
 # distributed under the License is distributed on an "AS IS" BASIS,
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 # See the License for the specific language governing permissions and
 # limitations under the License.
 #
 ###############################################################

if(LINUX AND HAVE_EXT_BOOST)

	condor_glob( HeaderFiles SourceFiles "" )
	condor_exe( condor_vm-gahp2 "${HeaderFiles};${SourceFiles}" ${C_SBIN} "${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${LIBVIRT_FOUND};${LIBXML2_FOUND};${COREDUMPER_FOUND};${ZLIB_FOUND};-lboost_program_options")

else()

    message(STATUS "NOTE: *vmgahp2* targets will not be built LINUX=${LINUX} & BOOST_FOUND=${BOOST_FOUND}")

endif()
