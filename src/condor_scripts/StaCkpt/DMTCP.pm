#! /usr/bin/perl -w
use strict;
use warnings;

package StaCkpt::DMTCP;
use CondorUtils;
use Errno qw/EAGAIN/;
use File::Path;
use POSIX ":sys_wait_h";

# how much time to sleep while waiting for things to happen.
sub delay { sleep .20; }

###############################################################################
# The functions which implement the public interface to this object.
###############################################################################


####################
# Instantiate a StaCkpt::DMTCP object
####################
sub new 
{
	# This first argument is $this, or it will be....
	my ($class) = shift;

	# The rest of the arguments to new
	my ($dist) = @_;

	my $h = bless {
		# initialize the location of DMTCP
		dist		=> $dist,
		bindir		=> "$dist/bin",
		libdir		=> "$dist/lib",
		coord		=> "$dist/bin/dmtcp_coordinator",
		cmnd		=> "$dist/bin/dmtcp_command",
		ckpt		=> "$dist/bin/dmtcp_checkpoint",

		# what phase is the checkpointer harness in?
		phase => 'UNSETUP',
		activity => undef,
		state => undef,

		# my slave pid
		slave_pid => 0,

		# The slave's pid associated with the program it is running
		exec_pid => 0,

	}, (ref($class) || $class);
	return  $h
}

####################
# Setup the harness
####################

sub setup 
{
	my $this = shift;
	my($testname, $sandbox_root, $validator_func) = @_;
	my $task;
	my @results;
	my $result;
	my $done = 0;

	if (!defined($sandbox_root)) {
		die "StaCkpt::DMTCP(): Please specify a directory where checkpoint " .
			"files may be stored";
	}
	$this->{'testname'} = $testname;
	$this->{'sandbox'} = "$sandbox_root/dmtcp-ckptdir-$testname-$$";
	$this->{'stdout'} = "$this->{'sandbox'}/$this->{'testname'}.out";
	$this->{'stderr'} = "$this->{'sandbox'}/$this->{'testname'}.err";
	# used for the message passing interface between parent and slave
	$this->{'mqueue'} = "$sandbox_root/dmtcp-ckptdir-$testname-$$/mqueue";

	if (mkpath($this->{'sandbox'}) == 0) {
		die "Failed to make sandbox: $this->{'sandbox'}";
	}

	if (mkpath($this->{'mqueue'}) == 0) {
		#die "Failed to make mqueue: $this->{'mqueue'}";
	}

	$this->{'validator_func'} = $validator_func;

	close_gcb_inherit_fd();

	print "Master: About to start slave...\n";
	$this->slave_start();

	$task = gentask("ping");
	$this->master_write_task($task);

	# block forever until I get the pong message back from the slave
	while(!$done) {
		# I should only be getting ONE expected result back in the array due 
		# to the state transition flow, but I don't check/enforce it here.
		@results = $this->master_read_results();
		foreach $result (@results) {
			if ($result->{'result_for_task'} =~ /^ping$/) {
				if ($result->{'pong'} =~ m/^ok$/) {
					$done = 1;
				}
			}
		}

		# see if the slave died...
		if ($this->reap_the_dead()) {
			print "Master: The slave died and was reaped.\n";
		}

		delay();
	}
	print "Master: Started Slave[$this->{'slave_pid'}]!\n";

	$this->set_status("SETUP", undef, undef);
}

sub init 
{
	my $this = shift;
	my $task;

	$this->set_status("INIT", "STARTING", undef);

	$task = gentask("init");
	$this->master_write_task($task);
}

sub status 
{
	my $this = shift;
	my @list = ($this->{'phase'}, $this->{'activity'}, $this->{'state'});

	return @list;
}

# if I timed out, return 1, otherwise, return 0.
sub wait_until_not
{
	my $this = shift;
	my ($phase, $activity, $state, $timeout) = @_;
	my ($cur_phase, $cur_activity, $cur_state);
	my ($start, $end);
	my $timed_out = 0;
	
	$start = time();
	while($this->is_status($phase, $activity, $state)) {
		$this->master_process_results_iteration();
		if (defined($timeout)) {
			$end = time();
			if ($timeout <= $end - $start) {
				# timeout happened, so this call didn't do what I expected it
				# to do. The status is not changed at the end of this call.
				return 1;
			}
		}
		delay();
	}

	# Everything was happy, the status changed.
	return 0;
}

sub checkpoint
{
	my $this = shift;
}

sub checkpoint_and_exit
{
	my $this = shift;
}

sub verify_checkpoint
{
	my $this = shift;
}

sub execute
{
	my $this = shift;
	my($executable_name, @args) = @_;
	my $task;
	my $a;

	$this->set_status("TASK", "STARTING", "NO_CHECKPOINT");
	$task = gentask("execute", {
				executable => $executable_name,
				num_args => scalar(@args),
				});

	# add in the args in order of presentation
	for ($a = 0; $a < scalar(@args); $a++) {
		# XXX Hrm, Maybe I should enclose it in quotes and strip them
		# off later to preserve whitespace at the ends of the argument?
		# Meh, I don't think it is a big deal at this time.
		$task->{"arg_$a"} = $args[$a];
	}

	$this->master_write_task($task);
}

sub shutdown
{
	my $this = shift;
	my $msg;
	my @results;

	print "Shutting down checkpointing system.\n";
	$msg = gentask("shutdown");
	$this->master_write_task($msg);
	$this->set_status("SHUTDOWN", "STARTING", undef);
}

# This will slaughter the slave before removing the sandbox, so the slave
# can't do anything wrong. 
sub teardown
{
	my $this = shift;
	my ($phase, $activity, $state) = $this->status();

	if (!defined($this->{'phase'}) || $phase =~ m/UNSETUP/) {
		print "Avoiding teardown of seemingly not set up harness.\n";
		return;
	}

	# shut off the task, if any
	# TODO

	# shut off the dmtcp_coordinator, if any
	# TODO

	# Kill the slave, if present.
	if ($this->reap_the_dead()) {
		print "Master: The slave died and was reaped.\n";
	} else {
		print "Killing slave: $this->{'slave_pid'}\n";
		# XXX race condition! if the slave is was alive for the reaping check
		# but died before this code ran, then I could
		# potentially kill another process on the system of mine that happened
		# to take the same pid.
		system("kill -9 $this->{'slave_pid'} > /dev/null 2>&1");
	}

	# remove the stuff under the sandbox
	rmtree($this->{'sandbox'});
}

# XXX Yikes! I don't check to see if this is called under the correct
# conditions of a task/completed/* state.
sub validate
{
	my $this = shift;

	return 
		&{$this->{'validator_func'}}(
								$this->{'task_exit_status'}, 
								$this->{'stdout'}, 
								$this->{'stderr'});
}

###############################################################################
# Utility functions
###############################################################################
sub set_status 
{
	my $this = shift;
	my ($phase, $activity, $state) = @_;

	$this->{'phase'} = $phase;
	$this->{'activity'} = $activity;
	$this->{'state'} = $state;
}

# return true if the status is the same as the arguments, false otherwise.
sub is_status 
{
	my $this = shift;
	my ($phase, $activity, $state) = @_;
	my ($cur_phase, $cur_activity, $cur_state) = $this->status();

	$phase = "undef" if (!defined($phase));
	$activity = "undef" if (!defined($activity));
	$state = "undef" if (!defined($state));

	$cur_phase = "undef" if (!defined($cur_phase));
	$cur_activity = "undef" if (!defined($cur_activity));
	$cur_state = "undef" if (!defined($cur_state));

	return $phase =~ /^\Q$cur_phase\E$/ &&
			$activity =~ /^\Q$cur_activity\E$/ &&
			$state =~ /^\Q$cur_state\E$/;
}

# Depending upon who calls this, master or slave, this function will reap:
#	A slave process
#	A job process the slave executed.
sub reap_the_dead
{
	my $this = shift;
	my $who = shift;
	my $reaped_pid;
	my $status;
	my ($executed, $signaled, $coredump, $retcode, $signal);

	$reaped_pid = waitpid(-1, WNOHANG);
	$status = $?;
	if ($reaped_pid == -1 || $reaped_pid == 0) {
		return 0;
	}

	($executed, $signaled, $coredump, $retcode, $signal) = dissect($status);

	# Processes the Master cares about:
	# was it a slave that died? (only makes sense in Master)
	if ($reaped_pid == $this->{'slave_pid'}) {
		print "Master: Reaped Slave[$reaped_pid]\n";
		$this->{'slave_pid'} = 0;
		$this->{'slave_pid_status'}{'executed'} = $executed;
		$this->{'slave_pid_status'}{'signaled'} = $signaled;
		$this->{'slave_pid_status'}{'coredump'} = $coredump;
		$this->{'slave_pid_status'}{'retcode'} = $retcode;
		$this->{'slave_pid_status'}{'signal'} = $signal;
		return 1;
	}

	# Processes the Slave cares about:
	# was it a job executable? (only makes sense in Slave)
	if ($reaped_pid == $this->{'exec_pid'}) {
		print "Slave[$$]: Reaped executable[$reaped_pid]\n";
		$this->{'exec_pid'} = 0;
		$this->{'exec_pid_status'}{'executed'} = $executed;
		$this->{'exec_pid_status'}{'signaled'} = $signaled;
		$this->{'exec_pid_status'}{'coredump'} = $coredump;
		$this->{'exec_pid_status'}{'retcode'} = $retcode;
		$this->{'exec_pid_status'}{'signal'} = $signal;
		return 1;
	}

	return 0;
}

# cut apart a return status from an exited program
sub dissect
{
	my ($status) = @_;
	my ($executed, $signaled, $coredump, $retcode, $signal);

	# defaults which are contextually interpreted.
	$executed = 1;
	$signaled = 0;
	$coredump = 0;
	$retcode = 0;
	$signal = 0;

	if ($status == -1) {
		# failed to execute
		$executed = 0;
	}
	elsif ($status & 127) {
		# died with signal
		$signaled = 1;
		$signal = $status & 127;
		$coredump = $status & 128;
	} else {
		# normal exit
		$retcode = $status >> 8;
	}

	return ($executed, $signaled, $coredump, $retcode, $signal);
}

###############################################################################
# The master codes which process the results from the slave.
# This processes a single set of current results, then returns.
###############################################################################
sub master_process_results_iteration
{
	my $this = shift;
	my @results;

	@results = $this->master_read_results();
	$this->master_process_results(\@results);
}

sub master_process_results
{
	my $this = shift;
	my ($ref) = @_;
	my @results = @{$ref};
	my $t;

	my %cmds = (
		'ping' => \&master_process_result_ping,
		'init' => \&master_process_result_init,
		'execute' => \&master_process_result_execute,
		'task_completed' => \&master_process_result_task_completed,
		'shutdown' => \&master_process_result_shutdown,
	);

	# Each handler function is born with the result that spawned it as the first
	# argument.
	foreach $t (@results) {
		print "Master: Processing Result: " .
			"$t->{'result_for_task'}/$t->{'task_seqnum'}\n";

		if (exists($cmds{$t->{'result_for_task'}})) {
			&{$cmds{$t->{'result_for_task'}}}($this, $t);
		} else {
			print "Master: ERROR! Result handler for '$t->{result_for_task}' " .
				"is unimplemented!\n";
		}
	}
}

sub master_process_result_ping
{
	my $this = shift;
	my ($ref) = @_;

	# XXX Implement me? Can I use it as a regular liveness ping?
}

sub master_process_result_shutdown
{
	my $this = shift;
	my ($ref) = @_;

	if ($ref->{'result'} =~ /^ok$/) {
		$this->set_status("SHUTDOWN", "COMPLETED", undef);
	}

	# we reap it elsewhere...
}

sub master_process_result_init
{
	my $this = shift;
	my ($ref) = @_;

	if ($ref->{'result'} =~ /^notok$/) {
		print "Master: Slave failed to initialize dmtcp!\n";
		$this->set_status("INIT", "FAILED", undef);
		return;
	}

	# store this in my object's hash
	$this->{'coord_port'} = $ref->{'port'};

	$this->set_status("INIT", "COMPLETED", undef);
}

sub master_process_result_execute
{
	my $this = shift;
	my ($ref) = @_;

	if ($ref->{'result'} =~ /^notok$/) {
		$this->set_status("TASK", "FAILED", "BAD_START");
		return;
	}

	# This is already a race condition, since the executed task may have
	# decided to checkpoint on its own already.
	# XXX deal with me.
	$this->set_status("TASK", "RUNNING", "NO_CHECKPOINT");
}

# This is often an asynchronous event from the slave stating that the executable
# I asked to run has exited. This contains the exit values from the process.
# The master will check to see if a checkpoint was taken to set the $state
# portion of the checkpointer state. This is just a simple file check for now.
sub master_process_result_task_completed
{
	my $this = shift;
	my ($ref) = @_;
	my @ckptfiles;
	my $ckpt_files_found;
	my %status;

	# pick out only what I need from the result about how the code exited.
	$status{'executed'} = $ref->{'executed'};
	$status{'signaled'} = $ref->{'signaled'};
	$status{'coredump'} = $ref->{'coredump'};
	$status{'retcode'} = $ref->{'retcode'};
	$status{'signal'} = $ref->{'signal'};
	$this->{'task_exit_status'} = \%status;

	# XXX XXX XXX
	# This code must be made aware of the exit status of a program that
	# has called ckpt_and_exit(). Under Condor, the program will die with
	# a USR2, but the DMTCP bridge API for that function isn't completed yet.

	# 0. Check the return code from the executable, if anything funny happened,
	# like an incorrectly signaled exit or a return code of non zero, 
	# the process is marked failed.
	if ($ref->{'signaled'} == 1 || $ref->{'retcode'} != 0) {
		$this->set_status("TASK", "FAILED", "BAD_EXIT");
		# store the reference to a status hash in my local object hash for 
		# later perusal by the validator function.
		return;
	}

	# 1. Check to see if some checkpoint files exist.
	# XXX They might have existed from the last run, how can I tell?
	# How do I know which ones for which I should be looking and how many?
	@ckptfiles = `find $this->{'sandbox'} -name "*.dmtcp" -print`;

	if ($? == 0 && scalar(@ckptfiles) > 0) {
		print "Master: Found a checkpoint!\n";
		$this->set_state("TASK", "COMPLETED", "CHECKPOINT_DONE");
		return;
	} 

		print "Master: No checkpoints found!\n";
	$this->set_status("TASK", "COMPLETED", "NO_CHECKPOINT");
}

###############################################################################
# This is responsible for starting a slave process.
###############################################################################
sub slave_start
{
	my $this = shift;
	my $pid;

	FORK: {
		if ($pid = fork()) {
			# parent
			# we waitpid nohang on this in the master iteration loop.
			$this->{'slave_pid'} = $pid;
		} elsif ($pid == 0) {
			# child
			exit $this->slave_main_loop();
		} else {
			# parent
			if ($! == EAGAIN) {
				sleep 1;
				redo FORK;
			}
		}
	}
}


###############################################################################
# Wait For, Find, and Handle various tasks from the master
###############################################################################
sub slave_main_loop
{
	my $this = shift;
	my @tasks;
	my $result;

	print "Slave[$$]: Summoned!\n";

	# a task handler getting a shutdown might set this to 1 in the object,
	# which causes us to break out of our main loop.
	$this->{'slave_shutdown'} = 0;

	while(!$this->{'slave_shutdown'}) {

		@tasks = $this->slave_read_tasks();
		$this->slave_process_tasks(\@tasks); # will side effect condition check

		if ($this->reap_the_dead()) { 
			print "Slave[$$]: The executable under my control exited:\n" .
			"\texecuted = $this->{'exec_pid_status'}{'executed'}\n" .
			"\tsignaled = $this->{'exec_pid_status'}{'signaled'}\n" .
			"\tcoredump = $this->{'exec_pid_status'}{'coredump'}\n" .
			"\tretcode = $this->{'exec_pid_status'}{'retcode'}\n" .
			"\tsignal = $this->{'exec_pid_status'}{'signal'}\n";

			# Tell the master, so it can update its internal state about the
			# system.
			$result = genresult_spontaneous("task_completed", {
				executed => $this->{'exec_pid_status'}{'executed'},
				signaled => $this->{'exec_pid_status'}{'signaled'},
				coredump => $this->{'exec_pid_status'}{'coredump'},
				retcode => $this->{'exec_pid_status'}{'retcode'},
				signal => $this->{'exec_pid_status'}{'signal'}});
			$this->slave_write_result($result);
		}

		delay();
	}

	print "Slave[$$]: Shutdown and exiting.\n";
	return 0;
}

sub slave_process_tasks
{
	my $this = shift;
	my ($ref) = @_;
	my @tasks = @{$ref};
	my $t;

	my %cmds = (
		'ping' => \&slave_do_task_ping,
		'init' => \&slave_do_task_init,
		'execute' => \&slave_do_task_execute,
		'shutdown' => \&slave_do_task_shutdown,
	);

	# Each handler function is born with the task that spawned it as the first
	# argument.
	foreach $t (@tasks) {
		print "Slave[$$]: Processing Task: $t->{'task'}/$t->{'seqnum'}\n";
		# I hope Strousoup gets the willies when this next line executes.
		if (exists($cmds{$t->{'task'}})) {
			&{$cmds{$t->{'task'}}}($this, $t);
		} else {
			print "Slave[$$]: Task '$t->{task}' is unimplemented!\n";
		}
	}
}

####################
# Task ping: the master is asking the slave if it is alive
####################
sub slave_do_task_ping
{
	my $this = shift;
	my ($tref) = @_;
	my $result;

	# The slave process is up and running.
	$result = genresult($tref, {pong => "ok"});
	$this->slave_write_result($result);
}

####################
# Task init: the master wants us to start up the checkpointing system
####################
sub slave_do_task_init
{
	my $this = shift;
	my ($tref) = @_;
	my $result;
	my $rhash;
	my $port;

	print "Slave[$$]: Initializing DMTCP.\n";

	$rhash = runcmd("$this->{'coord'} --port 0 --exit-on-last --background --ckptdir $this->{'sandbox'} 2>&1 | grep Port: | /bin/sed -e \"s/Port://g\" -e \"s/[ \\t]//g\"", 
			{die_on_failed_expectation => \&FALSE,
			emit_output => FALSE});

	######################################################
	# XXX runcmd seems broken. :( So let's force it into working.
	######################################################
	$rhash->{'expectation'} = 1;

	if ($rhash->{'expectation'} == 0) {
		# our default expectation of passing failed....
		print "Slave[$$]: Failed to start dmtcp_coordinator!\n";
		$result = genresult($tref, {result => "notok"});

		$this->slave_write_result($result);
		return;
	}

	$port = $rhash->{'stdout'}[0];
	chomp $port;

	# give it some time to wake up fully before having a job connect to it.
	sleep .5;

	# record this in my object so I can access it later when shutting it down
	# or checkpointing it.
	$this->{'coord_port'} = $port;

	print "Slave[$$]: Started DMTCP coordinator on port: $port\n";

	$result = genresult($tref, {
				result => "ok",
				port => $port});

	$this->slave_write_result($result);
}

####################
# Task execute: run the program specified with specified arguments under
# the checkpointer.
####################
sub slave_do_task_execute
{
	my $this = shift;
	my ($tref) = @_;
	my $pid;
	my $result;
	my @args;
	my $a;
	my $slave_pid = $$;

	# XXX TODO

	# we fork the process and exec it. We'll shove the pid into the object's
	# hash table and reap for it in our slave main loop.
	# When and if the process dies, we'll send a message back to the master.

	if (! -x $tref->{'executable'}) {
		print "Slave[$$]: Executable not executable!\n";
		$result = genresult($tref, {
					result => "notok", 
					error => "not_executable"});
		$this->slave_write_result($result);
		return;
	}

	# assemble the argument list;
	for ($a = 0; $a < $tref->{'num_args'}; $a++) {
		push(@args, $tref->{"arg_$a"});
	}

	FORK: {
		if ($pid = fork()) {
			# parent

			# The slave will reap this in its main loop later.
			$this->{'exec_pid'} = $pid;
			print "Slave[$$]: Executed Job.\n";

		} elsif ($pid == 0) {
			my @prog;
			# exec'ing task under the dmtcp checkpointer.

			@prog = (
				# The dmtcp checkpoint program and arguments to get it to do the
				# right thing with the coordinator we started, among other
				# things.
				"$this->{'ckpt'}",
				"--port", "$this->{'coord_port'}",
				"--ckptdir", "$this->{'sandbox'}",
				"--tmpdir", "$this->{'sandbox'}",
				"--join", "--quiet",

				# the executable we wish to start and arguments.
				"$tref->{'executable'}",
				@args,

				# The file i/o redirection
				# XXX Fix input redirection to be usable.
				"< /dev/null",
				"1>", "$this->{'stdout'}",
				"2>", "$this->{'stderr'}",
			);

			print "Slave[$slave_pid]-helper[$$]: About to exec: " .
				join(' ', @prog) . "\n";

			# If sees shell metacharacters in here, like redirects and whatnot,
			# it'll execute it under /bin/sh -c automatically.
			exec join(' ', @prog) 
				or print STDERR "Slave:[$$]. Exec failed: $!\n";

			# XXX Since I already checked to see if the executable was actually
			# executable, the likelihood of this happening is very, very rare.
			# So, I won't currently properly handle the case unless it becomes
			# an actual problem in practice. Basically, I have to out of band
			# tell the slave this exit code is for the helper, not the job.
			# Maybe write a task and have the slave pick it up and do something
			# with it? Hrm...
			print "Slave[$slave_pid]-helper[$$]: ERROR! FAILED TO EXEC JOB!\n";
			exit 42;

		} else {
			# parent
			if ($! == EAGAIN) {
				sleep 1;
				redo FORK;
			}
		}
	}

	$result = genresult($tref, {result => "ok"});
	$this->slave_write_result($result);
}

####################
# Task shutdown: the master is asking the slave to shutdown
####################
sub slave_do_task_shutdown
{
	my $this = shift;
	my ($tref) = @_;
	my $result;
	my $rhash;

	# XXX Kill the jobs and turn off the coordinator.
	if (defined($this->{'coord_port'}) && $this->{'coord_port'} != 0) {
		# This may fail due to the job completing and --exit-on-last being
		# used in the initial set up. Or, it could fail for another unknown
		# reason we'd have to deduce, or it could happily succeed. For now, 
		# we just accept any result of this command.
		$rhash = runcmd(
			"$this->{'cmnd'} --quiet --port $this->{'coord_port'} -q",
			{expect_result => \&ANY, emit_output => FALSE});
		$this->{'coord_port'} = 0;
	}

	# This marks that the slave main loop is to finish.
	$this->{'slave_shutdown'} = 1;

	# Tell the master what we've done.
	$result = genresult($tref, {result => "ok"});
	$this->slave_write_result($result);
}


###############################################################################
# functions related to DMTCP's operation
###############################################################################
sub close_gcb_inherit_fd
{
	my $FDNUM = 0;

	if (!exists($ENV{'CB_INHERITFILE'})) {
		# nothing to do
		return; 
	}

	$FDNUM = $ENV{'CB_INHERITFILE'};
	chomp $FDNUM;

	if ($FDNUM =~ m/\d+/) {
		# fdopen the fd, then close it, which closes the fd too.
		print "Closing unneeded GCB_INHERIT fd: $FDNUM\n";
		open(FILEHANDLE, "<&=$FDNUM"); close FILEHANDLE;

		# Since this fd isn't available anymore, remove the env var.
		delete($ENV{'CB_INHERITFILE'});

		# anything this process forks afterwsrds will not have that
		# env var, or the actual open fd, in its process space.
	}
}

###############################################################################
# Functions which initialize the slave process
###############################################################################

# Seqnums of 0 are high proority results the slave wants to spontaneously
# send to the master. Otherwise, the task/result seqnums will match.
my $seqnum = 1;
sub genseqnum {
	my $val = $seqnum;
	$seqnum++;
	return $val;
}


###############################################################################
# Functions related to the message passing communication system to the slave
###############################################################################

# in a file called: foo.task
# SEQNUM: nnn 
# TASK: xxx
# arbitrary other key/value pairs which end up in a hash, slave interprets.

# Returns an array that is either empty, or has some tasks in it to do.
# sorted by SEQNUM, of the messages.
sub slave_read_tasks
{
	my $this = shift;
	my @fnames;
	my $f;
	my $line;
	my @tmp;
	my @tasks;

	# get the list of tasks;
	@fnames = `find $this->{'mqueue'} -name "*.task" -print`;
	map { chomp $_; } @fnames;

	# read each one into a hash and push it onto the task array
	foreach $f (@fnames) {
		next if ($f !~ /\.task$/);

		my %hash = (); # get a new hash
		open(FIN, $f) or die "Can't open mqueue: $this->{'mqueue'}: $!";
		# read the values, clean them up, put them into the hash
		while (defined($line = <FIN>)) {
			next if ($line =~ /^\s*#.*$/); # skip line comments
			next if ($line =~ /^(\s)*$/); # skip empty/blank lines
			my ($key, $value) = $line =~ /^([^:]+):(.*)$/;
			$key =~ s/^\s+//g;
			$key =~ s/\s+$//g;
			$key = lc($key);

			$value =~ s/^\s+//g;
			$value =~ s/\s+$//g;
			$hash{$key} = $value;
		}
		close(FIN);

		# Check to make sure there are keys for seqnum, task
		if (!exists($hash{'seqnum'})) {
			die "Incorrect mqueue task format: missing seqnum\n";
		}
		if (!exists($hash{'task'})) {
			die "Incorrect mqueue task format: missing task\n";
		}
		# store a reference to the pending task
		push @tmp, \%hash;

		# get rid of the newly read task file
		if (unlink($f) != 1) {
			die "Slave $$ couldn't unlink file $f. Race condition. This means ".
				"more than one test is using the same sandbox_root _and_ " .
				"testname. Please fix.";
		}
	}

	# sort the tasks by sequence number so they are processed correctly.
	@tasks = sort { $$a{'seqnum'} <=> $$b{'seqnum'}} @tmp;

	return @tasks;
}

# TASK_SEQNUM must match SEQNUM from task
# key/value pairs of the result, master interprets.
sub slave_write_result
{
	my $this = shift;
	my ($res) = @_;
	my $resfile;
	my $k;

	if (!exists($res->{'task_seqnum'})) {
		die "Slave $$ can't write unknown seqnum as a result.";
	}

	$resfile = "$this->{'mqueue'}/$res->{'task_seqnum'}.result";
	# We write a tmp file so the master doesn't see a partial copy of this.
	open(FOUT, "> $resfile.tmp") or
		die "Can't open result file: $resfile: $!\n";
	print FOUT "# Written by slave $$\n";
	foreach $k (keys %{$res}) {
		print FOUT "$k: $res->{$k}\n";
	}
	close(FOUT);

	# An atomic rename.
	system("mv $resfile.tmp $resfile");
}

sub master_write_task
{
	my $this = shift;
	my ($task) = @_;
	my $slave_file;
	my $k;

	if (!exists($task->{'seqnum'})) {
		die "Master $$ can't write unknown seqnum task to slave.";
	}

	if (!exists($task->{'task'})) {
		die "Master $$ can't write unknown task to slave.";
	}

	$slave_file = "$this->{'mqueue'}/$task->{'seqnum'}.task";
	# Write our task as a tmp file so the slave doesn't see it until it is done.
	open(FOUT, "> $slave_file.tmp") or
		die "Can't open slave task file: $slave_file: $!\n";
	print FOUT "# Written by master $$\n";
	foreach $k (keys %{$task}) {
		$k = lc($k);
		print FOUT "$k: $task->{$k}\n";
	}
	close(FOUT);

	# The rename is atomic.
	system("mv $slave_file.tmp $slave_file");
}

sub master_read_results
{
	my $this = shift;
	my @fnames;
	my $f;
	my $line;
	my @tmp;
	my @results;

	# get the list of results;
	@fnames = `find $this->{'mqueue'} -name "*.result" -print`;
	map { chomp $_; } @fnames;

	# read each one into a hash and push it onto the tmp array
	foreach $f (@fnames) {
		next if ($f !~ /\.result$/);

		my %hash = (); # get a new hash
		open(FIN, $f) or die "Can't open mqueue: $this->{'mqueue'}: $!";
		# read the values, clean them up, put them into the hash
		while (defined($line = <FIN>)) {
			next if ($line =~ /^\s*#.*$/); # skip line comments
			next if ($line =~ /^(\s)*$/); # skip empty/blank lines
			my ($key, $value) = $line =~ /^([^:]+):(.*)$/;
			$key =~ s/^\s+//g;
			$key =~ s/\s+$//g;
			$key = lc($key);

			$value =~ s/^\s+//g;
			$value =~ s/\s+$//g;
			$hash{$key} = $value;
		}
		close(FIN);

		# Check to make sure there are keys for task_seqnum
		if (!exists($hash{'task_seqnum'})) {
			die "Incorrect mqueue task format: missing task_seqnum\n";
		}
		# store a reference to the pending result
		push @tmp, \%hash;

		# get rid of the newly read result file
		if (unlink($f) != 1) {
			die "Master $$ couldn't unlink file $f. Race condition. This " .
				"means more than one test is using the same sandbox_root " .
				"_and_ testname. Please fix.";
		}
	}

	# sort the results by sequence number so they are processed correctly.
	@results = sort { $$a{'task_seqnum'} <=> $$b{'task_seqnum'}} @tmp;

	return @results;
}

# called like gentask("thing", {stuff => "asdjhf"})
sub gentask
{
	my %task;
	my $tname = shift;
	my ($href) = @_;
	my $a;

	# unique serial number
	$task{'seqnum'} = genseqnum();

	# The actual thing the slave is supposed to do
	$task{'task'} = $tname;

	# copy over the arguments into the task
	if (defined $href) {
		foreach $a (keys %{$href}) {
			$task{$a} = $href->{$a};
		}
	}

	return \%task;
}

# called like genresult($task_hash_ref, {stuff => "asdjhf"})
sub genresult
{
	my %result;
	my ($tref) = shift;
	my ($href) = @_;
	my $a;

	$result{'task_seqnum'} = $tref->{'seqnum'};
	$result{'result_for_task'} = $tref->{'task'};

	# copy over the arguments into the result, master interprets.
	if (defined $href) {
		foreach $a (keys %{$href}) {
			$result{$a} = $href->{$a};
		}
	}

	return \%result;
}

# called like genresult_spontaneous("result_name", {stuff => "asdjhf"})
# seqnum 0 is considered a spontaneously created message by the slave 
# to the master.
sub genresult_spontaneous
{
	my %result;
	my ($rname) = shift;
	my ($href) = @_;
	my $a;

	$result{'task_seqnum'} = 0;
	$result{'result_for_task'} = $rname;

	# copy over the arguments into the result, master interprets.
	if (defined $href) {
		foreach $a (keys %{$href}) {
			$result{$a} = $href->{$a};
		}
	}

	return \%result;
}

###############################################################################
# Stupid tricks to help me debug things.
###############################################################################

# Give me my function name
sub where
{
	my ($package, $filename, $line, $subroutine, $hasargs, 
		$wantarray, $evaltext, $is_require, $hints, $bitmask) = caller(1);
	
	return "$package::$subroutine():";
}

sub enter
{
	my ($package, $filename, $line, $subroutine, $hasargs, 
		$wantarray, $evaltext, $is_require, $hints, $bitmask) = caller(1);
	
	return;
	print "Entering ${package}::${subroutine}()\n";
}

sub leave
{
	my ($package, $filename, $line, $subroutine, $hasargs, 
		$wantarray, $evaltext, $is_require, $hints, $bitmask) = caller(1);
	
	return;
	print "Leaving ${package}::${subroutine}()\n";
}

1;




