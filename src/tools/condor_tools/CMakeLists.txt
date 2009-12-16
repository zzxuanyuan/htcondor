
set(TOOL_LINK_LIBS "${CONDOR_TOOL_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND}")

condor_exe(cod "cod_tool.cpp" bin "${TOOL_LINK_LIBS}" )

condor_exe(preen "preen.cpp" bin "qmgmt;${TOOL_LINK_LIBS}" )

condor_exe(advertise "advertise.cpp" sbin "${TOOL_LINK_LIBS}")

condor_exe(fetchlog "fetch_log.cpp" sbin "${TOOL_LINK_LIBS}")

condor_exe(config_val "config_val.cpp" bin "${TOOL_LINK_LIBS}")

condor_exe(userprio "user_prio.cpp" bin "${TOOL_LINK_LIBS}")

condor_exe(findhost "give_interactive.cpp" bin "${TOOL_LINK_LIBS}")

condor_exe(qedit "qedit.cpp" bin "qmgmt;${TOOL_LINK_LIBS}")

if (HAVE_SSH_TO_JOB)
	condor_exe(ssh_to_job "ssh_to_job.cpp" bin "${TOOL_LINK_LIBS}")
endif()

condor_exe(transfer_data "transfer_data.cpp" bin "${TOOL_LINK_LIBS}")

condor_exe(version "version.cpp" bin "${TOOL_LINK_LIBS}")

condor_exe(wait "wait.cpp" bin "${TOOL_LINK_LIBS}")

if (HAVE_EXT_POSTGRESQL)
	condor_exe(history "history.cpp" bin "${TOOL_LINK_LIBS};${POSTGRESQL_FOUND}")

	condor_exe(load_history "load_history.cpp" bin "tt;${TOOL_LINK_LIBS};${POSTGRESQL_FOUND}")

	condor_exe(dump_history "dump_history.cpp" bin "${TOOL_LINK_LIBS};${POSTGRESQL_FOUND}")

	condor_exe(convert_history "convert_history.cpp" bin "${TOOL_LINK_LIBS};${POSTGRESQL_FOUND}")
endif()

condor_exe(store_cred "store_cred_main.cpp" sbin "${TOOL_LINK_LIBS}")

condor_exe(condor "tool.cpp" "bin" "${TOOL_LINK_LIBS}")
condor_exe(on "tool.cpp" sbin "${TOOL_LINK_LIBS}")
condor_exe(off "tool.cpp" sbin "${TOOL_LINK_LIBS}")
condor_exe(checkpoint "tool.cpp" "bin" "${TOOL_LINK_LIBS}")
condor_exe(vacate "tool.cpp" "bin" "${TOOL_LINK_LIBS}")
condor_exe(restart "tool.cpp" sbin "${TOOL_LINK_LIBS}")
condor_exe(reconfig "tool.cpp" sbin "${TOOL_LINK_LIBS}")
condor_exe(reconfig_schedd "tool.cpp" sbin "${TOOL_LINK_LIBS}")
condor_exe(reschedule "tool.cpp" "bin" "${TOOL_LINK_LIBS}")
condor_exe(master_off "tool.cpp" sbin "${TOOL_LINK_LIBS}")
condor_exe(set_shutdown "tool.cpp" sbin "${TOOL_LINK_LIBS}")

condor_exe(stats "stats.cpp" bin "${TOOL_LINK_LIBS}")

# what to do about the test executables..?
# there are a couple other targets which do not appear in the output too.

