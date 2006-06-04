#!/bin/env perl

################################ Main script body #####################################
use IO::File;
use File::Copy;
use Time::Local;
use Switch;
use POSIX qw(strftime);

# Directories and files paths
my $hadMonitoringSystemDirectory = $ENV{MONITORING_HOME} || $ENV{PWD};
my $eventFilesDirectory          = $hadMonitoringSystemDirectory . "/EventFiles";
my $errorLogsDirectory           = $hadMonitoringSystemDirectory . "/ErrorLogs";
my $outputLogsDirectory          = $hadMonitoringSystemDirectory . "/OutputLogs";
my $daemonLogsDirectory          = $hadMonitoringSystemDirectory . "/DaemonLogs";
my $archiveLogsDirectory         = $hadMonitoringSystemDirectory . "/ArchiveLogs";
my $stateFilePath                = $hadMonitoringSystemDirectory . "/State";
my $configurationFilePath        = $hadMonitoringSystemDirectory . "/Configuration";
my $currentTime                  = strftime "%d-%m-%Y-%H-%M-%S", localtime;
my $errorLogPath                 = $errorLogsDirectory  . "/ErrorLog-$currentTime";
my $outputLogPath                = $outputLogsDirectory . "/OutputLog-$currentTime";
my $epocheLogPath                = $outputLogsDirectory . "/EpocheLog-$currentTime";
my $eventFileTemplatePath        = $eventFilesDirectory . "/EventFile-$currentTime";
my $consolidatedErrorLogPath     = $errorLogsDirectory  . "/ConsolidatedErrorLog";
my $consolidatedOutputLogPath    = $outputLogsDirectory . "/ConsolidatedOutputLog";
my $consolidatedEpocheLogPath    = $outputLogsDirectory . "/ConsolidatedEpocheLog";
my $errorHistoryLogPath          = $errorLogsDirectory  . "/ErrorHistoryLog";
my $outputHistoryLogPath         = $outputLogsDirectory . "/OutputHistoryLog";
my $epocheHistoryLogPath         = $outputLogsDirectory . "/EpocheHistoryLog";

# Regular expressions, determining the type of event
my $exitingRegEx  = 'EXITING WITH STATUS';
my $startingRegEx = 'PASSWD_CACHE_REFRESH is undefined, using default value of'; 
my $leaderRegEx   = 'go to <LEADER_STATE>';
my $stateRegEx    = 'go to <PASSIVE_STATE>';
        
# Events
use constant EXITING_EVENT  => 'Exiting';
use constant STARTING_EVENT => 'Starting';
use constant LEADER_EVENT   => 'Leader';
use constant BACKUP_EVENT   => 'Backup';

# Status
use constant DOWN_STATUS    => 'Down';
use constant LEADER_STATUS  => 'Leader';
use constant BACKUP_STATUS  => 'Backup';

# Various constants                
use constant STABILIZATION_TIME     => 78;
use constant TRUE                   => 1;
use constant FALSE                  => 0;
use constant MAX_INT                => 999999999999;
use constant FICTIVE_SENDER_ADDRESS => 'had_monitoring_system@cs';
use constant LINE_SEPARATOR         => "*********************************************\n";

# Error numbers (must be positive)
use constant NO_NEGOTIATOR            => 1;
use constant MORE_THAN_ONE_NEGOTIATOR => 2;

# Error messages
use constant NO_NEGOTIATOR_MESSAGE            => 'no negotiator found in pool';
use constant MORE_THAN_ONE_NEGOTIATOR_MESSAGE => 'more than one negotiator found in pool';

# Monitoring data structures
my @eventFilesHandles               = ();
my @statusVector                    = ();
my @timestampsVector                = ();
my @eventsVector                    = ();
my @initialStatus                   = ();
my @potentialErrorStatus            = ();
my $isPotentialError                = FALSE;
my $potentialErrorStartingTimestamp = 0;
my $errorNumber                     = 0;
my $totalErrorsNumber               = 0;
my $errorTimestamp                  = 0;
my $currentTimestamp                = 0;
my $previousTimestamp               = 0;
my $lastStatusTimestamp             = 0;
my $monitoringSystemCounter         = 0;

# Configuration file entries
my $smtpServer                      = "";
#my $reportRecipients                = "";
my $consolidatedReportRecipients    = "";
my $errorReportRecipients           = "";
my $isReportSent                    = TRUE;
my $isNoErrorReportSent             = TRUE;
my $consolidatedReportFrequency     = "";
my $storeOldArchivesDays            = 7;

# Loading Condor pool parameters
my @condorParameters = `condor_config_val HAD_LIST HAD_USE_PRIMARY`;
chomp(@condorParameters);
my $hadList       = $condorParameters[0];
my $isPrimaryUsed = $condorParameters[1];

&LoadConfiguration();

my @logFilePaths = &FetchLogs();
# For debugging
#my @logFilePaths = 
#	('/home/sharov/HadMonitoringSystem/DaemonLogs.bak/HADLog.<132.68.37.112:60104>',
#	 '/home/sharov/HadMonitoringSystem/DaemonLogs.bak/HADLog.<132.68.37.124:60105>',
#	 '/home/sharov/HadMonitoringSystem/DaemonLogs.bak/HADLog.<132.68.37.126:60106>');

&GenerateEventFiles(@logFilePaths);
&Initialize(\@eventFilesHandles, \@statusVector, 
	    \@timestampsVector , \@eventsVector, $#logFilePaths);
while(!&IsFinished(@timestampsVector))
{
	# Finds the earliest events of 'timestampsVector'
	my @nextEventsIndices = &FindNextEvents(@timestampsVector);
	
	$currentTimestamp = $timestampsVector[$nextEventsIndices[0]];

	last if $currentTimestamp == MAX_INT;
	# Applies status for machines, specified by 'nextEventsIndices' and
	# reads the next events for these files
	my @oldStatusVector = @statusVector;

	&ApplyStatus(\@nextEventsIndices, \@eventFilesHandles,
		     \@statusVector     , \@timestampsVector ,
		     \@eventsVector);
	if("@oldStatusVector" ne "@statusVector")
	{
		open(EPOCHE_LOG, ">> $epocheLogPath") or die "open: $!";
	        print EPOCHE_LOG &ConvertTimestampToTime($previousTimestamp) . " - " .
				 &ConvertTimestampToTime($currentTimestamp) . ": " . 
				 join(',', @oldStatusVector) . "\n";
        	close(EPOCHE_LOG) or die "close: $!";
		$previousTimestamp = $currentTimestamp;
	}

	if(!&CheckValidity(\@statusVector                   , \$isPotentialError, 
			   \$potentialErrorStartingTimestamp, \@potentialErrorStatus,
			   \$errorNumber                    , $currentTimestamp))
	{
		&ReportError($errorNumber, $potentialErrorStartingTimestamp, $errorLogPath);
		$totalErrorsNumber ++;
	}
}
# Registering the last epoche of this run
if($previousTimestamp != $currentTimestamp)
{
        open(EPOCHE_LOG, ">> $epocheLogPath") or die "open: $!";
        print EPOCHE_LOG &ConvertTimestampToTime($previousTimestamp) . " - " .
                         &ConvertTimestampToTime($currentTimestamp) . ": " .
                         join(',', @statusVector) . "\n";
        close(EPOCHE_LOG) or die "close: $!";
	$previousTimestamp = $currentTimestamp;
}


&Finalize(\@eventFilesHandles, \@statusVector, $currentTimestamp);

# Prepending initial information for the consolidated epoche log
# Consolidated epoche log is sent in any case, even if nothing happened in the pool at that period of time
if(! -f $consolidatedEpocheLogPath)
{
	open(CONSOLIDATED_EPOCHE_LOG, ">> $consolidatedEpocheLogPath") or die "open: $!";
	print CONSOLIDATED_EPOCHE_LOG "Consolidated epoche log:\n\n";
        print CONSOLIDATED_EPOCHE_LOG 'Initial state: ' . &ConvertTimestampToTime($lastStatusTimestamp) .
                                      ': ' . join(',', @initialStatus) . "\n";
        close(CONSOLIDATED_EPOCHE_LOG);
}
# Prepending initial information for the consolidated error log
if(-f $errorLogPath && ! -f $consolidatedErrorLogPath)
{
	open(CONSOLIDATED_ERROR_LOG, ">> $consolidatedErrorLogPath") or die "open: $!";
        print CONSOLIDATED_ERROR_LOG "Consolidated error log:\n\n";
        close(CONSOLIDATED_ERROR_LOG);
}

# Appends the created error and output log files to the global history logs   
if(-f $outputLogPath)
{
        AppendHistory($outputLogPath, $outputHistoryLogPath);  
        AppendHistory($outputLogPath, $consolidatedOutputLogPath);
}   

if(-f $epocheLogPath)
{
	&AppendHistory($epocheLogPath, $epocheHistoryLogPath);
	&AppendHistory($epocheLogPath, $consolidatedEpocheLogPath);
}

if(-f $errorLogPath)
{
	&AppendHistory($errorLogPath, $errorHistoryLogPath);
	&AppendHistory($errorLogPath, $consolidatedErrorLogPath);

	&SendReport("Errors were discovered while running the HAD Monitoring System.\n" . 
		    "Find the error log attached.")
		if $isReportSent eq TRUE &&
           	   $monitoringSystemCounter % $consolidatedReportFrequency != 0;
}

# Collect unused logs according to the garbage collector policy 
# and archive them before anything has been deleted
&RemoveLogs();

if($isReportSent eq TRUE && $monitoringSystemCounter % $consolidatedReportFrequency == 0)
{
	# Appending the final status of the pool
	open(CONSOLIDATED_EPOCHE_LOG, ">> $consolidatedEpocheLogPath") or die "open: $!";
	print CONSOLIDATED_EPOCHE_LOG LINE_SEPARATOR;
        print CONSOLIDATED_EPOCHE_LOG LINE_SEPARATOR;
	print CONSOLIDATED_EPOCHE_LOG 'Final state: ' . &ConvertTimestampToTime($currentTimestamp) .
				      ': ' . join(',', @statusVector) . "\n";
	close(CONSOLIDATED_EPOCHE_LOG);

	&SendConsolidatedReport("Find the relevant history messages attached.");
	# Start collecting the logs for the next consolidated report from the scratch
	unlink($consolidatedErrorLogPath);
	unlink($consolidatedOutputLogPath);
	unlink($consolidatedEpocheLogPath);
}

################################ End of main script body ###################################

################################## Auxiliary functions #####################################

############################################################################################
## Function    : ReturnConfigurationInformation                                            #
## Return value: $configurationInformation - the Condor pool configuration information     #
## Description : returns Condor pool configuration information                             #
############################################################################################
sub ReturnConfigurationInformation
{
       	my $configurationInformation = 
	       "Configuration information:\n" .
               "HAD_LIST (in this order HAD states will appear in the report) - " . $hadList . "\n" .
               "HAD_USE_PRIMARY - " . $isPrimaryUsed . "\n";

        if(uc($isPrimaryUsed) eq 'TRUE')
        {
                my @hadAddresses = split(',', $hadList);

                $configurationInformation .= "Primary HAD - " . $hadAddresses[0]  . "\n";
        }
        $configurationInformation .= "\n";

	return $configurationInformation;
}

############################################################################################
## Function    : AppendConfigurationInformationToLog                                       #
## Arguments   : $logPath - the log file, to which the information will be appended        #
## Description : appends Condor pool configuration information to the specified log        #
############################################################################################
sub AppendConfigurationInformationToLog
{
	my $logPath = shift;

        open(LOG, ">> $logPath") or die "open: $!";
        print LOG &ReturnConfigurationInformation();
	close(LOG);	
}

############################################################################################
## Function    : ConvertTimestampToTime                                                    #
## Arguments   : $timestamp - the timestamp to convert                                     #
## Return value: the time string, in "%a %b %e %H:%M:%S %Y" format                         #
## Description : converts specified timestamp to string and returns it                     #
############################################################################################
sub ConvertTimestampToTime
{
	my $timestamp = shift;

	return strftime "%a %b %e %H:%M:%S %Y", localtime($timestamp);
}

############################################################################################
## Function    : RemoveLogs                                                                #
## Description : removes unnecessary logs according to the garbage collector policy        #
############################################################################################
sub RemoveLogs
{
	# Compressing all the logs always at the end of the monitoring session
	mkdir $archiveLogsDirectory;

	use Archive::Zip qw( :ERROR_CODES :CONSTANTS );

	my $zip = Archive::Zip->new();

	$zip->addTree("$eventFilesDirectory",  'EventFiles');
	$zip->addTree("$errorLogsDirectory" ,  'ErrorLogs');
     	$zip->addTree("$outputLogsDirectory",  'OutputLogs');
	$zip->addTree("$daemonLogsDirectory",  'DaemonLogs');
	$zip->addFile("$stateFilePath",        'State');
	$zip->addFile("$configurationFilePath", 'Configuration');

	$zip->writeToFileNamed("$archiveLogsDirectory/Archive-$currentTime.zip") == AZ_OK or die "zip: $!";

	my @filePaths = glob("$eventFilesDirectory/* " . 
			     "$errorLogsDirectory/ErrorLog-* " .
			     "$outputLogsDirectory/OutputLog-* " .
			     "$outputLogsDirectory/EpocheLog-* " .
			     "$daemonLogsDirectory/HADLog* ");

	# Removing all the logs, produced by the monitoring system during the last session        
        foreach my $filePath (@filePaths)
        {
                unlink($filePath) or warn "unlink: $!";
        }
	# Removing old zipped tars
	
	# Finding all the archives
	my @archivePaths = glob("$archiveLogsDirectory/Archive-*.zip");

	foreach my $archivePath (@archivePaths)
	{
		$archivePath =~ /Archive-(\d+)-(\d+)-(\d+)-(\d+)-(\d+)-(\d+)/;
		my ($date, $month, $year, $hour, $minute, $second) = ($1, $2, $3, $4, $5, $6);

		# Month index is taken out of {0, 1, ..., 11} group
		$month --;
		my $archiveTime = timelocal($second ,$minute, $hour, $date, $month, $year);

		if(time() - $archiveTime > $storeOldArchivesDays * 86400)
		{
			unlink($archivePath) or warn "unlink: $!";
		}
	}
}
############################################################################################
## Function    : FetchLog                                                                  #
## Arguments   : $remoteDaemonSinfulString - address of remote daemon, from which the log  #                  
##                                           is to be fetched                              #
##               $remoteLogName - name of log to be fetched                                #
##               $localLogName - local path, to which the log is to be fetched             #
## Description : fetches a log from a remote daemon to the specified local path            #
############################################################################################
sub FetchLog
{
	my ($remoteDaemonSinfulString, $remoteLogName, $localLogName) = (@_);

	`condor_fetchlog \'$remoteDaemonSinfulString\' $remoteLogName > \'$localLogName\' 2>>$outputLogPath`;
	#open(LOG, "condor_fetchlog \'$remoteDaemonSinfulString\' $remoteLogName|");
        #open(HAD_LOG, "> $localLogName");
                
        #while(<LOG>)
        #{
        #	print HAD_LOG $_;
        #}
        #close(HAD_LOG);
        #close(LOG);
}
############################################################################################
## Function    : FetchLogs                                                                 #
## Return value: @logFilePaths - list of file paths, where the logs were fetched to        #
## Description : fetches all the necessary daemons' logs and returns their paths           #
############################################################################################
sub FetchLogs
{
	mkdir $daemonLogsDirectory;

	my @hadSinfulStrings = split(',', $hadList);
	my @logFilePaths;

	foreach my $hadSinfulString (@hadSinfulStrings)
	{
		my $logFilePath = "$daemonLogsDirectory/HADLog.$hadSinfulString-$currentTime";
		
		push(@logFilePaths, $logFilePath);
		FetchLog($hadSinfulString, 'HAD', $logFilePath);
		FetchLog($hadSinfulString, 'HAD.old', "$logFilePath.old");
	}
	return @logFilePaths;
}
############################################################################################
## Function    : AppendFile                                                                #  
## Arguments   : $fromPath - a path to a log, that is going to be appended                 #
##               $toPath - a path to a log, to which the information is going to be        #
##                         appended                                                        #
## Description : loads appends the file, specified by '$fromPath' to the file, specified by#
##		 '$toPath'                                                                 #
############################################################################################
sub AppendFile
{
	my ($fromPath, $toPath) = (@_);
 
        open(TO_LOG,   ">> $toPath");
        open(FROM_LOG, "< $fromPath");

        while(<FROM_LOG>)
        {
                print TO_LOG $_;
        }

        close(FROM_LOG);
        close(TO_LOG);
}
############################################################################################
## Function    : AppendHistory                                                             #
## Arguments   : $logPath - a path to a log, that is going to be appended                  #
##               $historyLogPath - a path to a log, to which the information is going to be#
##                                 appended                                                #
## Description : appends log file to history log file                                      #
############################################################################################
sub AppendHistory
{
	my ($logPath, $historyLogPath) = (@_);

	open(HISTORY_LOG, ">> $historyLogPath");

	print HISTORY_LOG LINE_SEPARATOR;
	print HISTORY_LOG LINE_SEPARATOR;	

	close(HISTORY_LOG);

	&AppendFile($logPath, $historyLogPath);
}
############################################################################################
## Function    : LoadConfiguration                                                         #
## Description : loads configuration file entries into global variables                    #
############################################################################################
sub LoadConfiguration
{
	open(CONFIGURATION_FILE, "< $configurationFilePath") or die("open: $!");
	my @configurationFileContents = <CONFIGURATION_FILE>;
	close(CONFIGURATION_FILE);

	chomp(@configurationFileContents);

	foreach my $configurationFileLine (@configurationFileContents)
	{
		my ($key, $value) = split('=', $configurationFileLine);
		
		switch($key)
        	{
                	case 'SMTP_SERVER'       { $smtpServer            = $value; }
			case 'ERROR_REPORT_RECIPIENTS'  
						 { $errorReportRecipients = $value; }
			case 'CONSOLIDATED_REPORT_RECIPIENTS'
						 { $consolidatedReportRecipients = $value; }	
                	# case 'REPORT_RECIPIENTS' { $reportRecipients = $value; }
			case 'IS_REPORT_SENT'    { $isReportSent = FALSE 
						   if grep(/$value/i, 'no'); }
			case 'IS_NO_ERROR_REPORT_SENT'
						 { $isNoErrorReportSent = FALSE
						   if grep(/$value/i, 'no'); }
			case 'CONSOLIDATED_REPORT_FREQUENCY'
						 { $consolidatedReportFrequency = $value; }
			case 'STORE_OLD_ARCHIVES_DAYS'
						 { $storeOldArchivesDays = $value; }
                	else                     { die "No such configuration entry: $key"; }
        	}
	}
}
############################################################################################
## Function    : SendConsolidatedReport                                                    #
## Arguments   : $messageBody - the text that will become the body of the                  #
##                              future SMTP message                                        #
## Description : sends bundle of reports about the logged activities to all whom it may    #
##               concern; this report contain the log of all the errors and the log of all #
##               the significant pool events                                               #
############################################################################################
sub SendConsolidatedReport
{
	return if $isNoErrorReportSent eq FALSE && $totalErrorsNumber == 0;

        my ($messageBody)             = (@_);
	my $totalErrorsNumberAsString = ($totalErrorsNumber == 0) ? 'no errors' : "$totalErrorsNumber errors";
        
        use MIME::Lite;
        use Net::SMTP;

        my $message = MIME::Lite->new(From    => FICTIVE_SENDER_ADDRESS,
                                      #To      => $reportRecipients,
				      To      => $consolidatedReportRecipients,
                                      Subject => "HAD Monitoring System Consolidated Report - $totalErrorsNumberAsString",
                                      Type    => 'multipart/mixed');
        $message->attach(Type => 'TEXT',
                         Data => $messageBody);
	$message->attach(Type => 'TEXT',
			 Data => &ReturnConfigurationInformation());
#	$message->attach(Type        => 'application/zip',
#                         Path        => $consolidatedOutputLogPath,
#                         Filename    => 'Consolidated output log.txt',
#                         Disposition => 'attachment') if -f $consolidatedOutputLogPath;
	# We send all the logs as one attachment
	my $unifiedLogPath = $consolidatedEpocheLogPath . ".tmp";

	&AppendFile($consolidatedEpocheLogPath, $unifiedLogPath) if -f $consolidatedEpocheLogPath;

	# Adding a separator between two logs in case, both of them exist
	if(-f $unifiedLogPath)
	{
		open(UNIFIED_LOG, ">> $unifiedLogPath");
		print UNIFIED_LOG "\n\n";
		close(UNIFIED_LOG);
	}
	&AppendFile($consolidatedErrorLogPath , $unifiedLogPath) if -f $consolidatedErrorLogPath;

	$message->attach(Type        => 'TEXT',
                         Path        => $unifiedLogPath,  
                         Filename    => 'History logs.txt') 
                         if -f $unifiedLogPath;

        MIME::Lite->send('smtp', $smtpServer, Timeout => 60);

        $message->send();

	unlink($unifiedLogPath);
}
############################################################################################
## Function    : SendReport                                                                #
## Arguments   : $messageBody - the text that will become the body of the                  #
##                              future SMTP message                                        #
## Description : sends report about inappropriate pool state to all whom it may concern    #
############################################################################################
sub SendReport
{
	my ($messageBody) = (@_);

	use MIME::Lite;
	use Net::SMTP;
	
	my $message = MIME::Lite->new(From    => FICTIVE_SENDER_ADDRESS, 
				      #To      => $reportRecipients, 
				      To      => $errorReportRecipients,
				      Subject => "HAD Monitoring System Report - $totalErrorsNumber errors",
				      Type    => 'multipart/mixed');
	$message->attach(Type => 'TEXT',
  			 Data => $messageBody);
        $message->attach(Type => 'TEXT',
                         Data => &ReturnConfigurationInformation());
	$message->attach(Type        => 'TEXT',
			 #Type        => 'application/zip',
		         Path        => $errorLogPath,
		         Filename    => 'Error log.txt');
#		         Disposition => 'attachment');

	MIME::Lite->send('smtp', $smtpServer, Timeout => 60);

	$message->send();
}
############################################################################################
## Function    : CheckValidity                                                             #
## Arguments   : $refStatusVector - reference to status array of all the pool machines     #
##               $refIsPotentialError - reference to boolean value, designating whether    #
##                                      potential error has happened or not                #
##               $refPotentialErrorStartingTimestamp - reference to timestamp, designating #
##                                                     the starting time of the potential  #
##                                                     error                               #
##               $refErrorNumber - reference to the number of the error, that happened     #
##               $currentTimestamp - current timestamp                                     #
## Return value: TRUE - if the specified status vector is valid, FALSE - otherwise         #
## Description : checks, whether the given machines status at some time point is valid;    #
##               if it is not sets the potential error to true and the potential timestamp #
##               to the current timestamp; if after STABILIZATION_TIME seconds the pool    #
##               hasn't yet stabilized, the FALSE is returned along with the error number  #
##               set to the value of error, that happened when the potential error was     #
##               discovered                                                                #
############################################################################################
sub CheckValidity
{
	# Retrieving arguments
	my $refStatusVector                    = shift;
	my $refIsPotentialError                = shift;
	my $refPotentialErrorStartingTimestamp = shift;
	my $refPotentialErrorStatus            = shift;
	my $refErrorNumber                     = shift;
	my $currentTimestamp                   = shift;

	# Local variables
	my @statusVector          = @{$refStatusVector};
	my $machinesNumber        = $#statusVector;
	my $leadersNumber         = 0;
	my $downNumber            = 0;
	my $errorNumberDiscovered = 0;
	my $potentialErrorTime    = &ConvertTimestampToTime($currentTimestamp);
	my $potentialError;

	# Scanning the status vector and retrieving analysis information
	foreach my $machineIndex (0 .. $machinesNumber)
	{
		$leadersNumber ++ if($refStatusVector->[$machineIndex] eq LEADER_STATUS);
		$downNumber    ++ if($refStatusVector->[$machineIndex] eq DOWN_STATUS);
	}
	my @statusVector = @{$refStatusVector};
	
	open(OUTPUT_LOG, ">> $outputLogPath") or die "open: $!";
	print OUTPUT_LOG "$potentialErrorTime: " . join(',', @statusVector) . "\n";
	close(OUTPUT_LOG) or die "close: $!";
	# Analyzing the scanned information

	# Normal state of the system, all the fears for potential errors are eliminated:
	# Either the leader is alone or all the pool is down
	if($leadersNumber == 1 || $downNumber == $machinesNumber + 1)
	{
		my $oldIsPotentialError              = $$refIsPotentialError;

		$$refIsPotentialError                = FALSE;
#		$$refPotentialErrorStartingTimestamp = 0;
#		$$refErrorNumber                     = 0;

#		return TRUE;
		return ($oldIsPotentialError eq TRUE &&
			$currentTimestamp - $$refPotentialErrorStartingTimestamp > 
			STABILIZATION_TIME) ? FALSE : TRUE;
	}
	# Otherwise there is a potential error in the system
	$errorNumberDiscovered = NO_NEGOTIATOR            if $leadersNumber == 0;
        $errorNumberDiscovered = MORE_THAN_ONE_NEGOTIATOR if $leadersNumber > 1; 

        # open(OUTPUT_LOG, ">> $outputLogPath") or die "open: $!";
         
        switch($errorNumberDiscovered)
        {
                case NO_NEGOTIATOR	      { $potentialError = NO_NEGOTIATOR_MESSAGE; }
                case MORE_THAN_ONE_NEGOTIATOR { $potentialError = MORE_THAN_ONE_NEGOTIATOR_MESSAGE; }
                else                          { die "No such error number: $errorNumberDiscovered"; }
        }

        #print OUTPUT_LOG "$potentialErrorTime: $potentialError\n";
        #close(OUTPUT_LOG) or die "close: $!";

	if($$refIsPotentialError == FALSE)
	{
		$$refPotentialErrorStartingTimestamp = $currentTimestamp;
		$$refErrorNumber                     = $errorNumberDiscovered;
		$$refIsPotentialError                = TRUE;
		@{$refPotentialErrorStatus}          = @statusVector;

		return TRUE;
	}
	elsif($currentTimestamp - $$refPotentialErrorStartingTimestamp > STABILIZATION_TIME)
	{
		$$refIsPotentialError                = FALSE;
#                $$refPotentialErrorStartingTimestamp = 0;

		return FALSE;
	}
	return TRUE;
}
############################################################################################
## Function    : ApplyStatus                                                               #
## Arguments   : $refNextEventsIndices - relevant arrays indices                           #
##               $refEventFilesHandles - all event files' handles array reference          #
##               $refStatusVector - reference to status array of all the pool machines     #
##               $refTimestampsVector - reference to possible next events timestamps array #
##               $refEventsVector - reference to possible next events array                #
## Description : modifies status of the specified machines and reads possible next events  #
##               and events timestamps to the appropriate vectors                          #
############################################################################################
sub ApplyStatus
{
	my $refNextEventsIndices = shift;
        my $refEventFilesHandles = shift;
        my $refStatusVector      = shift;
        my $refTimestampsVector  = shift;
	my $refEventsVector      = shift;
	my @nextEventsIndices    = @{$refNextEventsIndices};
	my $indicesNumber        = $#nextEventsIndices;
	
	foreach my $index (0 .. $indicesNumber)
	{
		my $eventIndex = $refNextEventsIndices->[$index];

		switch($refEventsVector->[$eventIndex])
		{
			case EXITING_EVENT  { $refStatusVector->[$eventIndex] = DOWN_STATUS; }
			case STARTING_EVENT { $refStatusVector->[$eventIndex] = BACKUP_STATUS; }
			case LEADER_EVENT   { $refStatusVector->[$eventIndex] = LEADER_STATUS; }
			case BACKUP_EVENT   { $refStatusVector->[$eventIndex] = BACKUP_STATUS; }
			else                { die "No such event: " . $refEventsVector->[$eventIndex]; }
		}
		&ReadLine($refEventFilesHandles, $refTimestampsVector, $refEventsVector, $eventIndex);
	}
}
##############################################################################################
## Function    : FindNextEvents                                                              #
## Arguments   : @timestampsVector - possible next events timestamps array                   #
## Return value: array of indices to the earliest timestamps in the given vector             #
## Description : finds indices to the earliest timestamps in the specified timestamps vector #
##############################################################################################
sub FindNextEvents
{
	my @timestampsVector = (@_);
	my @sortedTimestampsVector;

	foreach my $timestampIndex (0 .. $#timestampsVector)
	{
		$sortedTimestampsVector[$timestampIndex] = $timestampsVector[$timestampIndex] . ',' . $timestampIndex;
	}
	@sortedTimestampsVector = sort(@sortedTimestampsVector);

	my ($bestTimestamp, $bestIndex) = split(',', $sortedTimestampsVector[0]);
	my @bestIndices;

	foreach my $sortedTimestamp (0 .. $#sortedTimestampsVector)
	{
		my ($timestamp, $index) = split(',', $sortedTimestampsVector[$sortedTimestamp]);

		$bestIndices[$sortedTimestamp] = $index if $bestTimestamp == $timestamp;
	}
	return @bestIndices;
}
#################################################################################################
## Function    : ReportError                                                                    #
## Arguments   : $errorNumber - predefined error number (from error constants)                  #
##               $errorTimestamp - time when the error, specified by the above number, happened #
##               $errorLogPath - file system path to the error log                              #
## Description : writes a record to the specified error log according to specified error number #
##               and timestamp                                                                  #
#################################################################################################
sub ReportError
{
	my ($errorNumber, $errorTimestamp, $errorLogPath) = (@_);
	my $errorTime = &ConvertTimestampToTime($errorTimestamp); #strftime "%a %b %e %H:%M:%S %Y", localtime($errorTimestamp);
	my $error;

#	&AppendConfigurationInformationToLog($errorLogPath) if(! -f $errorLogPath);

	open(ERROR_LOG, ">> $errorLogPath") or die "open: $!";
	
	switch($errorNumber)
	{
		case NO_NEGOTIATOR            { $error = NO_NEGOTIATOR_MESSAGE . ' for more than ' . 
						STABILIZATION_TIME . ' seconds'; }
		case MORE_THAN_ONE_NEGOTIATOR { $error = MORE_THAN_ONE_NEGOTIATOR_MESSAGE . 
						' for more than ' . STABILIZATION_TIME . ' seconds'; }
		else                          { die "No such error number: $errorNumber"; }
	}
	
	print ERROR_LOG "$errorTime: $error (state: " . join(',', @potentialErrorStatus) . ")\n";
	close(ERROR_LOG) or die "close: $!";
}
###################################################################################
## Function    : IsFinished                                                       #
## Arguments   : @timestampsVector - possible next events timestamps array        #
## Return value: TRUE - if there are no more events to process, FALSE - otherwise #
## Description : determines, whether any further files processing is necessary    #
###################################################################################
sub IsFinished
{
	my @timestampsVector = (@_);

	foreach my $timestampIndex (0 .. $#timestampsVector)
	{
		return FALSE if $timestampsVector[$timestampIndex] != MAX_INT;
	}
	return TRUE;
}
############################################################################################
## Function    : ReadLine                                                                  #
## Arguments   : $refEventFilesHandles - all event files' handles array reference          #
##               $refTimestampsVector - reference to possible next events timestamps array #
##               $refEventsVector - reference to possible next events array                #
##               $eventFileIndex - necessary event file, timestamp and event index         #
## Description : reads one more line from specified event file and assigns its content to  #
##               the specified timestamp and event pointed by the specified index in       #
##               respective vectors                                                        #
############################################################################################
sub ReadLine
{
	my $refEventFilesHandles = shift;
	my $refTimestampsVector  = shift;
	my $refEventsVector      = shift;
	my $eventFileIndex       = shift;

	my $line = readline($refEventFilesHandles->[$eventFileIndex]);
         
	if($line)
	{
        	($refTimestampsVector->[$eventFileIndex],
                     $refEventsVector->[$eventFileIndex]) = split(' ', $line);
        }
        else
        {
                ($refTimestampsVector->[$eventFileIndex],
                     $refEventsVector->[$eventFileIndex]) = (MAX_INT, '');
	}
}
############################################################################################
## Function    : Initialize                                                                #
## Arguments   : $refEventFilesHandles - all event files' handles array reference          #
##               $refStatusVector - reference to status array of all the pool machines     #
##               $refTimestampsVector - reference to possible next events timestamps array #
##               $refEventsVector - reference to possible next events array                #
##               $eventFilesNumber - (overall amount of event files - 1)                   #
## Description : initializes all monitoring data structures                                #
############################################################################################
sub Initialize
{
	my $refEventFilesHandles = shift;
	my $refStatusVector      = shift;
	my $refTimestampsVector  = shift;
	my $refEventsVector      = shift;
	my $eventFilesNumber     = shift;

	mkdir $eventFilesDirectory;
	mkdir $errorLogsDirectory;
	mkdir $outputLogsDirectory;

	# Initializing the state file information
	if(! -f $stateFilePath)
	{
		open(STATE_FILE, "> $stateFilePath");
		my @initialStatus;
		
		foreach my $machineIndex (0 .. $eventFilesNumber)
		{
			$initialStatus[$machineIndex] = DOWN_STATUS;
		}

		print STATE_FILE join(',', @initialStatus) . "\n0" . "\n0";
		close(STATE_FILE);
	}
	open(STATE_FILE, "< $stateFilePath");
	my $initialStatusLine    = <STATE_FILE>;

	chomp($initialStatusLine);

	@initialStatus           = split(/,/, $initialStatusLine);
	
	$lastStatusTimestamp     = <STATE_FILE>;
	chomp($lastStatusTimestamp);
	
	$monitoringSystemCounter = <STATE_FILE>;
	chomp($monitoringSystemCounter);
	
	$monitoringSystemCounter ++;
	close(STATE_FILE);

	foreach my $eventFileIndex (0 .. $eventFilesNumber)
	{
		$refEventFilesHandles->[$eventFileIndex] = 
			IO::File->new("< $eventFileTemplatePath\." . $eventFileIndex) or 
				die "IO:File->new: $!";
		do
		{
			&ReadLine($refEventFilesHandles, $refTimestampsVector, 
				  $refEventsVector, $eventFileIndex);
		}
		while($lastStatusTimestamp >= $refTimestampsVector->[$eventFileIndex]);

		$refStatusVector->[$eventFileIndex] = $initialStatus[$eventFileIndex];
	}
	$currentTimestamp  = $lastStatusTimestamp;
	$previousTimestamp = $lastStatusTimestamp;
#	open(EPOCHE_LOG, ">> $epocheLogPath") or die "open: $!";
#	print EPOCHE_LOG &ConvertTimestampToTime($currentTimestamp) . 
#			 ": HAD_LIST - " . $hadList . ", HAD_USE_PRIMARY - " . $isPrimaryUsed . "\n";
#        print EPOCHE_LOG &ConvertTimestampToTime($currentTimestamp) . ": " . join(',', @initialStatus) . " - initial state\n";
#        close(EPOCHE_LOG) or die "close: $!";

	# Initialize the total number of errors by the number of errors that appear in the consolidated error log.
	# We count how many lines there are, containing 'for more than 78 seconds' expression, since this expression
	# appears in every error message
	if( -f $consolidatedErrorLogPath)
	{
		open(CONSOLIDATED_ERROR_LOG, "< $consolidatedErrorLogPath");
		my @consolidatedErrorLogContent = <CONSOLIDATED_ERROR_LOG>;

		# 'grep' returns number in scalar context
		$totalErrorsNumber = grep(/for more than 78 seconds/, @consolidatedErrorLogContent);
		close(CONSOLIDATED_ERROR_LOG);
	}
}
####################################################################################
## Function    : Finalize                                                          #
## Arguments   : $refEventFilesHandles - all event files' handles array reference  #
##               $refStatusVector - reference to status array of all the pool      #
##                                  machines                                       #
##               $lastTimestamp - last event timestamp of all the event files      #
## Description : closes all the opened event files and dumps the pool state to the #
##               file                                                              #
####################################################################################
sub Finalize
{
	my $refEventFilesHandles = shift;
	my $refStatusVector      = shift;
	my $lastTimestamp        = shift;
	my @eventFilesHandles    = @{$refEventFilesHandles};
	my $eventFilesNumber     = $#eventFilesHandles;
	my @statusVector         = @{$refStatusVector};

	foreach my $eventFileIndex (0 .. $eventFilesNumber)
        {
		close($refEventFilesHandles->[$eventFileIndex]) or die "close: $!";
	}

	# Saving the state for the next run of the script
	open(STATE_FILE, "> $stateFilePath");
	print STATE_FILE join(',', @statusVector) . "\n$lastTimestamp" . "\n$monitoringSystemCounter";
        close(STATE_FILE);	
}
#############################################################################
## Function    : GenerateEventFiles                                         #
## Arguments   : @logFilesPaths - file system paths to all HAD log files    #
## Description : creates all event files from the appropriate HAD log files #
#############################################################################
sub GenerateEventFiles
{
	my (@logFilesPaths) = (@_);
	my $index           = 0;

	foreach my $logFilePath (@logFilesPaths)
	{
		my $oldLogFilePath = $logFilePath . ".old";
		my $newLineChar    = "";

		&GenerateEventFile($oldLogFilePath, $index, \$newLineChar) if -f $oldLogFilePath;
		&GenerateEventFile($logFilePath, $index, \$newLineChar)    if -f $logFilePath;
		$index ++;
	}
}
################################################################################################
## Function    : GenerateEventFile                                                             #
## Arguments   : $logFilePath - file system path to HAD log file                               #
##               $index - index number of the HAD log file                                     #
## Description : passes over the HAD log file and records interesting events to the event file #
################################################################################################
sub GenerateEventFile
{
	my ($logFilePath, $index, $refNewLineChar) = (@_);
	my $eventFilePath         = "$eventFileTemplatePath\." . $index;

#	unlink($eventFilePath);

	my $eventFileHandle = IO::File->new(">> $eventFilePath") or die "IO:File->new: $!";
	my $logFileHandle   = IO::File->new("< $logFilePath") or die "IO::File->new: $!";
	
	my $previousLine;
	my $previousTimestamp;
#	my $newLineChar = "";

      	while(1)
	{
		$previousLine = readline($logFileHandle);
		last if ! defined($previousLine);

		chomp($previousLine);
		next if $previousLine eq "" || $previousLine !~ /\d+.\d+ \d+:\d+:\d+/;

		$previousTimestamp = &FindTimestamp($previousLine);
		$$refNewLineChar = &DiscoverEvents($previousLine, $previousTimestamp, $eventFileHandle, $$refNewLineChar);
		last;
	}
	my $currentLine;
	my $currentTimestamp;

	while(<$logFileHandle>)
	{
		$currentLine = $_;
		chomp($currentLine);
		next if $currentLine eq "" || $currentLine !~ /\d+.\d+ \d+:\d+:\d+/;

		$currentTimestamp = &FindTimestamp($currentLine);

		# Discovering ungraceful exiting: it is important to do it prior to
		# discovering other events in this cycle
		if(grep(!/$exitingRegEx/, $previousLine) && grep(/$startingRegEx/, $currentLine))
		{
			my $ungracefulExitingTimestamp = $previousTimestamp;# + STABILIZATION_TIME;

			print $eventFileHandle $$refNewLineChar . "$ungracefulExitingTimestamp " . EXITING_EVENT;
			$$refNewLineChar = "\n";
		}

		$$refNewLineChar       = &DiscoverEvents($currentLine, $currentTimestamp, $eventFileHandle, $$refNewLineChar);
		$previousLine      = $currentLine;
		$previousTimestamp = $currentTimestamp;
#		$$refNewLineChar       = "\n";
	}

	close($eventFileHandle);
	close($logFileHandle);
}
######################################################################################################
## Function    : DiscoverEvents                                                                      #
## Arguments   : $line - HAD log file line                                                           #
##               $timestamp - timestamp of the above line                                            #
##               $eventFileHandle - handle of the file, to which the discovered event is recorded    #
##               $newLineChar - designates, whether anything was written to the event file till      #
##                              this moment: if it is equal to "", then the file is blank,           #
##                              otherwise it is equal to "\n"                                        #
## Return value: new value of $newLineChar ("\n" if anything was written to the event file,          #
##                                          "" - otherwise)                                          #
## Description : discovers interesting event from HAD log file line and records it to the event file #
######################################################################################################
sub DiscoverEvents
{
	my ($line, $timestamp, $eventFileHandle, $newLineChar) = (@_);

	# Discovering starting
        if(grep(/$startingRegEx/, $line))
        {
        	print $eventFileHandle $newLineChar . "$timestamp " . STARTING_EVENT;
                return "\n";
        }
        # Discovering graceful exiting
        if(grep(/$exitingRegEx/, $line))
        {
       		print $eventFileHandle $newLineChar . "$timestamp " . EXITING_EVENT;
                return "\n";
        }
        # Discovering passing to leader state
        if(grep(/$leaderRegEx/, $line))
        { 
       		print $eventFileHandle $newLineChar . "$timestamp " . LEADER_EVENT;
       		return "\n";
        }
        # Discovering passing to backup state
        if(grep(/$stateRegEx/, $line)) # grep(!/$leaderRegEx/, $line) && 
        {
        	print $eventFileHandle $newLineChar . "$timestamp " . BACKUP_EVENT;
		return "\n";
        }
	return $newLineChar;
}
##########################################################################################
## Function    : FindTimestamp                                                           #
## Arguments   : $line - line of HAD log file                                            #
## Return value: timestamp - in seconds from epoche time (00:00:00 UTC, January 1, 1970) #
## Description : returns timestamp of the given HAD log file line                        #
##########################################################################################
sub FindTimestamp
{
	my ($line) = (@_);

	my @lineFields = split(' ', $line);
	my $dateField  = $lineFields[0];
	my $timeField  = $lineFields[1];

	my ($month, $date)           = split('/', $dateField);
	my ($hour, $minute, $second) = split(':', $timeField);
	my $year                     = (localtime)[5] + 1900;

	$month --;
	$date   += 0;
	$hour   += 0;
	$minute += 0;
	$second += 0;

	my $timestamp = timelocal($second, $minute, $hour, $date, $month, $year);

	# If we mistook in guessing the year number of the log, correct it by subtracting an
	# entire year
	$timestamp -= 3600 * 24 * 365 if $timestamp > time();

	return $timestamp;
}

############################ End of auxiliary functions ##################################
