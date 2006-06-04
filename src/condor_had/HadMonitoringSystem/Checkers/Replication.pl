#!/bin/env perl

use Switch;

my $hadMonitoringSystemDirectory = $ENV{MONITORING_HOME} || $ENV{PWD};
# For DOWN_STATUS and EXITING_EVENT
use Common qw(DOWN_STATUS EXITING_EVENT);

# Regular expressions, determining the type of event
my $exitingRegEx            = 'EXITING WITH STATUS';
my $startingRegEx           = 'PASSWD_CACHE_REFRESH is undefined, using default value of';
my $requestingRegEx         = 'go to <VERSION_REQUESTING>';
my $downloadingRegEx        = 'go to <VERSION_DOWNLOADING>';
my $leaderRegEx             = 'go to <REPLICATION_LEADER>';
my $backupRegEx             = 'go to <BACKUP>';

# Events
#use constant EXITING_EVENT    => 'Exiting';
use constant STARTING_EVENT    => 'Starting';
use constant LEADER_EVENT      => 'Leader';
use constant BACKUP_EVENT      => 'Backup';
use constant REQUESTING_EVENT  => 'Requesting';
use constant DOWNLOADING_EVENT => 'Downloading';

# Status
#use constant DOWN_STATUS        => 'Down';
use constant LEADER_STATUS      => 'Leader';
use constant BACKUP_STATUS      => 'Backup';
use constant JOINING_STATUS     => 'Joining';

# Error messages
use constant NO_RD_LEADER_MESSAGE            => 'no RD leader found in pool';
use constant MORE_THAN_ONE_RD_LEADER_MESSAGE => 'more than one RD leader found in pool';

# Various constants
use constant STABILIZATION_TIME => 78;

sub ReplicationValidate
{
	my ($epocheStartTime, $epocheEndTime, @statusVector) = (@_);

	my $leadersNumber  = 0;
	my $downNumber     = 0;
	my $message        = "";

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

	$message .= NO_RD_LEADER_MESSAGE            if($leadersNumber == 0);
	$message .= MORE_THAN_ONE_RD_LEADER_MESSAGE if($leadersNumber > 1);
	$message .= " for more than " . STABILIZATION_TIME . " seconds " .
		    "(state: " . join(',', @statusVector) . ")";

	return $message;
}

sub ReplicationDiscoverEvent
{
	my $line = shift;
	# Discovering starting
	return STARTING_EVENT if(grep(/$startingRegEx/, $line));
        # Discovering graceful exiting
	return EXITING_EVENT if(grep(/$exitingRegEx/, $line));
	# Discovering joining to the pool events
	return REQUESTING_EVENT if(grep(/$requestingRegEx/, $line));
	return DOWNLOADING_EVENT if(grep(/$downloadingRegEx/, $line));
        # Discovering passing to backup state
	return BACKUP_EVENT if(grep(/$backupRegEx/, $line));
        # Discovering passing to leader state
        return LEADER_EVENT if(grep(/$leaderRegEx/, $line));
	return "";
}

sub ReplicationApplyStatus
{
	my $event = shift;

	switch($event)
	{
		case EXITING_EVENT     { return DOWN_STATUS; }
		case STARTING_EVENT    { return JOINING_STATUS; }
		case REQUESTING_EVENT  { return JOINING_STATUS; }
		case DOWNLOADING_EVENT { return JOINING_STATUS; }
		case BACKUP_EVENT      { return BACKUP_STATUS; }
		case LEADER_EVENT      { return LEADER_STATUS; }
		else                   { die "No such event: " . $event . " - $!"; }
	}
}

sub ReplicationGap
{
        my $replicationInterval = `condor_config_val REPLICATION_INTERVAL`;
        
        chomp($replicationInterval);
        
        return $replicationInterval;
}

