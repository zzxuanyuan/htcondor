#! /usr/bin/perl -w
use strict;
use warnings;

################################################################################
# You would typically put this package in a file called "Ckpt/Condor.pm" and put
# "1;" at the end, but you don't need to.
package StaCkpt::Condor;

sub new {
	my($class) = shift;
	my(@other_arguments) = @_;
	my $h = bless {
		## This is just a hash, dump whatever you like
		## in here, possibly based on @other_arguments
		#name => $other_arguments[0],
		#count => $other_arguments[1],
		initialized => 0,
		running => undef,
	}, (ref($class) || $class);
	return  $h
}

sub init {
	my $this = shift;
	my(@other_arguments) = @_;
	$this->{'initialized'} = 1;
	return 1;
}

sub run {
	my $this = shift;
	my($executable_name, @args) = @_;
	if(not $this->{'initialized'}) {
		# This should probably be a "die" fatal error, but this is
		# just an example
		print "Warning: Can't run $executable_name: Not initialized\n";
		return 0;
	}
	$this->{'running'} = $executable_name;
	print "Running $executable_name\n";
	return 1;
}

1;
