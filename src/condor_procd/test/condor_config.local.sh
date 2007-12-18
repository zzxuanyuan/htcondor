#
# This needs to be set correctly for this config file to work
#
export '_CONDOR_PROCD_DIR=/home/greg/Condor/V6_9-procd_test-branch/src/condor_procd'

#
# Location of controller and drone binaries
#
export '_CONDOR_PROCD_TEST_DRONE=$(PROCD_DIR)/condor_procd_test_drone'

#
# Disable periodic snapshots completely (the controller will explicitly
# tell the condor_procd when to take snapshots)
#
export '_CONDOR_PROCD_MAX_SNAPSHOT_INTERVAL=-1'
export '_CONDOR_PID_SNAPSHOT_INTERVAL=-1'

#
# Generate output in test directory
#
export '_CONDOR_LOG=$(PROCD_DIR)/test'
export '_CONDOR_PROCD_TEST_CONTROLLER_LOG=$(LOG)/ControllerLog'
export '_CONDOR_TRUNC_PROCD_TEST_CONTROLLER_LOG_ON_OPEN=True'
export '_CONDOR_PROCD_TEST_DRONE_LOG=$(LOG)/DroneLog'
export '_CONDOR_TRUNC_PROCD_TEST_DRONE_LOG_ON_OPEN=True'
export '_CONDOR_PROCD_LOG=$(LOG)/ProcLog'
export '_CONDOR_ALL_DEBUG=D_PROCFAMILY'

#
# Input file also located in test directory
#
export '_CONDOR_PROCD_TEST_INPUT=$(PROCD_DIR)/test/condor_procd_test_input'

#
# Don't clone because it confuses GDB
#
export '_CONDOR_USE_CLONE_TO_CREATE_PROCESSES=False'
