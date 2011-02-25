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



############################################################### 
# Default condor_config value for all platform
#
# Note: Varaible name is the same as in condor_config file
###############################################################


set( CONDOR_HOST            central-manager-hostname.your.domain )
set( RELEASE_DIR            /usr/local/condor )
set( LOCAL_DIR              $(TILDE) )
set( LOCAL_CONFIG_FILE      $(LOCAL_DIR)/condor_config.local )
set( LOCAL_CONFIG_DIR       $(LOCAL_DIR)/config )

set( COLLECTOR_NAME         "My Pool" )

set( CONDOR_ADMIN           condor-admin@your.domain )
set( MAIL                   /usr/bin/mail )
set( UID_DOMAIN             your.domain )
set( FILESYSTEM_DOMAIN      your.domain )
set( ALLOW_READ             *  )
set( ALLOW_WRITE            YOU_MUST_CHANGE_THIS_INVALID_CONDOR_CONFIGURATION_VALUE )
set( ALLOW_ADMINISTRATOR    $(CONDOR_HOST) )

set( JAVA                   /usr/bin/java )

set( WANT_SUSPEND           $(UWCS_WANT_SUSPEND) )
set( WANT_VACATE            $(UWCS_WANT_VACATE) )
set( START                  $(UWCS_START) )
set( SUSPEND                $(UWCS_SUSPEND) )
set( CONTINUE               $(UWCS_CONTINUE) )
set( PREEMPT                $(UWCS_PREEMPT) )


set( LOCK                   $(LOG) )
set( LOG                    $(LOCAL_DIR)/log )
set( SPOOL                  $(LOCAL_DIR)/spool )
set( EXECUTE                $(LOCAL_DIR)/execute )
set( LIB                    $(RELEASE_DIR)/lib )
set( INCLUDE                $(RELEASE_DIR)/include )
set( SBIN                   $(RELEASE_DIR)/sbin )
set( LIBEXEC                $(RELEASE_DIR)/libexec )

set( DAEMON_LIST            "MASTER, STARTD, SCHEDD" )

set( QUEUE_SUPER_USERS      "root, condor" )

set( JAVA_MAXHEAP_ARGUMENT       -Xmx )
set( JAVA_CLASSPATH_SEPARATOR    : )

set( C_GAHP_LOG                  /tmp/CGAHPLog.$(USERNAME) )
set( C_GAHP_LOCK                 /tmp/CGAHPLock.$(USERNAME) )
set( C_GAHP_WORKER_THREAD_LOG    /tmp/CGAHPWorkerLog.$(USERNAME) )
set( C_GAHP_WORKER_THREAD_LOCK   /tmp/CGAHPWorkerLock.$(USERNAME) )

set( VM_GAHP_LOG            $(LOG)/VMGahpLog )

set( PROCD_ADDRESS          $(LOCK)/procd_pipe )

set( HDFS_HOME              $(RELEASE_DIR)/libexec/hdfs )
set( HDFS_NODETYPE          HDFS_DATANODE )
set( HDFS_NAMENODE          hdfs://example.com:9000 )
set( HDFS_NAMENODE_WEB      example.com:8000 )
set( HDFS_NAMENODE_DIR      /tmp/hadoop_name )
set( HDFS_DATANODE_DIR      /scratch/tmp/hadoop_data )

#Binary extension (blank for UNIX platfrom
set( EXE                    "" )

###############################################################


if (NOT WINDOWS)

	install(FILES		condor.sysconfig
			DESTINATION	"${C_SYSCONFIG}"
			PERMISSIONS ${CONDOR_SCRIPT_PERMS}
			RENAME		condor)

	install(FILES		condor_config.local.generic
			DESTINATION	"${C_ETC}"
			RENAME		condor_config.local)

	install (FILES
			condor.boot.generic
			condor_config.local.central.manager
			condor_config.local.ha_condor_client
			#condor.sysconfig
			README
			#condor.boot.rpm
			condor_config.local.ckpt.server
			condor_config.local.ha_condor_had
			lamscript
			condor_config.generic
			condor_config.local.dedicated.resource
			condor_config.local.view_server
			mp1script
			#condor_config.generic.debian.patch
			condor_config.local.dedicated.submit
			condor_config.submit.generic
			mp2script
			#condor_config.generic.rpm.patch
			#condor_config.local.generic
			condor.init
			openmpiscript
			DESTINATION ${C_ETC_EXAMPLES})

	install (FILES condor_limits_wrapper.sh DESTINATION ${C_LIBEXEC} PERMISSIONS ${CONDOR_SCRIPT_PERMS})
	
	#Platfrom specific overrides for packaging
	if ( CONDOR_PACKAGE_BUILD )

		set( CONDOR_HOST            $(FULL_HOSTNAME) )
		set( RELEASE_DIR            /usr )
		set( LOCAL_DIR              /var )
		set( LOCAL_CONFIG_FILE      /etc/condor/condor_config.local )
		set( LOCAL_CONFIG_DIR       /etc/condor/config.d )

		set( CONDOR_ADMIN           root@$(FULL_HOSTNAME) )
		set( MAIL                   /bin/mail )
		set( UID_DOMAIN             $(FULL_HOSTNAME) )
		set( FILESYSTEM_DOMAIN      $(FULL_HOSTNAME) )
		set( ALLOW_WRITE            $(FULL_HOSTNAME) )

		set( LOCK                   $(LOCAL_DIR)/lock/condor )
		
		#Run is not standard Condor param, so its value also include param name
		set( RUN                    "RUN		= $(LOCAL_DIR)/run/condor" )
		set( LOG                    $(LOCAL_DIR)/log/condor )
		set( SPOOL                  $(LOCAL_DIR)/lib/condor/spool )
		set( EXECUTE                $(LOCAL_DIR)/lib/condor/execute )
		set( LIB                    $(RELEASE_DIR)/lib/condor )
		set( INCLUDE                $(RELEASE_DIR)/include/condor )
		set( LIBEXEC                $(RELEASE_DIR)/libexec/condor )

		set( JAVA_MAXHEAP_ARGUMENT  "" )
		set( PROCD_ADDRESS          $(RUN)/procd_pipe )

		if ( RPM_SYSTEM_NAME )
		
			if (${BIT_MODE} MATCHES "32" OR ${SYS_ARCH} MATCHES "IA64" )	
				#Overrides for 32-bit

			else()
				#Overrides for 64-bit		
				set( LIB                    $(RELEASE_DIR)/lib64/condor )
			endif ()					
			
			install(FILES		condor.boot.rpm
					DESTINATION	"${C_INIT}"
					PERMISSIONS ${CONDOR_SCRIPT_PERMS}
					RENAME		condor)

		elseif ( ${LINUX_NAME} STREQUAL "Debian" )

			#Overrides for debian
			set( LIBEXEC                $(RELEASE_DIR)/lib/condor/libexec )		

			#Debian init script need to be patched because /etc/sysconfig/condor -> /etc/default/condor
			add_custom_command( OUTPUT condor.boot.debian
								WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
								COMMAND patch 
								ARGS condor.boot.rpm condor.boot.debian.patch -o condor.boot.debian )

			add_custom_target(	patch_init_script
								ALL
								DEPENDS condor.boot.debian)

			install(FILES		condor.boot.debian
					DESTINATION	"${C_INIT}"
					PERMISSIONS ${CONDOR_SCRIPT_PERMS}
					RENAME	condor)
		else()
			#Other Platform
			

			install(FILES		condor.boot.rpm
					DESTINATION	"${C_INIT}"
					PERMISSIONS ${CONDOR_SCRIPT_PERMS}
					RENAME		condor)
		endif()

	endif( CONDOR_PACKAGE_BUILD )

	configure_file(condor_config.generic.in condor_config.patched @ONLY)

	install(FILES	condor_config.patched
					DESTINATION	"${C_ETC}"
					RENAME	condor_config)

#NOT WINDOWS
else()
	

	#Add variable overrides for windows here
	
	set ( EXE .exe )
	
	set( CONDOR_HOST			$(IP_ADDRESS) )
	set( RELEASE_DIR			"c:\\condor" )
	set( LOCAL_DIR              $(RELEASE_DIR) )
	
	set( MAIL                   $(BIN)/condor_mail.exe )
	set( MAIL_FROM              "MAIL_FROM=#MAIL_FROM_VALUE#" )
	set( SMTP_SERVER            "SMTP_SERVER=#SMTP_SERVER_VALUE#" )
	set( UID_DOMAIN             $(IP_ADDRESS) )
	set (FILESYSTEM_DOMAIN		$(IP_ADDRESS) )
	
	set( JAVA                   "#JAVA_VALUE#" )
	set( JAVA_CLASSPATH_SEPARATOR    ; )
	
	set( LOCK                   $(LOG) )
	set( LIB                    $(BIN) )
	set( INCLUDE                $(RELEASE_DIR)/include )
	set( SBIN                   $(BIN) )
	set( LIBEXEC                $(BIN) )
	
	set( QUEUE_SUPER_USERS      condor, SYSTEM )

	set( C_GAHP_LOG                  NUL )
	set( C_GAHP_LOCK                 NUL )
	set( C_GAHP_WORKER_THREAD_LOG    NUL )
	set( C_GAHP_WORKER_THREAD_LOCK   NUL )

	set( VM_GAHP_LOG            NUL )

	set( PROCD_ADDRESS          "\\\\.\\pipe\\condor_procd_pipe" )
	
	set( HDFS_HOME              $(RELEASE_DIR)\\hdfs )
	set( HDFS_NAMENODE_DIR      $(HDFS_HOME)\\hadoop_name )
	set( HDFS_DATANODE_DIR      $(HDFS_HOME)\\hadoop_data )
	
	configure_file(condor_config.generic.in condor_config.patched @ONLY)

	set( CONDOR_HOST            "#CONDOR_HOST_VALUE#" )
	set( RELEASE_DIR            "#RELEASE_DIR_VALUE#" )

	set( COLLECTOR_NAME         "#COLLECTOR_NAME_VALUE#" )

	set( CONDOR_ADMIN           "#CONDOR_ADMIN_VALUE#" )

	set( UID_DOMAIN             "#UID_DOMAIN_VALUE#" )
	set( ALLOW_READ             "#ALLOW_READ_VALUE#"  )
	set( ALLOW_WRITE            "#ALLOW_WRITE_VALUE#" )
	set( ALLOW_ADMINISTRATOR    "#ALLOW_ADMINISTRATOR_VALUE#" )

	set( WANT_SUSPEND           "#WANT_SUSPEND_VALUE#" )
	set( WANT_VACATE            "#WANT_VACATE_VALUE#" )
	set( START                  "#START_VALUE#" )
	set( SUSPEND                "#SUSPEND_VALUE#" )
	set( CONTINUE               "#CONTINUE_VALUE#" )
	set( PREEMPT                "#PREEMPT_VALUE#" )

	set( DAEMON_LIST            "#DAEMON_LIST_VALUE#" )

	set( HDFS_NODETYPE          "#HDFS_NODETYPE_VALUE#" )
	set( HDFS_NAMENODE          "#HDFS_NAMENODE_VALUE#" )
	set( HDFS_NAMENODE_WEB      "#HDFS_NAMENODE_WEB_VALUE#" )

	configure_file(condor_config.generic.in condor_config.template @ONLY)
			
	install(FILES
			${CMAKE_CURRENT_BINARY_DIR}/condor_config.template
			DESTINATION	"${C_ETC}")
			
	install(FILES
			${CMAKE_CURRENT_BINARY_DIR}/condor_config.patched
			DESTINATION	"${C_ETC}"
			RENAME condor_config.generic)
			
	install(FILES
	        condor_config.power
	        condor_config.submit.generic
	        condor_config.local.generic
	        condor_config.local.credd 
	        condor_config.local.central.manager
	        convert_config_to_win32.awk
			DESTINATION	"${C_ETC}")


endif(NOT WINDOWS)
