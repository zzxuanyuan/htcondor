
set(TOOL_LINK_LIBS "${CONDOR_TOOL_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND}")

condor_exe(condor_cod "cod_tool.cpp" ${C_BIN} "${TOOL_LINK_LIBS}" )

condor_exe(condor_preen "preen.cpp" ${C_BIN} "qmgmt;${TOOL_LINK_LIBS}" )

condor_exe(condor_advertise "advertise.cpp" ${C_SBIN} "${TOOL_LINK_LIBS}")

condor_exe(condor_fetchlog "fetch_log.cpp" ${C_SBIN} "${TOOL_LINK_LIBS}")

condor_exe(condor_config_val "config_val.cpp" ${C_BIN} "${TOOL_LINK_LIBS}")

condor_exe(condor_userprio "user_prio.cpp" ${C_BIN} "${TOOL_LINK_LIBS}")

condor_exe(condor_findhost "give_interactive.cpp" ${C_BIN} "${TOOL_LINK_LIBS}")

condor_exe(condor_qedit "qedit.cpp" ${C_BIN} "qmgmt;${TOOL_LINK_LIBS}")

if (HAVE_SSH_TO_JOB)
	condor_exe(condor_ssh_to_job "ssh_to_job.cpp" ${C_BIN} "${TOOL_LINK_LIBS}")
endif()

condor_exe(condor_transfer_data "transfer_data.cpp" ${C_BIN} "${TOOL_LINK_LIBS}")

condor_exe(condor_version "version.cpp" ${C_BIN} "${TOOL_LINK_LIBS}")

condor_exe(condor_wait "wait.cpp" ${C_BIN} "${TOOL_LINK_LIBS}")

if (HAVE_EXT_POSTGRESQL)
	condor_exe(condor_history "history.cpp" ${C_BIN} "${TOOL_LINK_LIBS};${POSTGRESQL_FOUND}")

	condor_exe(condor_load_history "load_history.cpp" ${C_BIN} "tt;${TOOL_LINK_LIBS};${POSTGRESQL_FOUND}")

	condor_exe(condor_dump_history "dump_history.cpp" ${C_BIN} "${TOOL_LINK_LIBS};${POSTGRESQL_FOUND}")

	condor_exe(condor_convert_history "convert_history.cpp" ${C_BIN} "${TOOL_LINK_LIBS};${POSTGRESQL_FOUND}")
endif()

condor_exe(condor_store_cred "store_cred_main.cpp" ${C_SBIN} "${TOOL_LINK_LIBS}")

condor_exe(condor "tool.cpp" "${C_BIN}" "${TOOL_LINK_LIBS}")
condor_exe(condor_on "tool.cpp" ${C_SBIN} "${TOOL_LINK_LIBS}")
condor_exe(condor_off "tool.cpp" ${C_SBIN} "${TOOL_LINK_LIBS}")
condor_exe(condor_checkpoint "tool.cpp" "${C_BIN}" "${TOOL_LINK_LIBS}")
condor_exe(condor_vacate "tool.cpp" "${C_BIN}" "${TOOL_LINK_LIBS}")
condor_exe(condor_restart "tool.cpp" ${C_SBIN} "${TOOL_LINK_LIBS}")
condor_exe(condor_reconfig "tool.cpp" ${C_SBIN} "${TOOL_LINK_LIBS}")
condor_exe(condor_reconfig_schedd "tool.cpp" ${C_SBIN} "${TOOL_LINK_LIBS}")
condor_exe(condor_reschedule "tool.cpp" "${C_BIN}" "${TOOL_LINK_LIBS}")
condor_exe(condor_master_off "tool.cpp" ${C_SBIN} "${TOOL_LINK_LIBS}")
condor_exe(condor_set_shutdown "tool.cpp" ${C_SBIN} "${TOOL_LINK_LIBS}")

condor_exe(condor_stats "stats.cpp" ${C_BIN} "${TOOL_LINK_LIBS}")

# what to do about the test executables..?
# there are a couple other targets which do not appear in the output too.

