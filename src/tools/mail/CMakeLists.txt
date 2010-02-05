if(WINDOWS)

	# Will have to determine if there is an easy way to create managed C++ projects.
	# condor_glob(MailHdrs MailSrcs "")
	# condor_exe( condor_mail "${MailHdrs};${MailSrcs}" ${C_BIN} "${CONDOR_LIBS}")
	
	include_external_msproject(condor_mail ${CONDOR_SOURCE_DIR}/src/tools/mail/condor_mail.vcproj )

endif(WINDOWS)