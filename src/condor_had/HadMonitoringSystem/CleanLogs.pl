#!/bin/env perl

use Common qw(RemoveAllFiles);

my $hadMonitoringSystemDirectory = $ENV{MONITORING_HOME} || $ENV{PWD};
my $eventFilesDirectory          = $hadMonitoringSystemDirectory . "/EventFiles";
my $warningLogsDirectory         = $hadMonitoringSystemDirectory . "/WarningLogs";
my $errorLogsDirectory           = $hadMonitoringSystemDirectory . "/ErrorLogs"; 
my $outputLogsDirectory          = $hadMonitoringSystemDirectory . "/OutputLogs";
my $daemonLogsDirectory          = $hadMonitoringSystemDirectory . "/DaemonLogs";

&RemoveAllFiles($eventFilesDirectory);
&RemoveAllFiles($warningLogsDirectory);
&RemoveAllFiles($errorLogsDirectory);
&RemoveAllFiles($outputLogsDirectory);
&RemoveAllFiles($daemonLogsDirectory);

#sub RemoveAllFiles
#{
#	my $directory = shift;
#
#	my @filePaths = glob("$directory/*");
#
#	foreach my $filePath (@filePaths)
#	{
#		unlink($filePath) or warn "unlink: $!";
#	}
#}
