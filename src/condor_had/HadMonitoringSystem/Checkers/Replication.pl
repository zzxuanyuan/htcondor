#!/bin/env perl

use Switch;

my $hadMonitoringSystemDirectory = $ENV{MONITORING_HOME} || $ENV{PWD};
# For DOWN_STATUS and EXITING_EVENT
use Common qw(DOWN_STATUS EXITING_EVENT MAX_INT FindTimestamp ConvertTimestampToTime $replicationInterval);

# Regular expressions, determining the type of event
my $exitingRegEx            = 'EXITING WITH STATUS';
my $startingRegEx           = 'PASSWD_CACHE_REFRESH is undefined, using default value of';
my $requestingRegEx         = 'go to <VERSION_REQUESTING>';
my $downloadingRegEx        = 'go to <VERSION_DOWNLOADING>';
my $leaderRegEx             = 'go to <REPLICATION_LEADER>';
my $backupRegEx             = 'go to <BACKUP>';
my $stateFileSaveRegEx      = 'Version::save started';
my $rotateStateFileRegEx    = 'FilesOperations::rotateFile.+Accountantnew.+down started';

# Events
#use constant EXITING_EVENT    => 'Exiting';
use constant STARTING_EVENT    => 'Starting';
use constant LEADER_EVENT      => 'Leader';
use constant BACKUP_EVENT      => 'Backup';
use constant REQUESTING_EVENT  => 'Requesting';
use constant DOWNLOADING_EVENT => 'Downloading';
use constant MODIFYING_EVENT   => 'Modifying';

# Status
#use constant DOWN_STATUS        => 'Down';
use constant LEADER_STATUS      => 'Leader';
use constant BACKUP_STATUS      => 'Backup';
use constant JOINING_STATUS     => 'Joining';

# Error messages
use constant NO_RD_LEADER_MESSAGE            => 'no RD leader found in pool';
use constant MORE_THAN_ONE_RD_LEADER_MESSAGE => 'more than one RD leader found in pool';
use constant TIMESTAMPS_DIFFERENCE_MESSAGE   => 'earliest and latest timestamps of state files replicas differ more than ' .
						'twice as REPLICATION_INTERVAL seconds';

# Various constants
use constant STABILIZATION_TIME => 78;

sub ReplicationValidate
{
	my ($epocheStartTime, $epocheEndTime, @statusVector) = (@_);

	my $leadersNumber  = 0;
	my $downNumber     = 0;
	my $message        = "";
#	my @lastStateFileModificationTimestamps = @{$refLastStateFileModificationTimestamps};

	# Scanning the status vector and retrieving analysis information
	foreach my $machineIndex (0 .. $#statusVector)
	{
		$leadersNumber ++ if($statusVector[$machineIndex] eq LEADER_STATUS);
		$downNumber    ++ if($statusVector[$machineIndex] eq DOWN_STATUS);
	}

#	my $earliestStateFileModificationTimestamp = 0;
#	my $latestStateFileModificationTimestamp   = MAX_INT;
#	# Scanning the state file modifications and finding the earliest and the latest timestamps
#	foreach my $lastStateFileModificationTimestamp (@lastStateFileModificationTimestamps)
#	{
#		# Timestamps could be undefined due to machines shutdowns
#		next if !defined($lastStateFileModificationTimestamp);
#
#		$latestStateFileModificationTimestamp = $lastStateFileModificationTimestamp
#			if($lastStateFileModificationTimestamp < $latestStateFileModificationTimestamp);
#		$earliestStateFileModificationTimestamp = $lastStateFileModificationTimestamp
#			if($lastStateFileModificationTimestamp > $earliestStateFileModificationTimestamp);
#	}
#	`echo $earliestStateFileModificationTimestamp $latestStateFileModificationTimestamp >> /tmp/modification.log`;
#	# State files must differ no more than REPLICATION_INTERVAL
#	if($earliestStateFileModificationTimestamp - $latestStateFileModificationTimestamp > $replicationInterval)
#	{
#		my $timestampsAsString = "";
#		my $comma              = "";
#
#		foreach my $lastStateFileModificationTimestamp (@lastStateFileModificationTimestamps)
#		{
#			$timestampsAsString .= $comma . "down" 
#				if !defined($lastStateFileModificationTimestamp);
#			$timestampsAsString .= $comma . $lastStateFileModificationTimestamp 
#				if defined($lastStateFileModificationTimestamp);
#			$comma = ',';
#		}
#		return TIMESTAMPS_DIFFERENCE_MESSAGE . ' ' . $timestampsAsString;
#	}

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
	my $line                                  = shift;
#	my $refLastStateFileModificationTimestamp = shift;
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

	# Additional parameters include calculating the timestamp of accountant file modification
#	$$refLastStateFileModificationTimestamp = &FindTimestamp($line, 0) 
#		if(grep(/$rotateStateFileRegEx/, $line));
	return MODIFYING_EVENT if(grep(/$rotateStateFileRegEx/, $line) ||
				  grep(/$stateFileSaveRegEx/  , $line));
	return "";
}

sub ReplicationApplyStatus
{
	my $event          = shift;
	my $previousStatus = shift;
	my $eventTimestamp = shift;
	# Additional parameters include calculating the timestamp of accountant file modification
	my $refLastStateFileModificationTimestamps = shift;
	my $timestampIndex = shift;
	my $returnedStatus = '';
	my $returnedMessage = '';

	switch($event)
	{
		case EXITING_EVENT     { undef $refLastStateFileModificationTimestamps->[$timestampIndex];
					 $returnedStatus = DOWN_STATUS; }
		case STARTING_EVENT    { $returnedStatus = JOINING_STATUS; }
		case REQUESTING_EVENT  { $returnedStatus = JOINING_STATUS; }
		case DOWNLOADING_EVENT { $returnedStatus = JOINING_STATUS; }
		case BACKUP_EVENT      { $returnedStatus = BACKUP_STATUS; }
		case LEADER_EVENT      { $returnedStatus =  LEADER_STATUS; }
		case MODIFYING_EVENT   { $refLastStateFileModificationTimestamps->[$timestampIndex] = $eventTimestamp;
					 $returnedMessage = &CheckTimestampsDifference(@{$refLastStateFileModificationTimestamps});
					 $returnedMessage = 'At ' . &ConvertTimestampToTime($eventTimestamp) . ': ' . $returnedMessage
						if $returnedMessage ne '';

					 $returnedStatus = $previousStatus; }
		else                   { die "No such event: " . $event . " - $!"; }
	}

	return ($returnedStatus, $returnedMessage);
}

sub ReplicationGap
{
#        my $replicationInterval = `condor_config_val REPLICATION_INTERVAL`;
#        
#        chomp($replicationInterval);
#        
        return $replicationInterval;
}

################################## Auxiliary functions #####################################
sub CheckTimestampsDifference
{	
	my (@lastStateFileModificationTimestamps) = (@_);

        my $earliestStateFileModificationTimestamp = 0;
        my $latestStateFileModificationTimestamp   = MAX_INT;
        # Scanning the state file modifications and finding the earliest and the latest timestamps   
        foreach my $lastStateFileModificationTimestamp (@lastStateFileModificationTimestamps)
        {
                # Timestamps could be undefined due to machines shutdowns
                next if !defined($lastStateFileModificationTimestamp);
                               
                $latestStateFileModificationTimestamp = $lastStateFileModificationTimestamp
                        if($lastStateFileModificationTimestamp < $latestStateFileModificationTimestamp);
                $earliestStateFileModificationTimestamp = $lastStateFileModificationTimestamp
                        if($lastStateFileModificationTimestamp > $earliestStateFileModificationTimestamp);
        }
	# For debugging purposes
#        `echo $earliestStateFileModificationTimestamp $latestStateFileModificationTimestamp >> /tmp/modification.log`;
        # State files must differ no more than twice as REPLICATION_INTERVAL
        if($earliestStateFileModificationTimestamp - $latestStateFileModificationTimestamp > 2 * $replicationInterval)
        {
                my $timestampsAsString = "";
                my $comma              = "";
       
                foreach my $lastStateFileModificationTimestamp (@lastStateFileModificationTimestamps)
                {
                        $timestampsAsString .= $comma . "down"
                                if !defined($lastStateFileModificationTimestamp);
                        $timestampsAsString .= $comma . &ConvertTimestampToTime($lastStateFileModificationTimestamp)
                                if defined($lastStateFileModificationTimestamp);
                        $comma = ',';
               	}
        	return TIMESTAMPS_DIFFERENCE_MESSAGE . ' ' . $timestampsAsString;
       	}
	return "";
}
############################## End of auxiliary functions ##################################
