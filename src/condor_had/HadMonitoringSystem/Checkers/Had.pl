#!/bin/env perl

use Switch;

my $hadMonitoringSystemDirectory = $ENV{MONITORING_HOME} || $ENV{PWD};
# For DOWN_STATUS and EXITING_EVENT
use Common qw(DOWN_STATUS EXITING_EVENT);

# Regular expressions, determining the type of event
my $exitingRegEx  = 'EXITING WITH STATUS';
my $startingRegEx = 'PASSWD_CACHE_REFRESH is undefined, using default value of';
my $leaderRegEx   = 'go to <LEADER_STATE>';
my $stateRegEx    = 'go to <PASSIVE_STATE>';

# Events
#use constant EXITING_EVENT  => 'Exiting';
use constant STARTING_EVENT => 'Starting';
use constant LEADER_EVENT   => 'Leader';
use constant BACKUP_EVENT   => 'Backup';

# Status
#use constant DOWN_STATUS        => 'Down';
use constant LEADER_STATUS      => 'Leader';
use constant BACKUP_STATUS      => 'Backup';

# Error messages
use constant NO_HAD_LEADER_MESSAGE            => 'no HAD leader found in pool';
use constant MORE_THAN_ONE_HAD_LEADER_MESSAGE => 'more than one HAD leader found in pool';

# Various constants
use constant STABILIZATION_TIME => 78;

sub HadValidate
{
	my ($epocheStartTime, $epocheEndTime, @statusVector) = (@_);

	my $leadersNumber = 0;
	my $downNumber    = 0;
	my $message       = "";

	# Scanning the status vector and retrieving analysis information
	foreach my $machineIndex (0 .. $#statusVector)
	{
		$leadersNumber ++ if($statusVector[$machineIndex] eq LEADER_STATUS);
		$downNumber    ++ if($statusVector[$machineIndex] eq DOWN_STATUS);
	}

	# Normal state of the system, all the fears for potential errors are eliminated:
	# Either the leader is alone or all the pool is down or the epoche interval is
	# less than the stabilization time
	return "" if($leadersNumber == 1 ||
                     $downNumber    == $#statusVector + 1 ||
                     $epocheEndTime - $epocheStartTime < STABILIZATION_TIME);

	$message .= NO_HAD_LEADER_MESSAGE            if($leadersNumber == 0);
	$message .= MORE_THAN_ONE_HAD_LEADER_MESSAGE if($leadersNumber > 1);
	$message .= " for more than " . STABILIZATION_TIME . " seconds " .
		    "(state: " . join(',', @statusVector) . ")";

	return $message;
}

sub HadDiscoverEvent
{
	my $line = shift;
	# Discovering starting
	return STARTING_EVENT if(grep(/$startingRegEx/, $line));
#        {
#                print $eventFileHandle $newLineChar . "$timestamp " . STARTING_EVENT;
#                return "\n";
#        }
        # Discovering graceful exiting
	return EXITING_EVENT if(grep(/$exitingRegEx/, $line));
#        {
#                print $eventFileHandle $newLineChar . "$timestamp " . EXITING_EVENT;
#                return "\n";
#        }
        # Discovering passing to leader state
	return LEADER_EVENT if(grep(/$leaderRegEx/, $line));
#        {
#                print $eventFileHandle $newLineChar . "$timestamp " . LEADER_EVENT;
#                return "\n";
#        }
        # Discovering passing to backup state
	return BACKUP_EVENT if(grep(/$stateRegEx/, $line));# grep(!/$leaderRegEx/, $line) &&
#        {
#                print $eventFileHandle $newLineChar . "$timestamp " . BACKUP_EVENT;
#                return "\n";
#        }
	return "";
}

sub HadApplyStatus
{
	my $event = shift;

	switch($event)
	{
		case EXITING_EVENT  { return DOWN_STATUS; }
		case STARTING_EVENT { return BACKUP_STATUS; }
		case LEADER_EVENT   { return LEADER_STATUS; }
		case BACKUP_EVENT   { return BACKUP_STATUS; }
		else                { die "No such event: " . $event . " - $!"; }
	}
}

sub HadGap
{
	my ($hadConnectionTimeout, $hadList) = `condor_config_val HAD_CONNECTION_TIMEOUT HAD_LIST`;

	chomp($hadConnectionTimeout);
	chomp($hadList);

	my $hadInterval = (2 * $hadConnectionTimeout * split(',', $hadList) + 1) * 2;

	return $hadInterval;
}
