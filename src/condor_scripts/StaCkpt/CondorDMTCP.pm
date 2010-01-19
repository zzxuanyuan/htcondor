#! /usr/bin/env perl

# XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX 
# This file is a previous iteration of how to run dmtcp tests under our
# test suite. 
# This file is defunct and is being replaced by the StaCkpt/DMTCP.pm file.
# It is only here for reference until this is accomplished.
# XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX 

##**************************************************************
##
## Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##	http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

use strict;
use warnings;

package CondorDMTCP;
push @INC, ".";
use CondorUtils;
use File::Path;
use IO::Pipe;
use POSIX ":sys_wait_h";
use Multiplex;

our $VERSION = '1.00';
use base 'Exporter';
our @EXPORT = qw(dmtcp_init dmtcp_run dmtcp_checkpoint dmtcp_fini);

$| = 1; # needed so pipe operations don't buffer and block.

my $mode = "parent"; # or "child"

##############################################################################

sub dmtcp_init
{
	my ($testname, $dist, $cdir) = @_;
	my %dmtcp;
	my $ckptdir = "$cdir/dmtcp-ckptdir-$testname-$$";
	my $port;
	my $rhash;
	
	# set up where it is
	$dmtcp{'dist'} = $dist;
	$dmtcp{'bindir'} = "$dist/bin";
	$dmtcp{'libdir'} = "$dist/lib";
	$dmtcp{'coord'} = "$dmtcp{'bindir'}/dmtcp_coordinator";
	$dmtcp{'com'} = "$dmtcp{'bindir'}/dmtcp_command";
	$dmtcp{'ckpt'} = "$dmtcp{'bindir'}/dmtcp_checkpoint";

	# dmtcp croaks when trying to restart this fd, we don't need it anyways.
	dmtcp_close_gcb_inherit_fd();

	# mkdir a location to run the dmtcp_coordinator, this is where the
	# dmtcp_restart_script.sh file will be written.
	mkdir $ckptdir, 0755;
	print "Created location $ckptdir to hold the various checkpoint files.\n";

	# start the dmtcp_coordinator, and grab the random port it spits out,
	# storing it for later use.
	$rhash = runcmd("$dmtcp{'coord'} --port 0 --exit-on-last --background 2>&1 | grep Port: | /bin/sed -e \"s/Port://g\" -e \"s/[ \\t]//g\"");

	$port = $rhash->{'stdout'}[0];
	chomp $port;

	# race condition some time for it to wake up fully
	sleep 1;

	print "Started up dmtcp_coordinator on port $port\n";

	# shove discovered stuff into the hash table 
	$dmtcp{'testname'} = $testname;
	$dmtcp{'ckptdir'} = $ckptdir;
	$dmtcp{'port'} = $port;
	
	return \%dmtcp;
}

sub dmtcp_fini
{
	my ($dref, $cleanup) = @_;
	my $rhash;

	# shut down the dmtcp_coordinator, which might not be there since we told
	# it to exit on last. However, there could have been a error starting up
	# the job and it never connected regardless of --exit-on-last is present
	# or not, so we try anyway.
	print
		"Attempting to shut down dmtcp_coordinator on port $dref->{'port'}.\n";
	print "We expect any result as this may or may not fail.\n";

	$rhash = runcmd("$dref->{'com'} --quiet --port $dref->{'port'} -q",
		{expect_result => \&ANY});
	
	# XXX check to see if my chldren are still up and running, ensure to kill
	# them if so.

	# clean up the ckptdir crap left around
	if (-d $dref->{'ckptdir'}) {
		if (!defined($cleanup)) {
			print "Cleaning up temporary ckptdir $dref->{'ckptdir'}.\n";
			rmtree($dref->{'ckptdir'});
		} else {
			print "NOT cleaning up temporary ckptdir $dref->{'ckptdir'}.\n";
		}
	}
}

sub dmtcp_close_gcb_inherit_fd
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

# Ask the dmtcp_coordinator to take a checkpoint, but leave the process running.
sub dmtcp_checkpoint
{
	my ($dref) = @_;
	my $href;

	print "CondorDMTCP: Checkpointing the process.\n";

	# checkpoint the process
	$href = runcmd("$dref->{'com'} --port $dref->{'port'} c");

	# race condition a sleep
	sleep 10;

	# get the status
	print "CondorDMTCP: Status of Coordinator.\n";
	$href = runcmd("$dref->{'com'} --port $dref->{'port'} s");

	dmtcp_verify_checkpoint($dref);
}

# Ask the dmtcp_coordinator to take a checkpoint, and then kill off the nodes
# in the process, but leave the dmtcp_coorindator running.
sub dmtcp_checkpoint_and_exit
{
	my ($dref) = @_;
	my $href;

	dmtcp_checkpoint($dref);

	# kill the nodes
	print "CondorDMTCP: Killing coordinator, if present\n";
	$href = runcmd("$dref->{'com'} --port $dref->{'port'} q");
}

# see if there is a set of checkpoint files in addition to a
# dmtcp_restart_script.sh file in the appropriate place
sub dmtcp_verify_checkpoint
{
	my ($dref) = @_;

	# check to see if the checkpoint happened
	if ( -f "$dref->{'ckptdir'}/dmtcp_restart_script.sh") {
		# This isn't very through in determining of the ckpt happened...
		print "CondorDMTCP: Found a restart script, good.\n";
		$dref->{'ckpt_ok'} = 1;
	} else {
		print "CondorDMTCP: Failed to find a restart script, BAD.\n";
		$dref->{'ckpt_ok'} = 0;
	}
}

# This starts up a job the first time, pointing it to the correct
# dmtcp_coordinator and place to store checkpoint files.
# Or restarts it if a checkpoint had been taken earlier.
sub dmtcp_run
{
	my ($dref, $command) = @_;
	my $rhash;
	my $child_pid;
	my ($pin, $pout);
	my $line;
	my $kid;
	my $mux;

	pipe FROM_PARENT, TO_CHILD;
	$| = 1;

	# This will be set up different in the child and the parent
	$mux = new IO::Multiplex();
	$mux->set_callback_object(__PACKAGE__);

	# fork, have the child either initially start it, or restart it
	# the parent continues and monitors what is going on.
	# Man, handle SIGCHLD and other usual process management crap.

	print "Before Fork Parent: $$\n";

	$child_pid = fork();
	if (!defined($child_pid)) {
		print "Out of resources!\n";
	} elsif ($child_pid == 0) {
		# child
		print "Child Forked: $$\n";
		close TO_CHILD;

		$mux->add(\*FROM_PARENT);

		$mux->loop();

		exit 42;
	}

	print "Parent: $$ (Child: $child_pid)\n";
	close FROM_PARENT;

	$mux->set_timeout(\*TO_CHILD, .10);
	$mux->add(\*TO_CHILD);
	$mux->write(\*TO_CHILD, "->Quit\n");
	#mux->loop;

	print "Wait for child to exit\n";
	do {
		$kid = waitpid($child_pid, WNOHANG);
	} while ($kid == 0);

	print "Reaped child pid $kid status: " . ($?>>8) . "\n";

	return;

	# Hrm...
	system("$dref->{'ckpt'} --port $dref->{'port'} -j $command 1>$dref->{'testname'}.out 2>$dref->{'testname'}.err &");

}

sub mux_input 
{
	my $self = shift;
	my $mux  = shift;
	my $fh   = shift;
	my $data = shift;

	my $input;

	# Process each line in the input, leaving partial lines
	# in the input buffer
	while ($$data =~ s/^(.*?\n)//) {
		$input = $1;

		if ($mode =~ /"parent"/) {
			print "Parent: mux_input() read: $input\n";
			$mux->endloop;
		} else {
			print "Child: mux_input() read: $input\n";
			if ($input =~ m/->Quit/) {
				$mux->endloop;
			}
		}
	}
}

1;



