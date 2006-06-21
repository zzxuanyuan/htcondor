#!/bin/env perl

################################ Main script body #####################################
use IO::File;
use File::Copy;
use Time::Local;
use Switch;
use POSIX qw(strftime);
use Common qw(TRUE FALSE DOWN_STATUS EXITING_EVENT MAX_INT FindTimestamp ConvertTimestampToTime $hadList $hadConnectionTimeout $replicationInterval $isPrimaryUsed $replicationList $collectorHost);

# Directories and files paths
my $hadMonitoringSystemDirectory = $ENV{MONITORING_HOME} || $ENV{PWD};
my $eventFilesDirectory          = $hadMonitoringSystemDirectory . "/EventFiles";
my $warningLogsDirectory         = $hadMonitoringSystemDirectory . "/WarningLogs";
my $errorLogsDirectory           = $hadMonitoringSystemDirectory . "/ErrorLogs";
my $outputLogsDirectory          = $hadMonitoringSystemDirectory . "/OutputLogs";
my $daemonLogsDirectory          = $hadMonitoringSystemDirectory . "/DaemonLogs";
my $archiveLogsDirectory         = $hadMonitoringSystemDirectory . "/ArchiveLogs";
my $configurationFilePath        = $hadMonitoringSystemDirectory . "/Configuration";
my $currentTime                  = strftime "%d-%m-%Y-%H-%M-%S", localtime;
my $activityLogPath              = $outputLogsDirectory  . "/ActivityLog";
# Generic log paths prefixes
my $stateFilePathPrefix                = $hadMonitoringSystemDirectory . "/State-";
my $warningLogPathPrefix               = $warningLogsDirectory . "/WarningLog-$currentTime-";
my $errorLogPathPrefix                 = $errorLogsDirectory   . "/ErrorLog-$currentTime-";
#my $outputLogPathPrefix                = $outputLogsDirectory  . "/OutputLog-$currentTime-";
my $epocheLogPathPrefix                = $outputLogsDirectory  . "/EpocheLog-$currentTime-";
my $eventFileTemplatePathPrefix        = $eventFilesDirectory  . "/EventFile-$currentTime-";
my $consolidatedWarningLogPathPrefix   = $warningLogsDirectory . "/ConsolidatedWarningLog-";
my $consolidatedErrorLogPathPrefix     = $errorLogsDirectory   . "/ConsolidatedErrorLog-";
my $consolidatedOutputLogPathPrefix    = $outputLogsDirectory  . "/ConsolidatedOutputLog-";
my $consolidatedEpocheLogPathPrefix    = $outputLogsDirectory  . "/ConsolidatedEpocheLog-";
my $warningHistoryLogPathPrefix        = $warningLogsDirectory . "/WarningHistoryLog-";
my $errorHistoryLogPathPrefix          = $errorLogsDirectory   . "/ErrorHistoryLog-";
#my $outputHistoryLogPathPrefix         = $outputLogsDirectory  . "/OutputHistoryLog-";
my $epocheHistoryLogPathPrefix         = $outputLogsDirectory  . "/EpocheHistoryLog-";
# Log paths, constructed from the prefixes, suffixed with the name of daemon
my $stateFilePath                = "";
my $warningLogPath               = "";
my $errorLogPath                 = "";
#my $outputLogPath                = "";
my $epocheLogPath                = "";
my $eventFileTemplatePath        = "";
my $consolidatedWarningLogPath   = "";
my $consolidatedErrorLogPath     = "";
my $consolidatedOutputLogPath    = "";
my $consolidatedEpocheLogPath    = "";
my $warningHistoryLogPath        = "";
my $errorHistoryLogPath          = "";  
#my $outputHistoryLogPath         = "";  
my $epocheHistoryLogPath         = "";

# Regular expressions, determining the type of event
my $exitingRegEx  = 'EXITING WITH STATUS';
my $startingRegEx = 'PASSWD_CACHE_REFRESH is undefined, using default value of'; 
my $leaderRegEx   = 'go to <LEADER_STATE>';
my $stateRegEx    = 'go to <PASSIVE_STATE>';
        
# Events
#use constant EXITING_EVENT  => 'Exiting';
#use constant STARTING_EVENT => 'Starting';
#use constant LEADER_EVENT   => 'Leader';
#use constant BACKUP_EVENT   => 'Backup';

# Status
#use constant DOWN_STATUS    => 'Down';
#use constant LEADER_STATUS  => 'Leader';
#use constant BACKUP_STATUS  => 'Backup';

# Various constants                
#use constant STABILIZATION_TIME     => 78;
#use constant TRUE                   => 1;
#use constant FALSE                  => 0;
#use constant MAX_INT                => 999999999999;
#use constant FICTIVE_SENDER_ADDRESS => 'had_monitoring_system@cs';
use constant LINE_SEPARATOR         => "*********************************************\n";

# Error messages
#use constant NO_NEGOTIATOR_MESSAGE            => 'no negotiator found in pool';
#use constant MORE_THAN_ONE_NEGOTIATOR_MESSAGE => 'more than one negotiator found in pool';

# Monitoring data structures
my @eventFilesHandles               = ();
my @statusVector                    = ();
my @timestampsVector                = ();
my @eventsVector                    = ();
my @initialStatus                   = ();
my @sinfulStrings                   = ();
#my $totalErrorsNumber               = 0;
#my $totalWarningsNumber             = 0;
my $currentTimestamp                = 0;
my @offsets                         = ();
my @additionalParameters            = ();

# State file variables
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
my @monitoredDaemons                = ();
my $isOffsetCalculationNeeded       = TRUE;
my $fictiveSenderAddress            = "";

# Loading Condor pool parameters
my @condorParameters = `condor_config_val HAD_LIST HAD_USE_PRIMARY REPLICATION_LIST COLLECTOR_HOST HAD_CONNECTION_TIMEOUT REPLICATION_INTERVAL`;
chomp(@condorParameters);
$hadList                     = $condorParameters[0];
$isPrimaryUsed               = $condorParameters[1];
$replicationList             = $condorParameters[2];
$collectorHost               = $condorParameters[3];
$hadConnectionTimeout        = $condorParameters[4];
$replicationInterval         = $condorParameters[5];
my @hadSinfulStrings         = split(',', $hadList);
my @replicationSinfulStrings = split(',', $replicationList);
my @collectorSinfulStrings   = split(',', $collectorHost);
#my $hadInterval              = (2 * $hadConnectionTimeout * split(',', $hadList) + 1) * 2;

&LoadConfiguration();
@offsets = &CalculateOffsets(@hadSinfulStrings);

foreach my $monitoredDaemon (@monitoredDaemons) 
{
	my $checkerName               = &CapitalizeFirst($monitoredDaemon);
	$stateFilePath                = $stateFilePathPrefix              . $checkerName;
	$warningLogPath               = $warningLogPathPrefix             . $checkerName;
	$errorLogPath                 = $errorLogPathPrefix               . $checkerName;
#	$outputLogPath                = $outputLogPathPrefix              . $checkerName;
	$epocheLogPath                = $epocheLogPathPrefix              . $checkerName;
	$eventFileTemplatePath        = $eventFileTemplatePathPrefix      . $checkerName;
	$consolidatedWarningLogPath   = $consolidatedWarningLogPathPrefix . $checkerName;
	$consolidatedErrorLogPath     = $consolidatedErrorLogPathPrefix   . $checkerName;
	$consolidatedOutputLogPath    = $consolidatedOutputLogPathPrefix  . $checkerName;
	$consolidatedEpocheLogPath    = $consolidatedEpocheLogPathPrefix  . $checkerName;
	$warningHistoryLogPath        = $warningHistoryLogPathPrefix      . $checkerName;
	$errorHistoryLogPath          = $errorHistoryLogPathPrefix        . $checkerName;
#	$outputHistoryLogPath         = $outputHistoryLogPathPrefix       . $checkerName; 
	$epocheHistoryLogPath         = $epocheHistoryLogPathPrefix       . $checkerName;

	# Monitoring data structures
	@eventFilesHandles               = ();
	@statusVector                    = ();
	@timestampsVector                = ();
	@eventsVector                    = ();
	@initialStatus                   = ();
	@sinfulStrings                   = eval('@' . lc($monitoredDaemon) . 'SinfulStrings');

	# Initializing additional parameters structure with nulls
	foreach my $machineIndex (0 .. $#hadSinfulStrings)
	{
		$additionalParameters[$machineIndex] = undef;
	}
	$currentTimestamp                = 0;
	# We need to load the state file parameters in order to know to sift the already seen dates
	&LoadStateFile($#hadSinfulStrings, $monitoredDaemon);

	my @logFilePaths = &FetchLogs($monitoredDaemon); 
	# For debugging 
#	my @logFilePaths = 
#	 ('/home/sharov/HA6/src/condor_had/HadMonitoringSystem/DaemonLogs.bak/HADLog.<132.68.37.112:60104>', 
#	 '/home/sharov/HA6/src/condor_had/HadMonitoringSystem/DaemonLogs.bak/HADLog.<132.68.37.124:60105>');#, 
#	 '/home/sharov/HA6/src/condor_had/HadMonitoringSystem/DaemonLogs.bak/HADLog.<132.68.37.126:60106>');
#	('/home/sharov/HA6/src/condor_had/HadMonitoringSystem/DaemonLogs.bak/DaemonLog-<132.68.37.112:60104>-16-06-2006-12-28-10-Replication',
#	 '/home/sharov/HA6/src/condor_had/HadMonitoringSystem/DaemonLogs.bak/DaemonLog-<132.68.37.113:60105>-16-06-2006-12-28-10-Replication');

	&GenerateEventFiles($monitoredDaemon, @logFilePaths);
	&Initialize(\@eventFilesHandles, \@statusVector, 
		    \@timestampsVector , \@eventsVector, $#logFilePaths);

	&GenerateEpocheFile(\@eventFilesHandles, \@statusVector, \@timestampsVector,
			    \@eventsVector, $previousTimestamp, $currentTimestamp, $monitoredDaemon);

	&Finalize(\@eventFilesHandles, \@statusVector, $currentTimestamp);

	&CheckValidity($monitoredDaemon);
}

&SendReport("Errors/warnings were discovered while running the HAD Monitoring System.\n" . 
	    "Find the error/warning logs attached.")
#	if (-f $errorLogPath || -f $warningLogPath) && 
	if (glob($warningLogPathPrefix . "*") || glob($errorLogPathPrefix . "*")) &&
	      $isReportSent eq TRUE &&
	      $monitoringSystemCounter % $consolidatedReportFrequency != 0;

# Collect unused logs according to the garbage collector policy 
# and archive them before anything has been deleted
&RemoveLogs();

if($isReportSent eq TRUE && $monitoringSystemCounter % $consolidatedReportFrequency == 0)
{
	&SendConsolidatedReport("Find the relevant messages attached.");
	# Start collecting the logs for the next consolidated report from the scratch
	foreach my $monitoredDaemon (@monitoredDaemons)
	{
		my $checkerName               = &CapitalizeFirst($monitoredDaemon);
		$consolidatedWarningLogPath   = $consolidatedWarningLogPathPrefix . $checkerName;
		$consolidatedErrorLogPath     = $consolidatedErrorLogPathPrefix   . $checkerName;
		$consolidatedOutputLogPath    = $consolidatedOutputLogPathPrefix  . $checkerName;
		$consolidatedEpocheLogPath    = $consolidatedEpocheLogPathPrefix  . $checkerName;

		unlink($consolidatedWarningLogPath);
		unlink($consolidatedErrorLogPath);
		unlink($consolidatedOutputLogPath);
		unlink($consolidatedEpocheLogPath);
	}
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
       	my $configurationInformation = "Configuration information:\n";
#               "HAD_LIST (in this order HAD states will appear in the report) - " . $hadList . "\n" .
#               "HAD_USE_PRIMARY - " . $isPrimaryUsed . "\n";
#
#        if(uc($isPrimaryUsed) eq 'TRUE')
#        {
#                my @hadAddresses = split(',', $hadList);
#
#                $configurationInformation .= "Primary HAD - " . $hadAddresses[0]  . "\n";
#        }
#        $configurationInformation .= "\n";
	foreach my $monitoredDaemon (@monitoredDaemons)
	{
		my $checkerName                          = &CapitalizeFirst($monitoredDaemon);
                my $configurationInformationFunctionName = $checkerName . "ConfigurationInformation";
         
                require "$hadMonitoringSystemDirectory/Checkers/$checkerName.pl";

		$configurationInformation .= "\n" . &{$configurationInformationFunctionName}();
	}

	$configurationInformation .= "\n\n";

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
#sub ConvertTimestampToTime
#{
#	my $timestamp = shift;
#
#	return strftime "%a %b %e %H:%M:%S %Y", localtime($timestamp);
#}

############################################################################################
## Function    : RemoveLogs                                                                #
## Description : removes unnecessary logs according to the garbage collector policy        #
############################################################################################
sub RemoveLogs
{
	&AppendTextToActivityLog("Started removing redundant logs to the archives directory\n");
	# Compressing all the logs always at the end of the monitoring session
	mkdir $archiveLogsDirectory;

	use Archive::Zip qw( :ERROR_CODES :CONSTANTS );

	my $zip = Archive::Zip->new();

	$zip->addTree("$eventFilesDirectory"  , 'EventFiles');
	$zip->addTree("$warningLogsDirectory" , 'WarningLogs');
	$zip->addTree("$errorLogsDirectory"   , 'ErrorLogs');
     	$zip->addTree("$outputLogsDirectory"  , 'OutputLogs');
	$zip->addTree("$daemonLogsDirectory"  , 'DaemonLogs');

	# Archiving all the state files of all the monitored daemons
	foreach my $monitoredDaemon (@monitoredDaemons)
	{
		my $checkerName = &CapitalizeFirst($monitoredDaemon);
		$zip->addFile("$stateFilePath", "State-$checkerName");
	}
	$zip->addFile("$configurationFilePath", 'Configuration');

	$zip->writeToFileNamed("$archiveLogsDirectory/Archive-$currentTime.zip") == AZ_OK or die "zip: $!";
	&AppendTextToActivityLog("Archived the logs of this run to Archive-$currentTime.zip\n");

	my @filePaths = glob("$eventFilesDirectory/* " . 
			     "$errorLogsDirectory/ErrorLog-* " .
			     "$warningLogsDirectory/WarningLog-* " .
			     "$outputLogsDirectory/OutputLog-* " .
			     "$outputLogsDirectory/EpocheLog-* " .
			     "$daemonLogsDirectory/DaemonLog* ");

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
	&AppendTextToActivityLog("Removed archives older than $storeOldArchivesDays days\n");
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

#	`condor_fetchlog \'$remoteDaemonSinfulString\' $remoteLogName > \'$localLogName\' 2>>$outputLogPath`;
	`condor_fetchlog \'$remoteDaemonSinfulString\' $remoteLogName > \'$localLogName\' 2>>/dev/null`;

	if($? != 0)
	{
		&AppendTextToActivityLog("Log of $remoteLogName from $remoteDaemonSinfulString " . 
					 "could not be fetched\n");
		return ;
	}
	&AppendTextToActivityLog("Fetched log of $remoteLogName from $remoteDaemonSinfulString\n");
}
############################################################################################
## Function    : FetchLogs                                                                 #
## Return value: @logFilePaths - list of file paths, where the logs were fetched to        #
## Description : fetches all the necessary daemons' logs and returns their paths           #
############################################################################################
sub FetchLogs
{
	&AppendTextToActivityLog("Started fetching daemons logs\n");
	my $monitoredDaemon = shift;
	my $checkerName     = &CapitalizeFirst($monitoredDaemon);

	mkdir $daemonLogsDirectory;

#	my @hadSinfulStrings = split(',', $hadList);
	my @logFilePaths;
	my $machineIndex = 0;

	foreach my $hadSinfulString (@hadSinfulStrings)
	{
		my $logFilePath = "$daemonLogsDirectory/DaemonLog-$hadSinfulString-$currentTime-$checkerName";
		
		push(@logFilePaths, $logFilePath);
#		FetchLog($hadSinfulString, "$monitoredDaemon.old", "$logFilePath.old");
#		FetchLog($hadSinfulString,  $monitoredDaemon     ,  $logFilePath);

		# It is important to first fetch the older log in order not to get confused with dates
		# in case, when the logs are huge and they are just about being rotated
                FetchLog($collectorSinfulStrings[$machineIndex], "$monitoredDaemon.old", "$logFilePath.old");
		FetchLog($collectorSinfulStrings[$machineIndex],  $monitoredDaemon     ,  $logFilePath);

		$machineIndex ++;
	}
	return @logFilePaths;
}
############################################################################################
## Function    : AppendText                                                                #
## Arguments   : $toPath - a path to a file, to which the information is going to be       #
##                         appended                                                        #
##               $message - the information, that is to be appended to the specified file  #
## Description : appends the specified message to the give file                            #
############################################################################################
sub AppendText
{
	my ($toPath, $message) = (@_);

	open(TO_LOG,   ">> $toPath");
	print TO_LOG $message;
	close(TO_LOG);
}
############################################################################################
## Function    : AppendTextToActivityLog                                                   #
## Arguments   : $message - the information, that is to be appended to the activity log    #
## Description : appends the specified message to the activity log                         #
############################################################################################
sub AppendTextToActivityLog
{
        my $message = shift;

        &AppendText($activityLogPath, &ConvertTimestampToTime(time()) . ': ' . $message);
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
	&AppendTextToActivityLog("Started loading configuration file entries\n");
	my @configurationFileContents = &ReturnFileContent($configurationFilePath);

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
			case 'MONITORED_DAEMONS'
						 { my $monitoredDaemonsAux = uc($value); 
						
						   @monitoredDaemons = split(',', $monitoredDaemonsAux); 
						   # Checking that there is a validity checker for each daemon
						   foreach my $monitoredDaemon (@monitoredDaemons)
						   {
							my $checkerName = &CapitalizeFirst($monitoredDaemon);

						   	require "$hadMonitoringSystemDirectory/Checkers/$checkerName.pl";
						   } }
			case 'IS_OFFSET_CALCULATION_NEEDED'
						 { $isOffsetCalculationNeeded = FALSE
						   if grep(/$value/i, 'no'); }
			case 'FICTIVE_SENDER_ADDRESS'
						 { $fictiveSenderAddress = $value; }
                	else                     { die "No such configuration entry: $key"; }
        	}
		&AppendTextToActivityLog("Loaded configuration parameter $key = $value\n");
	}
}
############################################################################################
## Function    : SendConsolidatedReport                                                    #
## Arguments   : $messageBody - the text that will become the body of the                  #
##                              future SMTP message                                        #
## Description : sends bundle of reports about the logged activities to all whom it may    #
##               concern; this report contain the log of all the errors, warnings and the  #
##               log of the significant pool events                                        #
############################################################################################
sub SendConsolidatedReport
{
	my $totalErrorsNumber   = 0; #&ReturnFileLinesNumber($consolidatedErrorLogPath);
	my $totalWarningsNumber = 0; #&ReturnFileLinesNumber($consolidatedWarningLogPath);

	foreach my $monitoredDaemon (@monitoredDaemons)
        {
                my $checkerName               = &CapitalizeFirst($monitoredDaemon);
                $consolidatedWarningLogPath   = $consolidatedWarningLogPathPrefix . $checkerName;
                $consolidatedErrorLogPath     = $consolidatedErrorLogPathPrefix   . $checkerName;

		$totalErrorsNumber   += &ReturnFileLinesNumber($consolidatedErrorLogPath);
		$totalWarningsNumber += &ReturnFileLinesNumber($consolidatedWarningLogPath);
	}

	return if $isNoErrorReportSent eq FALSE && $totalErrorsNumber == 0 && $totalWarningsNumber == 0;

	&AppendTextToActivityLog("Started sending consolidated report to $consolidatedReportRecipients\n");

        my ($messageBody)               = (@_);
	my $totalErrorsNumberAsString   = ($totalErrorsNumber   == 0) ? 'no errors'   : "$totalErrorsNumber errors";
	my $totalWarningsNumberAsString = ($totalWarningsNumber == 0) ? 'no warnings' : "$totalWarningsNumber warnings";
        
        use MIME::Lite;
        use Net::SMTP;

        my $message = MIME::Lite->new(From    => $fictiveSenderAddress, #FICTIVE_SENDER_ADDRESS,
                                      #To      => $reportRecipients,
				      To      => $consolidatedReportRecipients,
                                      Subject => "HAD Monitoring System Consolidated Report - " . 
						 "$totalErrorsNumberAsString/$totalWarningsNumberAsString",
                                      Type    => 'multipart/mixed');
        $message->attach(Type => 'TEXT',
                         Data => $messageBody);
	$message->attach(Type => 'TEXT',
			 Data => &ReturnConfigurationInformation());

	foreach my $monitoredDaemon (@monitoredDaemons)
        {       
                my $checkerName               = &CapitalizeFirst($monitoredDaemon);
                $consolidatedWarningLogPath   = $consolidatedWarningLogPathPrefix . $checkerName;
                $consolidatedErrorLogPath     = $consolidatedErrorLogPathPrefix   . $checkerName;
		$consolidatedEpocheLogPath    = $consolidatedEpocheLogPathPrefix  . $checkerName;

		&AttachFileAsText($message, $consolidatedEpocheLogPath, "Consolidated epoche log - $checkerName")
			if(-f $consolidatedEpocheLogPath);
		&AttachFileAsText($message, $consolidatedErrorLogPath, "Consolidated error log - $checkerName")
			if(-f $consolidatedErrorLogPath);
		&AttachFileAsText($message, $consolidatedWarningLogPath, "Consolidated warning log - $checkerName")
                        if(-f $consolidatedWarningLogPath);
	}
        MIME::Lite->send('smtp', $smtpServer, Timeout => 60);

        $message->send();
}

sub AttachFileAsText
{
	my ($message, $filePath, $header) = (@_);

	my @text = &ReturnFileContent($filePath);
	$message->attach(Type => 'TEXT',
			 Data => "$header:\n\n@text");
}
############################################################################################
## Function    : SendReport                                                                #
## Arguments   : $messageBody - the text that will become the body of the                  #
##                              future SMTP message                                        #
## Description : sends report about inappropriate pool state to all whom it may concern    #
############################################################################################
sub SendReport
{
	&AppendTextToActivityLog("Started sending error/warning report to $errorReportRecipients\n");
	my ($messageBody) = (@_);

	use MIME::Lite;
	use Net::SMTP;

#        my @errorLogMessage = &ReturnFileContent($errorLogPath);
	my $errorsWarningsSubject = '';
	my $comma                 = '';

	foreach my $monitoredDaemon (@monitoredDaemons)
	{
		my $checkerName     = &CapitalizeFirst($monitoredDaemon);
	        $warningLogPath     = $warningLogPathPrefix . $checkerName;
	        $errorLogPath       = $errorLogPathPrefix   . $checkerName;

		my $errorsNumber    = &ReturnFileLinesNumber($errorLogPath);
		my $warningsNumber  = &ReturnFileLinesNumber($warningLogPath);
		my $errorsNumberAsString   = ($errorsNumber   == 0) ? 'no errors'   : "$errorsNumber errors";
	        my $warningsNumberAsString = ($warningsNumber == 0) ? 'no warnings' : "$warningsNumber warnings";

		$errorsWarningsSubject .= $comma . " $checkerName($errorsNumberAsString/$warningsNumberAsString)";
		$comma = ',';
	}
	my $message = MIME::Lite->new(From    => $fictiveSenderAddress, #FICTIVE_SENDER_ADDRESS, 
				      #To      => $reportRecipients, 
				      To      => $errorReportRecipients,
				      Subject => "HAD Monitoring System Report -$errorsWarningsSubject",
				      Type    => 'multipart/mixed');
	$message->attach(Type => 'TEXT',
 			 Data => $messageBody);
        $message->attach(Type => 'TEXT',
                         Data => &ReturnConfigurationInformation());
	#	$message->attach(Type => 'TEXT',
	#			 Data => "Error log:\n\n@errorLogMessage");
	
	foreach my $monitoredDaemon (@monitoredDaemons)
	{
		my $checkerName     = &CapitalizeFirst($monitoredDaemon);
                $warningLogPath     = $warningLogPathPrefix . $checkerName;
                $errorLogPath       = $errorLogPathPrefix   . $checkerName;

		&AttachFileAsText($message, $errorLogPath  , "Error log - $checkerName")   if -f $errorLogPath;
		&AttachFileAsText($message, $warningLogPath, "Warning log - $checkerName") if -f $warningLogPath;
	}
	MIME::Lite->send('smtp', $smtpServer, Timeout => 60);

	$message->send();
}
############################################################################################
## Function    : CheckValidity                                                             #
## Description : checks, whether the given machines status at epoche is valid; reports     #
##               about all the errors/warnings to the appropriate logs                     #
############################################################################################
sub CheckValidity
{
	my $monitoredDaemon  = shift;
	
	&AppendTextToActivityLog("Started checking validity of the epoche file of $monitoredDaemon\n");

	my @epocheLogContent = &ReturnFileContent($epocheLogPath);

	chomp(@epocheLogContent);

	my @previousStatusVector = @initialStatus;
#	my @hadSinfulStrings = split(',', $hadList);

	foreach my $epocheLogLine (@epocheLogContent)
	{
	        my ($epocheStartTime, $epocheEndTime, $statusVectorAsString) = split(' ', $epocheLogLine);
		my @statusVector = split(',', $statusVectorAsString);

	        my $machinesNumber        = $#statusVector;
	        my $leadersNumber         = 0;
        	my $downNumber            = 0;
		my $message               = '';
		my $comma                 = &ConvertTimestampToTime($epocheStartTime) . ' - ' .
                           		    &ConvertTimestampToTime($epocheEndTime) . ': ';
		# Checking if any of the running machines failed
		foreach my $machineIndex (0 .. $machinesNumber)
		{
			if($statusVector[$machineIndex] ne $previousStatusVector[$machineIndex] &&
			   $statusVector[$machineIndex] eq DOWN_STATUS)
			{
				$message .= $comma . $sinfulStrings[$machineIndex] . ' went down';
				$comma    = ',';
			}
		}
		# If so, warning message is issued
		if($message ne '')
		{
			$message .= "\n";
			&AppendText($warningLogPath            , $message);
                	&AppendText($consolidatedWarningLogPath, $message);
                	&AppendText($warningHistoryLogPath     , $message);
		}

		my $checkerName            = &CapitalizeFirst($monitoredDaemon);
		my $validationFunctionName = $checkerName . "Validate";

		require "$hadMonitoringSystemDirectory/Checkers/$checkerName.pl";

		$message = &{$validationFunctionName}($epocheStartTime, $epocheEndTime, @statusVector);
		next if($message eq "");
		# Otherwise any error must have happened
		$message = &ConvertTimestampToTime($epocheStartTime) . ' - ' . 
	 	      	   &ConvertTimestampToTime($epocheEndTime) . ': ' . $message . "\n";

		&AppendText($errorLogPath            , $message);
		&AppendText($consolidatedErrorLogPath, $message);
		&AppendText($errorHistoryLogPath     , $message);

		@previousStatusVector = @statusVector;
	}
}
############################################################################################
## Function    : ApplyStatus                                                               #
## Arguments   : $refNextEventsIndices - relevant arrays indices                           #
##               $refEventFilesHandles - all event files' handles array reference          #
##               $refStatusVector - reference to status array of all the pool machines     #
##               $refTimestampsVector - reference to possible next events timestamps array #
##               $refEventsVector - reference to possible next events array                #
##               $monitoredDaemon - daemon, the logs of which are being processed          #
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
	my $monitoredDaemon      = shift;
	my @nextEventsIndices    = @{$refNextEventsIndices};
	my $indicesNumber        = $#nextEventsIndices;
	
	foreach my $index (0 .. $indicesNumber)
	{
		my $eventIndex = $refNextEventsIndices->[$index];

#		switch($refEventsVector->[$eventIndex])
#		{
#			case EXITING_EVENT  { $refStatusVector->[$eventIndex] = DOWN_STATUS; }
#			case STARTING_EVENT { $refStatusVector->[$eventIndex] = BACKUP_STATUS; }
#			case LEADER_EVENT   { $refStatusVector->[$eventIndex] = LEADER_STATUS; }
#			case BACKUP_EVENT   { $refStatusVector->[$eventIndex] = BACKUP_STATUS; }
#			else                { die "No such event: " . $refEventsVector->[$eventIndex]; }
#		}
		my $checkerName             = &CapitalizeFirst($monitoredDaemon);
		my $applyStatusFunctionName = $checkerName . "ApplyStatus";

		require "$hadMonitoringSystemDirectory/Checkers/$checkerName.pl";

		($refStatusVector->[$eventIndex], my $message) = 
						  &{$applyStatusFunctionName}($refEventsVector->[$eventIndex], 
									      $refStatusVector->[$eventIndex],
									      $refTimestampsVector->[$eventIndex],
									      \@additionalParameters,
									      $eventIndex);
		# Applying status resulted in warning messages
                if($message ne '')
                {
                        $message .= "\n";
                        &AppendText($warningLogPath            , $message);
                        &AppendText($consolidatedWarningLogPath, $message);   
                        &AppendText($warningHistoryLogPath     , $message);
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
## Function    : LoadStateFile                                                             #
## Arguments   : $daemonsNumber - number of daemons, the state of which appear in the state#
##                                file, minus 1                                            #
## Description : loads last run data from the state file to the inner structures           #
############################################################################################
sub LoadStateFile
{
	my $daemonsNumber   = shift;
	my $monitoredDaemon = shift;

	# Initializing the state file information
	if(! -f $stateFilePath)
	{
		&AppendTextToActivityLog("Initializing the state file information for $monitoredDaemon\n");
		open(STATE_FILE, "> $stateFilePath");
		my @initialStatus;
 
		foreach my $machineIndex (0 .. $daemonsNumber)
		{
			$initialStatus[$machineIndex] = DOWN_STATUS;
 		}
        
		print STATE_FILE join(',', @initialStatus) . "\n0" . "\n0" . "\n0";
		close(STATE_FILE);  
	}
	open(STATE_FILE, "< $stateFilePath");
	my $initialStatusLine    = <STATE_FILE>;
                                 
	chomp($initialStatusLine);
        
	@initialStatus           = split(/,/, $initialStatusLine);
   
	$lastStatusTimestamp     = <STATE_FILE>;
	chomp($lastStatusTimestamp);

	$previousTimestamp       = <STATE_FILE>;
	chomp($previousTimestamp);

	$monitoringSystemCounter = <STATE_FILE>;
	chomp($monitoringSystemCounter);

	$monitoringSystemCounter ++;
	close(STATE_FILE);
	&AppendTextToActivityLog("State file loaded with daemons last known state = $initialStatusLine, last seen time = " .
				 &ConvertTimestampToTime($lastStatusTimestamp) . ", last status time = " .
				 &ConvertTimestampToTime($previousTimestamp)   . ", monitoring counter = $monitoringSystemCounter\n");
	&AppendTextToActivityLog("Passing by outdated entries in the fetched logs\n");
}
############################################################################################
## Function    : Initialize                                                                #
## Arguments   : $refEventFilesHandles - all event files' handles array reference          #
##               $refStatusVector - reference to status array of all the pool machines     #
##               $refTimestampsVector - reference to possible next events timestamps array #
##               $refEventsVector - reference to possible next events array                #
##               $eventFilesNumber - (overall amount of event files minus 1)               #
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
	mkdir $warningLogsDirectory;
	mkdir $errorLogsDirectory;
	mkdir $outputLogsDirectory;

	# Initializing the state file information
#	if(! -f $stateFilePath)
#	{
#		&AppendTextToActivityLog("Initializing the state file information\n");
#		open(STATE_FILE, "> $stateFilePath");
#		my @initialStatus;
#		
#		foreach my $machineIndex (0 .. $eventFilesNumber)
#		{
#			$initialStatus[$machineIndex] = DOWN_STATUS;
#		}
#
#		print STATE_FILE join(',', @initialStatus) . "\n0" . "\n0" . "\n0";
#		close(STATE_FILE);
#	}
#	open(STATE_FILE, "< $stateFilePath");
#	my $initialStatusLine    = <STATE_FILE>;
#
#	chomp($initialStatusLine);
#
#	@initialStatus           = split(/,/, $initialStatusLine);
#	
#	$lastStatusTimestamp     = <STATE_FILE>;
#	chomp($lastStatusTimestamp);
#
#	$previousTimestamp       = <STATE_FILE>;
#	chomp($previousTimestamp);
#	
#	$monitoringSystemCounter = <STATE_FILE>;
#	chomp($monitoringSystemCounter);
#	
#	$monitoringSystemCounter ++;
#	close(STATE_FILE);
#	&AppendTextToActivityLog("State file loaded with daemons last known state = $initialStatusLine, last seen time = " . 
#				 &ConvertTimestampToTime($lastStatusTimestamp) . ", last status time = " .
#				 &ConvertTimestampToTime($previousTimestamp)   . ", monitoring counter = $monitoringSystemCounter\n");
#	&AppendTextToActivityLog("Passing by outdated entries in the fetched logs\n");
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
#	$previousTimestamp = $lastStatusTimestamp;

	# Initialize the total number of errors by the number of errors that appear in the consolidated error log.
	# We count how many lines there are, containing 'for more than 78 seconds' expression, since this expression
	# appears in every error message
#	if(-f $consolidatedErrorLogPath)
#	{
#		my @consolidatedErrorLogContent = &ReturnFileContent($consolidatedErrorLogPath);
#
#		# 'grep' returns number in scalar context
#		$totalErrorsNumber = grep(/for more than 78 seconds/, @consolidatedErrorLogContent);
#		&AppendTextToActivityLog("Found total of $totalErrorsNumber errors in the current consolidated report\n");
#	}
#	# Initialize the total number of warnings for this consolidated report
#	if(-f $consolidatedWarningLogPath)
#	{
#		$totalWarningsNumber = &ReturnFileLinesNumber($consolidatedWarningLogPath);
#		&AppendTextToActivityLog("Found total of $totalWarningsNumber errors in the current consolidated report\n");
#	}
}
####################################################################################
## Function    : ReturnFileLinesNumber                                             #
## Arguments   : $filePath - file, the length of which is returned                 # 
## Return value: fileLength - the number of lines in the given file                # 
## Description : returns the number of lines in the specified file                 #
####################################################################################
sub ReturnFileLinesNumber
{
	my $filePath    = shift;
	my @fileContent = &ReturnFileContent($filePath);

	# 'grep' returns number in scalar context  
        # when // specified as a pattern, any line is matched
	my $fileLength = grep(//, @fileContent);

	return $fileLength;
}
####################################################################################
## Function    : Finalize                                                          #
## Arguments   : $refEventFilesHandles - all event files' handles array reference  #
## Description : finalizes all the monitoring system structures                    #
####################################################################################
sub Finalize
{
	&AppendTextToActivityLog("Started finalizing all the system structures\n");
	my $refEventFilesHandles = shift;
	my @eventFilesHandles    = @{$refEventFilesHandles};
	my $eventFilesNumber     = $#eventFilesHandles;

	foreach my $eventFileIndex (0 .. $eventFilesNumber)
        {
		close($refEventFilesHandles->[$eventFileIndex]) or die "close: $!";
	}
}
############################################################################################
## Function    : GenerateEpocheFile                                                        #
## Arguments   : $refEventFilesHandles - all event files' handles array reference          #
##               $refStatusVector - reference to status array of all the pool machines     #
##               $refTimestampsVector - reference to possible next events timestamps array #
##               $refEventsVector - reference to possible next events array                #
##               $previousTimestamp - 
##               $currentTimestamp -
##               $monitoredDaemon - 
## Description : generates epoche file out of generated daemon event files                 #
############################################################################################
sub GenerateEpocheFile
{
	&AppendTextToActivityLog("Started generating epoche file\n");
	my $refEventFilesHandles = shift;
	my $refStatusVector      = shift;
	my $refTimestampsVector  = shift;
	my $refEventsVector      = shift;
	my $previousTimestamp    = shift;
	my $currentTimestamp     = shift;
	my $monitoredDaemon      = shift;
	my @eventFilesHandles    = @{$refEventFilesHandles};
	my @statusVector         = @{$refStatusVector};
	my @timestampsVector     = @{$refTimestampsVector};
	my @eventsVector         = @{$refEventsVector};

	while(!&IsFinished(@timestampsVector))
	{
 	        # Finds the earliest events of 'timestampsVector'
        	my @nextEventsIndices = &FindNextEvents(@timestampsVector);

        	$currentTimestamp = $timestampsVector[$nextEventsIndices[0]];

	        last if $currentTimestamp == MAX_INT;
	        # Applies status for machines, specified by 'nextEventsIndices' and
        	# reads the next events for these files
	        my @oldStatusVector = @statusVector;

        	&ApplyStatus(\@nextEventsIndices, $refEventFilesHandles,
                     	     \@statusVector     , \@timestampsVector ,   
                     	     \@eventsVector     , $monitoredDaemon);
       		if("@oldStatusVector" ne "@statusVector")
	        {
			my $message = &ConvertTimestampToTime($previousTimestamp) . " - " .
				      &ConvertTimestampToTime($currentTimestamp) . ": " .
				      join(',', @oldStatusVector) . "\n";
			&AppendText($epocheLogPath, $previousTimestamp . ' ' . $currentTimestamp . ' ' .
						    join(',', @oldStatusVector) . "\n");
			&AppendText($consolidatedEpocheLogPath, $message);
			&AppendText($epocheHistoryLogPath, $message);
                	$previousTimestamp = $currentTimestamp;
        	}
	}
	# Registering the last epoche of this run
#	if($previousTimestamp != $currentTimestamp && $currentTimestamp > $lastStatusTimestamp)
#	{
#		&AppendText($epocheLogPath, $previousTimestamp . ' ' . $currentTimestamp . ' ' .
#                                            join(',', @statusVector) . "\n");
		&AppendText($epocheLogPath, $previousTimestamp . ' ' . time() . ' ' .
                                            join(',', @statusVector) . "\n");
#	}
	# Registering the last epoche for the consolidated log
	if(
#	   $previousTimestamp != $currentTimestamp  && 
#	   $currentTimestamp > $lastStatusTimestamp &&
	   $monitoringSystemCounter % $consolidatedReportFrequency == 0)
	{
		&AppendText($consolidatedEpocheLogPath, &ConvertTimestampToTime($previousTimestamp) . " - " .
#                                     			&ConvertTimestampToTime($currentTimestamp) . ": " .
							&ConvertTimestampToTime(time()) . ": " .
                                             		join(',', @statusVector) . "\n");
	}
	&AppendTextToActivityLog("Saving the state for the next run with daemons last known state = " .
				 join(',', @statusVector) . ", last seen time = " . 
				 &ConvertTimestampToTime($currentTimestamp)  . ", last status time = " .
                                 &ConvertTimestampToTime($previousTimestamp) . ", monitoring counter = $monitoringSystemCounter\n");
	# Saving the state for the next run of the script
        open(STATE_FILE, "> $stateFilePath");
        print STATE_FILE join(',', @statusVector) . "\n$currentTimestamp" . 
			 "\n$previousTimestamp"   . "\n$monitoringSystemCounter";
	close(STATE_FILE);
}

##############################################################################
## Function    : GenerateEventFiles                                          #
## Arguments   : $monitoredDaemon - daemon, logs of which are being processed#
##		 @logFilesPaths - file system paths to all HAD log files     #
## Description : creates all event files from the appropriate HAD log files  #
##############################################################################
sub GenerateEventFiles
{
	my ($monitoredDaemon, @logFilesPaths) = (@_);
	my $index           = 0;

	foreach my $logFilePath (@logFilesPaths)
	{
		my $oldLogFilePath = $logFilePath . ".old";
		my $newLineChar    = "";

		&GenerateEventFile($oldLogFilePath, $index, \$newLineChar, $monitoredDaemon) if -f $oldLogFilePath;
		&GenerateEventFile($logFilePath, $index, \$newLineChar, $monitoredDaemon)    if -f $logFilePath;
		$index ++;
	}
}
################################################################################################
## Function    : GenerateEventFile                                                             #
## Arguments   : $logFilePath - file system path to HAD log file                               #
##               $index - index number of the HAD log file                                     #
##               $monitoredDaemon - daemon, logs of which are being processed                  #
## Description : passes over the HAD log file and records interesting events to the event file #
################################################################################################
sub GenerateEventFile
{
	my ($logFilePath, $index, $refNewLineChar, $monitoredDaemon) = (@_);
	my $eventFilePath         = "$eventFileTemplatePath\." . $index;

#	unlink($eventFilePath);

	my $eventFileHandle = IO::File->new(">> $eventFilePath") or die "IO:File->new: $!";
	my $logFileHandle   = IO::File->new("< $logFilePath") or die "IO::File->new: $!";
	
	my $previousLine;
	my $previousTimestamp;
#	my $newLineChar = "";

	# Discovering first event in the log
      	while(1)
	{
		$previousLine = readline($logFileHandle);
		last if ! defined($previousLine);

		chomp($previousLine);
		next if $previousLine eq "" || $previousLine !~ /\d+.\d+ \d+:\d+:\d+/;

		$previousTimestamp = &FindTimestamp($previousLine, $offsets[$index]);
		$$refNewLineChar = &DiscoverEvents($previousLine   , $previousTimestamp, $eventFileHandle, 
						   $$refNewLineChar, $monitoredDaemon);
		last;
	}
	my $currentLine;
	my $currentTimestamp;
	
	# Discovering what is considered the gap for the monitored daemon
	my $checkerName = &CapitalizeFirst($monitoredDaemon);
	my $gapFunctionName = $checkerName . "Gap";
                        
        require "$hadMonitoringSystemDirectory/Checkers/$checkerName.pl";
                                      
        my $gap = &{$gapFunctionName}();

	# Discovering the rest of the events
	while(<$logFileHandle>)
	{
		$currentLine = $_;
		chomp($currentLine);
		next if $currentLine eq "" || $currentLine !~ /\d+.\d+ \d+:\d+:\d+/;

		$currentTimestamp = &FindTimestamp($currentLine, $offsets[$index]);

		# Discovering gaps inside the log file: we consider a gap, when two successive lines in the
		# log differ by more than two HAD intervals
		if($currentTimestamp > $lastStatusTimestamp && 
	#	   $currentTimestamp - $previousTimestamp > 2 * $hadInterval)
		   $currentTimestamp - $previousTimestamp > $gap)
		{
			my $message = 'In log of ' . $sinfulStrings[$index] . ' there is a gap between ' . 
				      &ConvertTimestampToTime($previousTimestamp) . ' and ' . 
				      &ConvertTimestampToTime($currentTimestamp) . "\n";

			&AppendText($warningLogPath            , $message);
			&AppendText($consolidatedWarningLogPath, $message);
			&AppendText($warningHistoryLogPath     , $message)
		}
		# Discovering ungraceful exiting: it is important to do it prior to
		# discovering other events in this cycle
		if(grep(!/$exitingRegEx/, $previousLine) && grep(/$startingRegEx/, $currentLine))
		{
			my $ungracefulExitingTimestamp = $previousTimestamp;# + STABILIZATION_TIME;

			print $eventFileHandle $$refNewLineChar . "$ungracefulExitingTimestamp " . EXITING_EVENT;
			$$refNewLineChar = "\n";
		}

		$$refNewLineChar   = &DiscoverEvents($currentLine    , $currentTimestamp, $eventFileHandle, 
						     $$refNewLineChar, $monitoredDaemon);
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
##               $monitoredDaemon - the daemon, logs of which are processed                          #
## Return value: new value of $newLineChar ("\n" if anything was written to the event file,          #
##                                          "" - otherwise)                                          #
## Description : discovers interesting event from HAD log file line and records it to the event file #
######################################################################################################
sub DiscoverEvents
{
	my ($line, $timestamp, $eventFileHandle, $newLineChar, $monitoredDaemon) = (@_);
	
#	# Discovering starting
#        if(grep(/$startingRegEx/, $line))
#        {
#        	print $eventFileHandle $newLineChar . "$timestamp " . STARTING_EVENT;
#                return "\n";
#        }
#        # Discovering graceful exiting
#        if(grep(/$exitingRegEx/, $line))
#        {
#       		print $eventFileHandle $newLineChar . "$timestamp " . EXITING_EVENT;
#                return "\n";
#        }
#        # Discovering passing to leader state
#        if(grep(/$leaderRegEx/, $line))
#        { 
#       		print $eventFileHandle $newLineChar . "$timestamp " . LEADER_EVENT;
#       		return "\n";
#        }
#        # Discovering passing to backup state
#        if(grep(/$stateRegEx/, $line)) # grep(!/$leaderRegEx/, $line) && 
#        {
#        	print $eventFileHandle $newLineChar . "$timestamp " . BACKUP_EVENT;
#		return "\n";
#        }
	my $checkerName               = &CapitalizeFirst($monitoredDaemon);
	my $discoverEventFunctionName = $checkerName . "DiscoverEvent";

	require "$hadMonitoringSystemDirectory/Checkers/$checkerName.pl";

#	my $eventName                 = &{$discoverEventFunctionName}($line, \$additionalParameters[$index]);
	my $eventName                 = &{$discoverEventFunctionName}($line);

	return $newLineChar if $eventName eq "";
	print $eventFileHandle $newLineChar . "$timestamp " . $eventName;
	return "\n";
}
##########################################################################################
## Function    : FindTimestamp                                                           #
## Arguments   : $line - line of daemon log file                                         #
##               $offset - timestamp offset from the remote machine                      #
## Return value: timestamp - in seconds from epoche time (00:00:00 UTC, January 1, 1970) #
## Description : returns timestamp of the given HAD log file line                        #
##########################################################################################
#sub FindTimestamp
#{
#	my ($line, $offset) = (@_);
#
#	my @lineFields = split(' ', $line);
#	my $dateField  = $lineFields[0];
#	my $timeField  = $lineFields[1];
#
#	my ($month, $date)           = split('/', $dateField);
#	my ($hour, $minute, $second) = split(':', $timeField);
#	my $year                     = (localtime)[5] + 1900;
#
#	$month --;
#	$date   += 0;
#	$hour   += 0;
#	$minute += 0;
#	$second += 0;
#
#	my $timestamp = timelocal($second, $minute, $hour, $date, $month, $year);
#
#	# If we mistook in guessing the year number of the log, correct it by subtracting an
#	# entire year
#	$timestamp -= 3600 * 24 * 365 if $timestamp > time();
#	$timestamp += $offset;
#
#	return $timestamp;
#}
##########################################################################################
## Function    : ReturnFileContent                                                       #
## Arguments   : $filePath - file, whose content is to be returned                       #
## Return value: fileContent - array, containing file lines                              #
## Description : returns content of the specified file                                   #
##########################################################################################
sub ReturnFileContent
{
	my $filePath = shift;

	open(FILE, "< $filePath");
	my @fileContent = <FILE>;
	close(FILE);

	return @fileContent;
}
##########################################################################################
## Function    : CapitalizeFirst                                                         #
## Arguments   : $string - the string to be operated on                                  #
## Return value: given string in lowercase with first letter capitalized                 #
## Description : returns string in lowercase with first letter capitalized               #
##########################################################################################
sub CapitalizeFirst
{
	my $string = shift;

	return ucfirst(lc($string)); 
} 
########################################################################################## 
## Function : CalculateOffsets                                                           # 
## Arguments : @hadSinfulStrings - addresses of HADs, from machines of which the date    # 
##       			   is to be queried                                      # 
## Return value: @offsets - the array of timestamp offsets of local time from the HAD    # 
## 			    machines times                                               # 
## Description : returns the array of timestamp offsets of local time from the HAD       #
## 		 machines times                                                          #
##########################################################################################
sub CalculateOffsets {
        my @hadSinfulStrings = (@_);
        my @offsets          = ();

	if($isOffsetCalculationNeeded eq FALSE)
	{
		foreach my $machineIndex (0 .. $#hadSinfulStrings)
		{
			$offsets[$machineIndex] = 0;
		}
		return @offsets;
	}

        foreach my $hadSinfulString (@hadSinfulStrings)
        {
                $_ = $hadSinfulString;
                tr/<>//d;
                my ($hostName, $port) = split(':', $_);

                my $timestampAsString = `ssh technion_sharov\@$hostName \'date \"+%m/%d %H:%M:%S\"\'`;

                chomp($timestampAsString);

                my $remoteTimestamp  = &FindTimestamp($timestampAsString, 0);
                my $localTimestamp   = time();
#               my $roundingConstant = ($remoteTimestamp > $localTimestamp) ? 0.5 : -0.5;

#               push(@offsets, int($roundingConstant + ($remoteTimestamp - $localTimestamp) / 3600));
                push(@offsets, $localTimestamp - $remoteTimestamp);
        }
	&AppendTextToActivityLog("Calculated offsets for " . join(',', @hadSinfulStrings) . 
				 ": " . join(',', @offsets) . "\n");

        return @offsets;
}

############################## End of auxiliary functions ##################################
