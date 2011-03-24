#ifndef _CONDOR_MASTER_TEST_H
#define _CONDOR_MASTER_TEST_H

class TestMaster: public Service
{
	public:
		TestMaster(int start_test_num);
		~TestMaster();
		void start_tests();

	private:
		void test_main();
		bool test_check();
		bool test_setup();
		bool do_basic_tests();

		// Check functions
		bool check_daemons_exist();
		bool check_daemon_started(char *daemon_name);
		bool check_daemons_started();
		bool check_restart_new_exec();
		bool check_daemon_stopped(char *daemon_name);
		bool check_daemons_stopped();
		bool check_no_daemons();
		bool check_update(bool daemons_running);
		bool check_master_restart(bool restarted);
		bool check_daemon_restart_time(char *daemon_name, int num_restarts,
			time_t start_time);


		// Utility functions
		bool setup();
		void cleanup(int rval);
		void dump_daemon_info();

		int timer_id;	// store timer id so we can cancel it
		int local_test_num;
		int global_test_num;
		int retry_num;
		int phase_num;
		bool initialized;
		bool setup_success;
		int next_start_offset;

		// Stuff thats saved when master starts up 
		time_t saved_time;
		time_t saved_start_time;
		int saved_num_children;
		StringList *saved_daemon_list;
		time_t saved_next_start;

		// Stuff we get from config file
		int default_period;
		int default_num_retries;
		char *daemon_check;

};

// Other stuff
enum { START_TIME, PID, RESTARTS, HOLD };
bool 	check_daemon_condition(int condition, int value, bool expect, bool ignore_master = false, bool output = true);
bool 	check_single_daemon_condition(int condition, int value, char *daemon_name, bool expect, bool output = false);
void 	print_daemon_info(char *daemon_name);
bool 	update_timestamp(char *daemon_name);
char 	*condition_to_string(int condition);
void 	setup_quick_stop(int timeframe);
bool 	save_test_num_for_restart(int test_num);
bool 	call_command_handler(int cmd, char *daemon_name = "");
void 	setup_quick_restart(int timeframe, int restarts);

#endif
