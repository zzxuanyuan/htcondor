file( GLOB HADSrcs StateMachine.* HAD.* Utils.* )

condor_exe(condor_had "${HADSrcs}" sbin "${PCRE_FOUND};${CONDOR_LIBS};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}")

file( GLOB RepSrcs AbstractReplicatorStateMachine.* ReplicatorStateMachine.* HAD_Version.* Utils.* Replication.* FilesOperations.* )

condor_exe(condor_replication "${RepSrcs}" sbin "${PCRE_FOUND};${CONDOR_LIBS};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}")

file( GLOB TransferSrcs BaseReplicaTransferer.* UploadReplicaTransferer.* DownloadReplicaTransferer.* Utils.* Transferer.* FilesOperations.* )

condor_exe(condor_transferer "${TransferSrcs}" sbin "${PCRE_FOUND};${CONDOR_LIBS};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}")