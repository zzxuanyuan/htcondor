/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "master.h"
#include "condor_daemon_core.h"
#include "string_list.h"
#include "master_test.h"

// arbitrary good return value
#define PASSED_TESTS 195

// these are defined in master.cpp
extern void		init_params();
extern void		init_daemon_list();
extern int		admin_command_handler(Service *, int, Stream *);
extern void		main_config();
extern int		master_exit(int retval);

extern int 		condor_main_argc;
extern char 	**condor_main_argv;
extern int		check_new_exec_interval;
extern int		new_bin_delay;
extern Daemons 	daemons;
extern int		master_backoff_constant;
extern int		master_backoff_ceiling;
extern float	master_backoff_factor;
extern int		master_recover_time;
extern int		shutdown_graceful_timeout;
extern int		shutdown_fast_timeout;


/*

Starts testing at start_test_num

Note: Can start testing at any given test, however some tests don't really have
any effect unless they are run immediately after the test before them. Therefore
tests may not actually test anything or may even fail if the previous test is
not run before it.

Starting test numbers of 0, 50, 100 are good start points.

 */
TestMaster::TestMaster(int start_test_num) {
	timer_id = 0;
	local_test_num = 0;
	global_test_num = start_test_num;
	retry_num = 0;
	saved_time = 0;
	saved_start_time = 0;
	saved_num_children = -1;
	saved_daemon_list = NULL;
	saved_next_start = 0;
	initialized = false;
	setup_success = false;
	next_start_offset = 1;

	// Get some stuff from our config file
	default_period = param_integer("MASTER_TEST_DEFAULT_PERIOD", 10, 1);	// CHANGE ME
	default_num_retries = param_integer("MASTER_TEST_DEFAULT_NUM_RETRIES", 6, 1); // CHANGE ME
	daemon_check = param("MASTER_TEST_DAEMON_CHECK");

	// Make sure we don't call FindDaemon(NULL)
	if(!daemon_check) {
		daemon_check = "STARTD";	// CHANGE ME
	}

	// This permits running a given test via command line vs a restart of the
	// master (don't perform test setup to start)
	if(start_test_num < 0) {
		dprintf(D_ALWAYS, "In TestMaster::TestMaster()\n");
		dprintf(D_ALWAYS, "Starting at test %d\n", -start_test_num);
		global_test_num = -start_test_num;
		setup_success = true;
	}
}

// Just delete our StringList
TestMaster::~TestMaster() {
	delete saved_daemon_list;
}

void TestMaster::start_tests() {
	timer_id = daemonCore->Register_Timer(default_period, default_period, (TimerHandlercpp)&TestMaster::test_main, "TestMaster::test_main", this);
	if(timer_id == -1)
		cleanup(-1);
}

bool TestMaster::setup() {
	// We don't want the the new exec timers interfering with us
	// FIX ME: This doesn't appear to work all the time
	daemons.CancelNewExecTimer();
	check_new_exec_interval = 0;

	saved_time = time(NULL);
	saved_num_children = daemons.NumberOfChildren();
	saved_daemon_list = new StringList(daemons.ordered_daemon_names);
	return (saved_daemon_list != NULL);
}

/*
	Test Descriptions
    ----------------------------------------------------------------------------

	-- Daemon class Tests --
0 - FindDaemon(), RegisterDaemon(), DaemonLog(), StopDaemon()
		check if daemons started
	1 - CheckForNewExecutable()
		set immediate_restart to false, new bin delay to default period/2
		update timestamps on non-master daemons, check for new execs
		check if daemons are started, non-master daemons restarted
	2 - CheckForNewExecutable()
		set immediate_restart to true, new bin delay to max
		update timestamps on non-master daemons, check for new execs
		check if daemons are started, non-master daemons restarted
	3 - StopAllDaemons()
		stop all daemons
		check if daemons are stopped
	4 - StartAllDaemons()
		start all daemons (1)
		check if daemons are started
	5 - StopFastAllDaemons()
		stop fast all daemons
		check if daemons are stopped
	6 - StartAllDaemons()
		start all daemons (2)
		check if daemons are started
7 - StopDaemon()
		stop daemons individually
		check if daemons are stopped
	8 - StartAllDaemons()
		init daemon list, setup controllers, init daemons params
		start all daemons
		check if daemons are started
	9 - HardKillAllDaemons()
		hard kill all daemons
		check if daemons are stopped
10- StartAllDaemons()
		start all daemons, stop all daemons, start all daemons
		check if daemons are started
	11- StopPeacefulAllDaemons()
		stop peaceful all daemons
		check if daemons are stopped
	12- DaemonsOn()
		start all daemons, daemons on
		check if daemons are started
	13- DaemonsOff()
		daemons off
		check if daemons are stopped
	14- DaemonsOn()
		daemons on (1)
		check if daemons are started
	15- DaemonsOff()
		daemons off fast
		check if daemons are stopped
	16- DaemonsOn()
		daemons on (2)
		check if daemons are started
	17- DaemonsOffPeaceful()
		daemons off peaceful
		check if daemons are stopped
18- DaemonsOn()
		daemons on, daemons off, daemons on
		check if daemons are started
	19- RestartMaster()
		restart master (1)
		check if daemons are started, master restarted
	20- RestartMaster()
		restart master (2)
		check if daemons are started, master restarted
	21- RestartMaster()
		set immediate restart to false, new_bin_delay to INT_MAX, restart master
		check if daemons are stopped, master didn't restart
	22- RestartMaster()
		set immediate restart to true, restart master
		check if daemons are started, master restarted
	23- RestartMasterPeaceful()
		restart master peaceful
		check if daemons are started, master restarted
	24- CheckForNewExecutable()
		update timestamp on master, check for new exec
		check if daemons are started, master restarted
	25- CheckForNewExecutable()
		stop all daemons
		update timestamps on all non-master daemons, check for new exec
		check if daemons are stopped
	26- StartAllDaemons()
		start all daemons
		check if daemons are started
	27- CheckForNewExecutable()
		stop all daemons, update timestamps on all non-master daemons
		set immediate restart to true, check for new exec
		check if daemons stopped
	28- StartAllDaemons()
		start all daemons
		check if daemons are started
	29- ReconfigAllDaemons()
		start all daemons, reconfig all daemons
		check if daemons are started
	30- AllReaper()
		call AllReaper with invalid pid
		check if all daemons started
	31- DefaultReaper()
		call DefaultReaper() with invalid pid
		check if all daemons started
	more
		exit_allowed
		CancelRetryStartAllDaemons() - need to have wait before starting other daemons

	-- Basic Tests (daemon class) --
	50- check if daemons started
	51- backoff factor
		set restarts to 0, recover time to max, stop/restart daemon
		check if daemon restarted, check start time vs saved next start
	52- backoff factor
		set restarts to 1, recover time to max, stop/restart daemon
		check if daemon restarted, check start time vs saved next start
	53- recover time
		set restarts to 2, recover time default_period/4
		stop/restart daemon
		check if daemon started, check start time vs saved next start
	54- backoff ceiling
		set restarts to 100, recover time to max, ceiling to default_period/2
		stop/restart daemon
		check if daemon restarted, check start time vs saved next start
	55- Hold(on)
		put daemon on hold
		check if daemon is on hold
	56- Hold(on)
		start daemon, update timestamp, check for new exec
		check if daemon is on hold
	57- Hold(off)
		take daemon off hold
		check if daemon is not on hold
	58- Hold(on)
		put daemon on hold
		check if daemon is on hold
	59- Hold(on)
		put daemon on hold, stop daemon
		check if daemon is on hold, stopped
	60- Hold(on)
		put daemon on hold, start daemon
		check if daemon is on hold, stopped
	61- Hold(off)
		take daemon off hold, restart daemon
		check if daemon restarted, check start time vs saved next start
62- CancelRestartTimers()
		phase 0:set restarts to 0, backoff constant to default period,
				reconfig, stop daemon
		phase 1:cancel restart timers
		check if daemon is stopped, restarts is 0
	63- Start()
		start daemon
		check if daemons started, restarts is 0
	64- Kill() - SIGTERM
		kill daemon
		check if daemon is started, restarts is 1
	65- Kill() - SIGQUIT
		set restarts to 0, kill daemon
		check if daemon is started, restarts is 1
	more-
		waitbeforestartingotherdaemons
			COLLECTOR_ADDRESS_FILE
		onlystopwhenmasterstops
			SHARED_PORT daemon only

	-- Command Handler Tests --
	100 - check if daemons started
	101-call RESTART command
		check if all daemons started, master restarted
	102-call RESTART_PEACEFUL command
		check if all daemons started, master restarted
	103-call DAEMONS_OFF command
		check if all daemons are stopped
	104-call DAEMONS_ON command (1)
		check if all daemons are started
	105-call DAEMONS_OFF_FAST command
		check if all daemons are stopped
	106-call DAEMONS_ON command (2)
		check if all daemons are started
	107-call DAEMONS_OFF_PEACEFUL command
		check if all daemons are stopped
108-call DAEMONS_ON command, call DAEMONS_OFF, call DAEMONS_ON
		check if all daemons are started
	109-call DAEMON_OFF command
		check if daemon is off
	110-call DAEMON_ON command (1)
		check if daemon is on
	111-call DAEMON_OFF_FAST command
		check if daemon is off
	112-call DAEMON_ON command (2)
		check if daemon is on
	113-call DAEMON_OFF_PEACEFUL command
		check if daemon is off
	114-call DAEMON_ON command, DAEMON_OFF command, DAEMON_ON command
		check if daemon is on
	115-call CHILD_OFF command
		check if child is off
	116-call CHILD_ON command (1)
		check if child is on
	117-call CHILD_OFF_FAST command
		check if child is off
	118-call CHILD_ON command (2)
		check if child is on
	119-call DAEMONS_OFF command
		call main_config
		check if daemons are stopped
	120-call DAEMONS_ON command
		call main_config
		check if daemons are started
	more-

	Other:
	- ouput what each test is doing
	- use multiple config files
	- change period between test_main() calls based on test


	Description
    ----------------------------------------------------------------------------
	This is the function that is continually called after registering the timer
	A normal call consists of:
		test_check() (finish current test)
		increment test numbers
		test_setup() (next test)
 */
void TestMaster::test_main() {
	// Re-init state that we may have changed
	init_params();
	dprintf(D_FULLDEBUG, "In TestMaster::test_main()\n");
	if(retry_num > 0)
		dprintf(D_ALWAYS, "Retry %d of %d\n", retry_num, default_num_retries);

	// Initialize state
	if(!initialized) {
		if(check_daemons_started()) {
			initialized = setup();
		} else {
			goto failure;
		}
	}

	// Check if we failed setup last time or have another phase of setup
	if(!setup_success || phase_num != 0) {
		dprintf(D_ALWAYS, "Test %d(%d) setup \n", global_test_num,
				local_test_num);
		if(phase_num != 0)
			dprintf(D_ALWAYS, "Phase number %d\n", phase_num);

		// Attempt setup again
		if((setup_success = test_setup())) {
			// if we pass setup, we want to wait before calling test_check
			retry_num = 0;
			goto end;
		} else {
			dprintf(D_ALWAYS, "Test setup failed.\n");
			goto failure;
		}
	} else if(test_check()) {
		dprintf(D_ALWAYS, "Passed test %d(%d)\n", global_test_num,
				local_test_num);
		local_test_num++;
		global_test_num++;
		phase_num = 0;

		dprintf(D_ALWAYS, "Test %d(%d)\n", global_test_num, local_test_num);
		if(!(setup_success = test_setup())) {
			dprintf(D_ALWAYS, "Test setup failed.\n");
			goto failure;
		} else {
			retry_num = 0;
		}
	} else {
failure:
		retry_num++;
		if(retry_num > default_num_retries) {
			dprintf(D_ALWAYS, "Retried %d times. Failed test %d. Exiting.\n",
					retry_num-1, global_test_num);
			cleanup(-1);
		}
	}
end:
	dprintf(D_FULLDEBUG, "End TestMaster::test_main()\n");
}

// Performs test setup based on global_test_num
bool TestMaster::test_setup() {
	dprintf(D_FULLDEBUG, "In TestMaster::test_setup()\n");
	char *daemon_name = NULL;
	daemon *d = NULL;
	bool rval = true;

	// setup next test
	switch(global_test_num) {
	case 1:
		daemons.immediate_restart = false;
		new_bin_delay = default_period/2;
		// update timestamps on all daemons beside the master
		daemons.ordered_daemon_names.rewind();
		while((daemon_name = daemons.ordered_daemon_names.next()) && rval) {
			if(strcmp(daemon_name, "MASTER") != MATCH) {
				rval &= update_timestamp(daemon_name);
			}
		}
		daemons.CheckForNewExecutable();
		break;
	case 2:
		daemons.immediate_restart = true;
		new_bin_delay = INT_MAX;
		saved_time = time(NULL);
		// update timestamps on all daemons beside the master
		daemons.ordered_daemon_names.rewind();
		while((daemon_name = daemons.ordered_daemon_names.next()) && rval) {
			if(strcmp(daemon_name, "MASTER") != MATCH) {
				rval &= update_timestamp(daemon_name);
			}
		}
		daemons.CheckForNewExecutable();
		break;
	case 3:
		setup_quick_stop(default_period);
		daemons.StopAllDaemons();
		break;
	case 6:
		daemons.StartAllDaemons();
	case 4:
		daemons.StartAllDaemons();
		break;
	case 5:
		setup_quick_stop(default_period);
		daemons.StopFastAllDaemons();
		break;
	case 7:
		setup_quick_stop(default_period);
		daemons.ordered_daemon_names.rewind();
		while((daemon_name = daemons.ordered_daemon_names.next())) {
			// TODO: this doesn't stop master, but still erases daemon* from
			// list
			daemons.StopDaemon(daemon_name);
		}
		break;
	case 8:
		init_daemon_list();
		rval = (daemons.SetupControllers() == 0);
		daemons.InitParams();
		daemons.InitMaster();	// REMOVE ME (See above)
		daemons.StartAllDaemons();
		break;
	case 9:
		daemons.HardKillAllDaemons();
		break;
	case 10:
		daemons.StartAllDaemons();
		// TODO: this fails
		//daemons.StopAllDaemons();
		//daemons.StartAllDaemons();	// daemons still running here
		break;
	case 11:
		daemons.StopPeacefulAllDaemons();
		break;
	case 12:
		daemons.StartAllDaemons();
		daemons.DaemonsOn();
		break;
	case 13:
		setup_quick_stop(default_period);
		daemons.DaemonsOff();
		break;
	case 16:
		daemons.DaemonsOn();
	case 14:
		daemons.DaemonsOn();
		break;
	case 15:
		setup_quick_stop(default_period);
		daemons.DaemonsOff(1);
		break;
	case 17:
		daemons.DaemonsOffPeaceful();
		break;
	case 18:
		daemons.DaemonsOn();
		// TODO: this fails...
		//daemons.DaemonsOff();
		//daemons.DaemonsOn();		// daemons still running here
		break;
	case 19:
		setup_quick_stop(default_period);
		rval = save_test_num_for_restart(global_test_num);
		daemons.RestartMaster();
		break;
	case 20:
		setup_quick_stop(default_period);
		rval = save_test_num_for_restart(global_test_num);
		daemons.RestartMaster();
		daemons.RestartMaster();
		break;
	case 21:
		daemons.immediate_restart = false;
		new_bin_delay = INT_MAX;
		rval = save_test_num_for_restart(global_test_num);
		daemons.RestartMaster();
		break;
	case 22:
		daemons.immediate_restart = true;
		new_bin_delay = INT_MAX;
		setup_quick_stop(default_period);
		rval = save_test_num_for_restart(global_test_num);
		daemons.RestartMaster();
		break;
	case 23:
		setup_quick_stop(default_period);
		rval = save_test_num_for_restart(global_test_num);
		daemons.RestartMasterPeaceful();
		break;
	case 24:
		setup_quick_stop(default_period);
		rval = save_test_num_for_restart(global_test_num) && 
				update_timestamp("MASTER");
		daemons.CheckForNewExecutable();
		break;
	case 27:
		daemons.immediate_restart = true;
	case 25:
		setup_quick_stop(default_period);
		daemons.StopAllDaemons();
		saved_daemon_list->rewind();
		while((daemon_name = saved_daemon_list->next())) {
			if(strcmp(daemon_name, "MASTER") != MATCH) {
				rval &= update_timestamp(daemon_name);
			}
		}
		daemons.CheckForNewExecutable();
		break;
	case 26:
	case 28:
		daemons.StartAllDaemons();
		break;
	case 29:
		daemons.StartAllDaemons();
		setup_quick_stop(default_period);
		daemons.ReconfigAllDaemons();
		break;
	case 30:
		setup_quick_stop(default_period);
		daemons.AllReaper(-1, SIGTERM);
		break;
	case 31:
		setup_quick_stop(default_period);
		daemons.DefaultReaper(-1, SIGTERM);
		break;
	case 51:
	case 52:
		if((rval = (d = daemons.FindDaemon(daemon_check)))) {
			master_recover_time = INT_MAX;
			setup_quick_restart(default_period, global_test_num-51);
			d->restarts = ((global_test_num-51));
			d->Reconfig();
			saved_next_start = time(NULL) + d->NextStart();
			saved_start_time = d->startTime;
			d->Stop();	// This will restart daemon
		}
		break;
	case 53:
		if((rval = (d = daemons.FindDaemon(daemon_check)))) {
			master_recover_time = default_period/4;
			setup_quick_restart(default_period, 2);
			d->restarts = 2;
			d->Reconfig();
			saved_next_start = time(NULL) + d->NextStart();
			saved_start_time = d->startTime;
			d->Stop();	// This will restart daemon
		}
		break;
	case 54:
		if((rval = (d = daemons.FindDaemon(daemon_check)))) {
			master_recover_time = INT_MAX;
			master_backoff_ceiling = default_period/2;
			setup_quick_stop(default_period);
			d->restarts = 100;
			d->Reconfig();
			saved_next_start = time(NULL) + d->NextStart();
			saved_start_time = d->startTime;
			d->Stop();	// This will restart daemon
		}
		break;
	case 55:
		if((rval = (d = daemons.FindDaemon(daemon_check)))) {
			d->Hold(true);
		}
		break;
	case 56:
		if((rval = (d = daemons.FindDaemon(daemon_check)))) {
			d->Start();
			rval = update_timestamp(daemon_check);
			daemons.CheckForNewExecutable();
		}
		break;
	case 57:
		if((rval = (d = daemons.FindDaemon(daemon_check)))) {
			d->Hold(false);
		}
		break;
	case 58:
		if((rval = (d = daemons.FindDaemon(daemon_check)))) {
			d->Hold(true);
		}
		break;
	case 59:
		if((rval = (d = daemons.FindDaemon(daemon_check)))) {
			d->Hold(true);
			setup_quick_restart(default_period, 0);
			d->Reconfig();
			d->restarts = 0;
			d->Stop();
		}
		break;
	case 60:
		if((rval = (d = daemons.FindDaemon(daemon_check)))) {
			d->Hold(true);
			d->Start();
		}
		break;
	case 61:
		if((rval = (d = daemons.FindDaemon(daemon_check)))) {
			setup_quick_restart(default_period, 0);
			d->Reconfig();
			d->restarts = 0;
			d->Hold(false);
			saved_next_start = time(NULL) + d->NextStart();
			saved_start_time = d->startTime;
			d->Restart();
		}
		break;
	case 62:
		if((rval = (d = daemons.FindDaemon(daemon_check)))) {
			if(phase_num == 0) {
				setup_quick_stop(default_period);
				master_backoff_constant = default_period+1;
				d->Reconfig();
				d->restarts = 0;
				d->Stop();
				phase_num++;
			} else {
				// TODO: this doesn't change number of restarts
				d->CancelRestartTimers();
				d->restarts = 0;	// REMOVE ME
				phase_num = 0;
			}
		}
		break;
	case 63:
		if((rval = (d = daemons.FindDaemon(daemon_check)))) {
			d->Start();
		}
		break;
	case 64:
		if((rval = (d = daemons.FindDaemon(daemon_check)))) {
			setup_quick_restart(default_period, 0);
			d->Reconfig();
			saved_next_start = time(NULL) + d->NextStart();
			saved_start_time = d->startTime;
			d->Kill(SIGTERM);
		}
		break;
	case 65:
		if((rval = (d = daemons.FindDaemon(daemon_check)))) {
			setup_quick_restart(default_period, 0);
			d->restarts = 0;
			d->Reconfig();
			saved_next_start = time(NULL) + d->NextStart();
			saved_start_time = d->startTime;
			d->Kill(SIGQUIT);
		}
		break;
	case 101:
		setup_quick_stop(default_period);
		rval = save_test_num_for_restart(global_test_num) &&
				call_command_handler(RESTART);
		break;
	case 102:
		setup_quick_stop(default_period);
		rval = save_test_num_for_restart(global_test_num) &&
				call_command_handler(RESTART_PEACEFUL);
		break;
	case 103:
		setup_quick_stop(default_period);
		rval = call_command_handler(DAEMONS_OFF);
		break;
	case 104:
		rval = call_command_handler(DAEMONS_ON);
		break;
	case 105:
		setup_quick_stop(default_period);
		rval = call_command_handler(DAEMONS_OFF_FAST);
		break;
	case 106:
		rval = call_command_handler(DAEMONS_ON) &&
				call_command_handler(DAEMONS_ON);
		break;
	case 107:
		setup_quick_stop(default_period);
		rval = call_command_handler(DAEMONS_OFF_PEACEFUL);
		break;
	case 108:
		rval = call_command_handler(DAEMONS_ON);
		// TODO: this fails
		//call_command_handler(DAEMONS_OFF);
		//call_command_handler(DAEMONS_ON);	// daemons still on here
		break;
	case 109:
		setup_quick_stop(default_period);
		rval = call_command_handler(DAEMON_OFF, daemon_check);
		break;
	case 110:
		rval = call_command_handler(DAEMON_ON, daemon_check);
		break;
	case 111:
		setup_quick_stop(default_period);
		rval = call_command_handler(DAEMON_OFF_FAST, daemon_check);
		break;
	case 112:
		rval = call_command_handler(DAEMON_ON, daemon_check) &&
				call_command_handler(DAEMON_ON, daemon_check);
		break;
	case 113:
		setup_quick_stop(default_period);
		rval = call_command_handler(DAEMON_OFF_PEACEFUL, daemon_check);
		break;
	case 114:
		rval = call_command_handler(DAEMON_ON, daemon_check) &&
				call_command_handler(DAEMON_OFF, daemon_check) &&
				call_command_handler(DAEMON_ON, daemon_check);
		break;
	case 115:
		setup_quick_stop(default_period);
		rval = call_command_handler(CHILD_OFF, daemon_check);
		break;
	case 116:
		rval = call_command_handler(CHILD_ON, daemon_check);
		break;
	case 117:
		setup_quick_stop(default_period);
		rval = call_command_handler(CHILD_OFF_FAST, daemon_check);
		break;
	case 118:
		rval = call_command_handler(CHILD_ON, daemon_check) &&
				call_command_handler(CHILD_ON, daemon_check);
		break;
	case 119:
		setup_quick_stop(default_period);
		rval = call_command_handler(DAEMONS_OFF);
		main_config();
		break;
	case 120:
		setup_quick_stop(default_period);
		rval = call_command_handler(DAEMONS_ON);
		main_config();
		break;
	case 0:
	case 50:
	case 100:
		break;
	default:
		// Move to next group of tests
		if(global_test_num < 50) {
			global_test_num = 50;
			rval = save_test_num_for_restart(global_test_num);
			setup_quick_restart(default_period, 0);
			daemons.RestartMaster();
		} else if(global_test_num < 100) {
			global_test_num = 100;
			rval = save_test_num_for_restart(global_test_num);
			setup_quick_restart(default_period, 0);
			daemons.RestartMaster();
		} else {
			// All done :)
			dprintf(D_ALWAYS, "All %d tests passed. Exiting\n",
					global_test_num);
			cleanup(PASSED_TESTS);
		}
		break;
	}

	dprintf(D_FULLDEBUG, "End TestMaster::test_setup()\n");
	return rval;
}

// Performs test check based on global_test_num
bool TestMaster::test_check() {
	dprintf(D_FULLDEBUG, "In TestMaster::test_check()\n");
	bool rval = true;

	switch(global_test_num) {
	case 0:
		if(rval = check_daemons_started() && check_update(true)) {
			rval = do_basic_tests();
		}
		break;
	case 4:
	case 6:
	case 8:
	case 10:
	case 12:
	case 14:
	case 16:
	case 18:
	case 26:
	case 28:
	case 29:
	case 30:
	case 31:
	case 104:
	case 106:
	case 108:
	case 120:
		rval = check_daemons_started() && check_update(true);
		break;
	case 19:
	case 20:
	case 22:
	case 23:
	case 24:
	case 101:
	case 102:
		rval = check_daemons_started() && check_update(true) &&
				check_master_restart(true);
		break;
	case 1:
	case 2:
		rval = check_daemons_started() && check_restart_new_exec();
		break;
	case 3:
	case 5:
	case 9:
	case 11:
	case 13:
	case 15:
	case 17:
	case 25:
	case 27:
	case 103:
	case 105:
	case 107:
	case 119:
		rval = check_daemons_stopped() && check_update(false);
		break;
	case 7:
		rval = check_no_daemons();
		break;
	case 21:
		rval = check_daemons_stopped() && check_master_restart(false);
		break;
	case 51:
	case 52:
		rval = check_daemons_started() &&
				check_daemon_restart_time(daemon_check, (global_test_num-50),
					saved_next_start);
		break;
	case 53:
		rval = check_daemons_started() &&
				check_daemon_restart_time(daemon_check, 0, saved_next_start);
		break;
	case 54:
		rval = check_daemons_started() &&
				check_daemon_restart_time(daemon_check, 101, saved_next_start);
		break;
	case 55:
	case 56:
	case 58:
		rval = check_daemon_started(daemon_check) &&
				check_single_daemon_condition(HOLD, 1, daemon_check, true,
					true);
		break;
	case 57:
		rval = check_daemon_started(daemon_check) &&
				check_single_daemon_condition(HOLD, 0, daemon_check, true,
					true);
		break;
	case 59:
	case 60:
		rval = check_daemon_stopped(daemon_check) &&
				check_single_daemon_condition(HOLD, 1, daemon_check, true,
					true);
		break;
	case 61:
		rval = check_daemons_started() &&
				check_daemon_restart_time(daemon_check, 1, saved_next_start);
		break;
	case 62:
		rval = check_single_daemon_condition(PID, 0, daemon_check, true, true)
				&& check_single_daemon_condition(RESTARTS, 0, daemon_check,
					true, true);
		break;
	case 63:
		rval = check_daemons_started() &&
				check_single_daemon_condition(RESTARTS, 0, daemon_check, true,
					true);
		break;
	case 64:
	case 65:
		rval = check_daemons_started() &&
				check_daemon_restart_time(daemon_check, 1, saved_next_start);
		break;
	case 109:
	case 111:
	case 113:
	case 115:
	case 117:
		rval = check_daemon_stopped(daemon_check);
		break;
	case 110:
	case 112:
	case 114:
	case 116:
	case 118:
		rval = check_daemon_started(daemon_check);
		break;
	default:
		break;
	}

	dprintf(D_FULLDEBUG, "End TestMaster::test_check()\n");
	return rval;
}

// Do some one-time tests that don't need to wait for an action to occur
bool TestMaster::do_basic_tests() {
	dprintf(D_FULLDEBUG, "In TestMaster::do_basic_tests()\n");
	char *daemon_name = NULL, *temp = NULL, *temp2 = NULL;
	daemon *d = NULL;

	// FindDaemon()
	if(daemons.FindDaemon("ASDF1234") != NULL) {
		dprintf(D_ALWAYS, "Error: FindDaemon() returned daemon for invalid "
				"input\n");
		return false;
	}
	// TODO: this segfaults
	//if(!(rval = (daemons.FindDaemon(NULL) == NULL)))
	//	dprintf(D_ALWAYS, "Error: FindDaemon() returned daemon for NULL "
	//			"input\n");

	// RegisterDaemon()
	// TODO: this segfaults
	//daemons.RegisterDaemon(NULL);

	// DaemonLog()
	temp = daemons.DaemonLog(-1);
	saved_daemon_list->rewind();
	while((daemon_name = saved_daemon_list->next()) &&
		(d = daemons.FindDaemon(daemon_name)))
	{
		temp2 = daemons.DaemonLog(d->pid);
		if(!temp2 || strcmp(temp2, "Unknown Program!!!") == MATCH) {
			dprintf(D_ALWAYS, "Error: DaemonLog() returned invalid log name for"
					" daemon '%s'\n", daemon_name);
			return false;
		}
		if(temp && strcmp(temp, temp2) == MATCH) {
			dprintf(D_ALWAYS, "Error: DaemonLog() returned a valid log name for"
					" an invalid pid\n");
			return false;
		}
	}

	// StopDaemon();
	daemons.StopDaemon("BLAH123");
	// TODO: this seqfaults
	//daemons.StopDaemon(NULL);

	return true;
}

// Check if all the initial daemons exist
// (Use ordered_daemon_names list, Daemons::FindDaemon())
bool TestMaster::check_daemons_exist() {
	dprintf(D_FULLDEBUG, "In TestMaster::check_daemons_exist()\n");
	char *saved_daemon_name = NULL, *daemon_name = NULL;
	bool found = false;

	// Check if we can find all the known daemons
	StringList *check = &daemons.ordered_daemon_names;
	if(saved_daemon_list != NULL) {
		check = saved_daemon_list;

		check->rewind();
		while((saved_daemon_name = check->next())) {
			found = false;
			daemons.ordered_daemon_names.rewind();
			while((daemon_name = daemons.ordered_daemon_names.next()) &&
					!found)
			{
				found = (strcmp(saved_daemon_name, daemon_name) == MATCH);
			}
			if(!found) {
				dprintf(D_ALWAYS, "Error: Cannot find daemon '%s'\n",
						saved_daemon_name);
				return false;
			}
		}
	}

	check->rewind();
	while((saved_daemon_name = check->next())) {
		if(!daemons.FindDaemon(saved_daemon_name)) {
			dprintf(D_ALWAYS, "Error: Cannot find daemon '%s'\n",
					saved_daemon_name);
			return false;
		}
		if(!daemons.FindDaemon(stringToDaemonType(saved_daemon_name))) {
			dprintf(D_ALWAYS, "Error: Cannot find daemon type '%d'\n", 
					stringToDaemonType(saved_daemon_name));
			return false;
		}
	}
	
	return true;
}

// Check if given daemon is started
bool TestMaster::check_daemon_started(char *daemon_name) {
	dprintf(D_FULLDEBUG, "In TestMaster::check_daemons_started()\n");

	if(!check_daemons_exist())
		return false;

	// Make sure this daemon started
	if(!check_single_daemon_condition(START_TIME, 0, daemon_name, false) ||
			!check_single_daemon_condition(PID, 0, daemon_name, false))
	{
		return false;
	}

	return true;
}

// Check if all daemons are started
bool TestMaster::check_daemons_started() {
	dprintf(D_FULLDEBUG, "In TestMaster::check_daemons_started()\n");

	if(!check_daemons_exist())
		return false;

	// Make sure all the known daemons started
	if(!check_daemon_condition(START_TIME, 0, false) ||
			!check_daemon_condition(PID, 0, false))
	{
		return false;
	}

	if(saved_num_children != -1 &&
			daemons.NumberOfChildren() != saved_num_children)
	{
		dprintf(D_ALWAYS, "Error: Expected to have %d children, actually have "
				"%d.\n", saved_num_children, daemons.NumberOfChildren());
		return false;
	}

	return true;
}

// Check if all non-master daemons restarted
// (Check if start time is more recent than our saved time)
bool TestMaster::check_restart_new_exec() {
	dprintf(D_FULLDEBUG, "In TestMaster::check_restart_new_exec()\n");
	char *daemon_name = NULL;
	daemon *d = NULL;

	daemons.ordered_daemon_names.rewind();
	while((daemon_name = daemons.ordered_daemon_names.next())) {
		if(strcmp(daemon_name, "MASTER") != MATCH &&
				(d = daemons.FindDaemon(daemon_name)))
		{
			if(d->startTime <= saved_time) {
				dprintf(D_ALWAYS, "Error: Daemon '%s' didn't restart\n",
						daemon_name);
				return false;
			}
		}
	}
	return true;
}

// Check if given daemon is stopped
bool TestMaster::check_daemon_stopped(char *daemon_name) {
	dprintf(D_FULLDEBUG, "In TestMaster::check_daemons_stopped()\n");

	if(!check_daemons_exist())
		return false;

	// Make sure all the known daemons are stopped
	if(!check_single_daemon_condition(PID, 0, daemon_name, true))
		return false;

	return true;
}

// Check if all non-master daemons are stopped
bool TestMaster::check_daemons_stopped() {
	dprintf(D_FULLDEBUG, "In TestMaster::check_daemons_stopped()\n");

	if(!check_daemons_exist())
		return false;

	// Make sure all the known daemons are stopped
	if(!check_daemon_condition(PID, 0, true, true))
		return false;

	return true;
}

// Check if no daemons are currently under the master's control
bool TestMaster::check_no_daemons() {
	dprintf(D_FULLDEBUG, "In TestMaster::check_no_daemons\n");
	char *daemon_name = NULL;

	if(daemons.NumberOfChildren() != 0) {
		dprintf(D_ALWAYS, "Error: Number of children is still %d\n",
				daemons.NumberOfChildren());
		return false;
	}

	bool rval = true;
	saved_daemon_list->rewind();
	while((daemon_name = saved_daemon_list->next()) && rval) {
		// Skip over the master
		if(strcmp(daemon_name, "MASTER") != MATCH) {
			// Check daemons
			if(daemons.FindDaemon(daemon_name) != NULL) {
				dprintf(D_ALWAYS, "Error: Found daemon '%s'\n", daemon_name);
				rval = false;
			}
		}
	}
	return rval;
}

// Check if Daemons::Update() works as expected
bool TestMaster::check_update(bool daemons_running) {
	dprintf(D_FULLDEBUG, "In TestMaster::check_update()\n");
	ClassAd temp_ad;
	char *daemon_name = NULL;
	daemon *d = NULL;
	int ad_value = 0, d_value = 0;
	char buf[128];

	daemons.Update(&temp_ad);
	saved_daemon_list->rewind();
	while((daemon_name = saved_daemon_list->next()) &&
			(d = daemons.FindDaemon(daemon_name)))
	{
		// Check timestamp
		ad_value = 0;
		d_value = (int)d->timeStamp;
		sprintf(buf, "%s_Timestamp", daemon_name);
		temp_ad.LookupInteger(buf, ad_value);
		if(ad_value != d_value) {
			dprintf(D_ALWAYS, "Error: Incorrect timestamp for daemon "
					"'%s'(%d != %d)\n", daemon_name, ad_value, d_value);
			return false;
		}

		// Check start time
		ad_value = 0;
		d_value = (int)d->startTime;
		sprintf(buf, "%s_StartTime", daemon_name);
		temp_ad.LookupInteger(buf, ad_value);

		if(daemons_running || strcmp(daemon_name, "MASTER") == MATCH) {
			if(ad_value != d_value) {
				dprintf(D_ALWAYS, "Error: Incorrect start time for daemon "
						"'%s'(%d != %d)\n", daemon_name, ad_value, d_value);
				return false;
			}
		} else {
			if(ad_value != 0) {
				dprintf(D_ALWAYS, "Error: Incorrect start time for daemon "
						"'%s'(%d != 0(%d))\n", daemon_name, ad_value, d_value);
				return false;
			}
		}
	}

	return true;
}

// Just check if local_test_num is 0
bool TestMaster::check_master_restart(bool restarted) {
	dprintf(D_FULLDEBUG, "In TestMaster::check_master_restart()\n");

	if(restarted && local_test_num != 0) {
		dprintf(D_ALWAYS, "Error: Master did not restart.\n");
		return false;
	} else if(!restarted && local_test_num == 0) {
		dprintf(D_ALWAYS, "Error: Master did restart.\n");
		return false;
	}

	return true;
}

// Make sure given daemon restarted so many times and restarted at/near the
// given time
bool TestMaster::check_daemon_restart_time(char *daemon_name, int num_restarts,
		time_t start_time)
{
	dprintf(D_FULLDEBUG, "In TestMaster::check_daemon_restart_time()\n");
	daemon *d;

	if((d = daemons.FindDaemon(daemon_name))) {
		// First check if start time is more recent than old start time
		if(d->startTime < saved_start_time) {
			dprintf(D_ALWAYS, "Error: Condition 'START_TIME' is '%d' for daemon"
					" '%s' with old start time '%d'\n", (int)d->startTime,
					daemon_name, (int)saved_start_time);
		}

		// Check if start time is close to what we expected
		if(d->startTime > start_time + next_start_offset ||
				d->startTime < start_time - next_start_offset)
		{
			dprintf(D_ALWAYS, "Error: Condition 'START_TIME' is '%d' not "
				"'%d +/- %d' for daemon '%s'\n", (int)d->startTime,
				(int)start_time, next_start_offset, daemon_name);
			return false;
		}
	}

	if(!check_single_daemon_condition(RESTARTS, num_restarts, daemon_check,
			true, true))
	{
		return false;
	}


	return true;
}

void TestMaster::cleanup(int rval) {
	dprintf(D_FULLDEBUG, "In TestMaster::cleanup()\n");
	char *temp = NULL;

	// Dump info
	if(rval != PASSED_TESTS) {
		dprintf(D_ALWAYS, "Timer id: %d\n", timer_id);
		dprintf(D_ALWAYS, "Local test number: %d\n", local_test_num);
		dprintf(D_ALWAYS, "Global test number: %d\n", global_test_num);
		dprintf(D_ALWAYS, "Retry number: %d\n", retry_num);
		dprintf(D_ALWAYS, "Phase number: %d\n", phase_num);
		dprintf(D_ALWAYS, "Initialized: %d\n", initialized);
		dprintf(D_ALWAYS, "Setup success: %d\n", setup_success);
		dprintf(D_ALWAYS, "Saved time: %d\n", (int)saved_time);
		dprintf(D_ALWAYS, "Saved number of children: %d\n", saved_num_children);
		if((temp = saved_daemon_list->print_to_string())) {
			dprintf(D_ALWAYS, "Saved daemon list: %s\n", temp);
			free(temp);
		}
		dprintf(D_ALWAYS, "Saved next start time: %d\n", (int)saved_time);

		dump_daemon_info();
	}

	// Cancel timer
	if(daemonCore->Cancel_Timer(timer_id) != 0)
		dprintf(D_ALWAYS, "Unable to cancel timer %d.\n", timer_id);

	// Stop the master
	master_exit(rval);
}

void TestMaster::dump_daemon_info() {
	StringList *temp_list = NULL;
	char *daemon_name = NULL;

	temp_list = &daemons.ordered_daemon_names;
	if(saved_daemon_list != NULL) {
		temp_list = saved_daemon_list;
	}
	temp_list->rewind();
	while((daemon_name = temp_list->next())) {
		print_daemon_info(daemon_name);
	}
}

bool check_daemon_condition(int condition, int value, bool match,
		bool ignore_master, bool output)
{
	bool rval = true;
	char *daemon_name;
	daemons.ordered_daemon_names.rewind();
	while((daemon_name = daemons.ordered_daemon_names.next())) {
		if(!ignore_master || strcmp(daemon_name, "MASTER") != MATCH) {
			if(!check_single_daemon_condition(condition, value, daemon_name,
					match))
			{
				if(output) {
					if(match) {
						dprintf(D_ALWAYS, "Error: Condition '%s' is not '%d' "
								"for daemon '%s'\n", 
								condition_to_string(condition), value, 
								daemon_name);
					} else {
						dprintf(D_ALWAYS, "Error: Condition '%s' is '%d' for "
								"daemon '%s'\n", condition_to_string(condition),
								value, daemon_name);
					}
				}
				rval = false;
			}
		}
	}
	return rval;
}

bool check_single_daemon_condition(int condition, int value, char *daemon_name,
		bool match, bool output)
{
	daemon *d = daemons.FindDaemon(daemon_name);
	bool rval = false;
	int actual_value = -1;

	if(d) {
		switch(condition) {
			case START_TIME:
				actual_value = d->startTime;
				break;
			case PID:
				actual_value = d->pid;
				break;
			case RESTARTS:
				actual_value = d->restarts;
				break;
			case HOLD:
				actual_value = d->OnHold();
				break;
			default:
				dprintf(D_ALWAYS, "Error: Invalid condition '%d'\n", condition);
				return false;
				break;
		}
	} else {
		dprintf(D_ALWAYS, "Error: Cannot find daemon '%s'\n", daemon_name);
		return false;
	}

	rval = (value == actual_value);

	if(!match) {
		rval = !rval;
	}

	if(output && !rval) {
		if(match) {
			dprintf(D_ALWAYS, "Error: Condition '%s' is '%d' not '%d' for "
					"daemon '%s'\n", condition_to_string(condition), 
					actual_value, value, daemon_name);
		} else {
			dprintf(D_ALWAYS, "Error: Condition '%s' is '%d' for daemon '%s'\n",
					condition_to_string(condition), value, daemon_name);
		}
	}

	return rval;
}

void print_daemon_info(char *daemon_name) {
	dprintf(D_ALWAYS, "Dumping info for daemon '%s'\n", daemon_name);
	MyString temp;
	daemon *d = NULL;

	if((d = daemons.FindDaemon(daemon_name))) {
		d->env.getDelimitedStringForDisplay(&temp);
		dprintf(D_ALWAYS, "Daemon %s:\n\ttype: %d\n\tname in config file: "
				"%s\n\td name: %s\n\tlog filename in config file: %s\n\t"
				"flag in config file: %s\n\tprocess name: %s\n\twatch name:"
				" %s\n\tlog_name: %s\n\truns here: %d\n\tpid: %d\n\t"
				"restarts: %d\n\tnewExec: %d\n\ttimestamp: %d\n\t"
				"start time: %d\n\tisDC: %d\n\ton hold: %d\n\tenv: %s\n", 
				daemon_name,
				d->type,
				d->name_in_config_file,
				d->daemon_name,
				d->log_filename_in_config_file,
				d->flag_in_config_file,
				d->process_name,
				d->watch_name,
				d->log_name,
				d->runs_here,
				d->pid,
				d->restarts,
				d->newExec,
				(int)d->timeStamp,
				(int)d->startTime,
				d->isDC,
				d->OnHold(),
				temp.Value());
	}
}

bool update_timestamp(char *daemon_name) {
	dprintf(D_FULLDEBUG, "In update_timestamp(%s)\n", daemon_name);
	daemon *d;
	if((d = daemons.FindDaemon(daemon_name))) {
		dprintf(D_FULLDEBUG, "Updating access/modification times for file '%s'"
				"\n", d->watch_name);
		time_t ctime = time(NULL);
		struct utimbuf tim;
		tim.actime = ctime;
		tim.modtime = ctime;
		return (utime(d->watch_name, &tim) == 0);
	}
	return false;
}

// change condor_main_argv so we start at test_num when master restarts
bool save_test_num_for_restart(int test_num) {
	dprintf(D_FULLDEBUG, "In save_test_num_for_restart(%d)\n", test_num);
	char buf[10];
	char **ptr = condor_main_argv;

	for(int i = 0; i < condor_main_argc && ptr && ptr[i]; i++) {
		if(ptr[i][1] == 'e') {
			i++;
			if(!ptr[i]) {
				return false;
			}
			free(ptr[i]);
			sprintf(buf, "%d", -test_num);
			ptr[i] = strdup((const char*)&buf);
			return true;
		}
	}

	return false;

}

bool call_command_handler(int cmd, char *daemon_name) {
	dprintf(D_FULLDEBUG, "In call_command_handler(%d, %s)\n", cmd, daemon_name);
	daemon *d = NULL;

	d = daemons.FindDaemon(daemon_name);

		// These commands cannot go through admin_command_handler without
		// using socks, so they just do what is done by the master for
		// simplicity
	switch(cmd) {
	case DAEMON_ON:
		if(!d)
			goto failure;
		d->Hold( false );
		return (d->Start() >= 1);
	case DAEMON_OFF:
		if(!d)
			goto failure;
		d->Hold( true );
		d->Stop();
		return true;
	case DAEMON_OFF_FAST:
		if(!d)
			goto failure;
		d->Hold( true );
		d->StopFast();
		return true;
	case DAEMON_OFF_PEACEFUL:
		if(!d)
			goto failure;
		d->Hold( true );
		d->StopPeaceful();
		return true;
	case CHILD_ON:
		if(!d)
			goto failure;
		d->Hold( false, true );
		return (d->Start(true) >= 1);
	case CHILD_OFF:
		if(!d)
			goto failure;
		d->Hold( true, true );
		d->Stop( true );
		return true;
	case CHILD_OFF_FAST:
		if(!d)
			goto failure;
		d->Hold( true, true );
		d->StopFast( true );
		return true;
	default:
		break;
	}

	return (admin_command_handler(NULL, cmd, NULL) == TRUE);
failure:
	dprintf(D_ALWAYS, "Error: Cannot find daemon name '%s'\n", daemon_name);
	return false;
}

char *condition_to_string(int condition) {
	switch(condition) {
		case START_TIME:
			return "START_TIME";
			break;
		case PID:
			return "PID";
			break;
		case RESTARTS:
			return "RESTARTS";
			break;
		case HOLD:
			return "HOLD";
			break;
		default:
			return "UNKNOWN";
			break;
	}
}

void setup_quick_stop(int timeframe) {
	new_bin_delay = timeframe/4;
	shutdown_graceful_timeout = timeframe/4;
	shutdown_fast_timeout = timeframe/4;
}

void setup_quick_restart(int timeframe, int restarts) {
	if(restarts == 1) {
		master_backoff_constant = timeframe/4;
		master_backoff_factor = timeframe/4;
	} else {
		master_backoff_constant = timeframe/2 - 1;
		master_backoff_factor = 1;
	}

	new_bin_delay = timeframe/8;
	shutdown_graceful_timeout = timeframe/8;
	shutdown_graceful_timeout = timeframe/8;
}
