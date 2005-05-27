#! /usr/bin/env perl

use strict;
use Cwd;

my $pid;
my $envvar;

my $opsys = $ARGV[0];
print "OPSYS = $opsys\n";

my $path;

$path = getcwd();
my $pout = "$path/parent";
my $cout = "$path/child";
my $count;

# parent shouldn't wait for any children
$SIG{'CHLD'} = "IGNORE";

open(POUT, "> job_core_child_cleanup.data") or die "Could not open file '$pout': $?";
print POUT "Parent's pid is $$\n\n";

$count = 0;
while ($count < 10) {
	$pid = fork();
	if ($pid == 0)
	{
		# child code....

		# child waits until a signal shows up
		$SIG{'INT'} = \&handler;
		$SIG{'HUP'} = \&handler;
		$SIG{'TERM'} = \&handler;

		while (1) { sleep 1; }

		exit 1;
	}

	# parent code...

	if (!defined($pid)) {
		die "some problem forking. Oh well.\n";
	}

	print POUT "Relationship: $$ created $pid\n";

	$count++;
}

#print POUT "Parent's environment is:\n";
#foreach $envvar (sort keys (%ENV)) {
	#print POUT "$envvar = $ENV{$envvar}\n";
#}
close(POUT);

exit 0;

sub handler
{
	my $sig = shift(@_);

	open(COUT, "> ${cout}_$$") or die "Could not open file '${cout}_$$': $?";
	print COUT "Child's pid is $$\n";
	print COUT "Got signal $sig\n\n";
	print COUT "Child's environment is:\n";
	foreach $envvar (sort keys (%ENV)) {
		print COUT "$envvar = $ENV{$envvar}\n";
	}
	close(COUT);

	exit 0;
}
