#! /usr/bin/perl -w

# You must be in the condor_tests/gcc directory to run this example.
# It currently just runs job_rsc_all-syscalls_std.dmtcp.exe under dmtcp
# and checks to see if it exited correctly. This program is intended to
# be the model of all .run files which use DMTCP to test a condor stduniv
# program. 

# The reason for the complexity of this program is that DMTCP consists of
# a daemon which controls checkpointing for a set of processes and a tool
# by which you enact things like checkpoints. There are many opportunities
# for blocking or having other problems which simply result in a .run file
# producing a -32 timeout in NMI. And that is worse than useless. So, this
# program and module are here to provide a master/slave relationship with
# the DMTCP (or other third party checkpointer, if desired) tools so the
# .run file (which is ultimately the master process) can figure out what
# happened to the best of its ability and give back useful information to
# the person looking at the test. This design is the result of 6+ iterations
# of code and designs in which these exact blocking problems and other things
# resulted in serious frustration and useless tests. This code base aims to
# remedy that.

# This program, and the StaCkpt module is currently unfinished. It needs
# to implement checkpoint, checkpoint and exit, and restart. It also needs
# to implement the verification that a checkpoint actually happened properly.

# There are some minor foibles with the state system as described in the
# doc file in StaCkpt. But those will get figured out as the .run files are
# written and used.

use strict;
use warnings;

push @INC, ".";
use StaCkpt::DMTCP;
use StaCkpt::Condor;

sub new_checkpoint_harness {
	my($type) = shift;

	if($type eq 'DMTCP') {
		return StaCkpt::DMTCP->new(@_);
	} elsif($type eq 'Condor') {
		return StaCkpt::Condor->new(@_);
	} else {
		die "Unknown ckpt_harness type: $type\n";
	}
}

sub main 
{
	my $h;
	my $timed_out;
	my $result;

	$h = new_checkpoint_harness('DMTCP', "./dmtcp-local");
	$h->setup("job_rsc_all-syscalls_std", ".", \&check_job_output);

	$h->init();

	$timed_out = $h->wait_until_not("INIT", "STARTING", undef, 120);

	if ($timed_out || $h->is_status("INIT", "FAILED", undef)) {
		print "Master: The slave couldn't start the coordinator. Aborting.\n";
		goto CLEANUP;
	}

	print "Master: dmtcp_coordinator started on port $h->{'coord_port'}\n";

	$h->execute("./job_rsc_all-syscalls_std.dmtcp.exe", 
		"-_condor_aggravate_bugs");

	$timed_out = $h->wait_until_not("TASK", "STARTING", "NO_CHECKPOINT", 120);

	# at this point, a the checkpointer could be in these states:
	# TASK FAILED BAD_START
	# TASK FAILED BAD_EXIT
	# TASK RUNNING NO_CHECKPOINT / CHECKPOINT_PENDING / CHECKPOINT_DONE
	# TASK COMPLETED NO_CHECKPOINT / CHECKPOINT_DONE

	if ($timed_out || $h->is_status("TASK", "FAILED", "BAD_START")) {
		print "Master: Task failed to start up!\n";
		goto CLEANUP;
	}
	print "Master: task is started up!\n";

	$timed_out = $h->wait_until_not("TASK", "RUNNING", "NO_CHECKPOINT");
	print "Master: Task exited!\n";

	if ($timed_out || $h->is_status("TASK", "FAILED", "BAD_EXIT")) {
		print "Master: Task exited with a bad status code!\n";
		goto CLEANUP;
	}

	# calls the validator I installed and gives me the result of the function
	# call. WARNING: MUST BE CALLED BEFORE TEARDOWN FUNCTION!
	$result = $h->validate();

	CLEANUP:
	$h->shutdown();

	$timed_out = $h->wait_until_not("SHUTDOWN", "STARTING", undef, 120);
	if ($timed_out || $h->is_status("SHUTDOWN", "FAILED", undef)) {
		print "Master: Shutdown of system failed!\n";
	}

	$h->teardown();

	print "Done.\n";

	return $result;
}

# returns 1 if everything was good with the output of the codes
# return 0 if something failed.
sub check_job_output
{
	# status hash, stdout file, stderr file
	my ($sref, $stdout, $stderr) = @_;

	my @line;

	if (! -f $stdout) {
		goto FAILED;
	}

	@line = `tail -1 $stdout`;

	if ($sref->{'signaled'} == 0 &&
		$sref->{'retcode'} == 0 &&
		$line[0] =~ m/^Succeeded Phase 4$/)
	{
		print "Master: Test worked correctly!\n";
		return 0; # EXIT_SUCCESS
	}

	FAILED:
	print "Master: Test failed!\n";
	return 1; # EXIT_FAILURE
}

# Something to test the master/slave msg passing api.
sub test_communication
{
	my $h = shift;
	my @tasks;
	my $ref;
	my $k;

	# do slave reads work?
	@tasks = $h->slave_read_tasks();
	foreach $ref (@tasks) {
		foreach $k (keys %{$ref}) {
			print "\t$k = $ref->{$k}\n";
		}
		print "...\n";
	}

	# can the master write a task?
	print "Master writing task...\n";
	my %msg;
	$msg{'seqnum'} = 12;
	$msg{'task'} = "start_dmtcp_coordinator";
	$h->master_write_task(\%msg);

	# can the slave read it?
	print "Slave reading tasks...\n";
	@tasks = $h->slave_read_tasks();
	foreach $ref (@tasks) {
		foreach $k (keys %{$ref}) {
			print "\t$k = $ref->{$k}\n";
		}
		print "...\n";
	}

	# can the slave write the result?
	print "Slave writing result...\n";
	%msg = ();
	$msg{'task_seqnum'} = 12;
	$msg{'port'} = 43212;
	$h->slave_write_result(\%msg);

	# can the master read the result?
	print "Master reading result...\n";
	my @results;
	@results = $h->master_read_results();
	foreach $ref (@results) {
		foreach $k (keys %{$ref}) {
			print "\t$k = $ref->{$k}\n";
		}
		print "...\n";
	}
}

exit main();

