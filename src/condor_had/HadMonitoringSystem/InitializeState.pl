#!/bin/env perl

my $hadMonitoringSystemDirectory = $ENV{MONITORING_HOME} || $ENV{PWD};
my $stateFilePathPrefix          = $hadMonitoringSystemDirectory . "/State";

my @stateFilePaths = glob("$stateFilePathPrefix-*");

foreach my $stateFilePath (@stateFilePaths)
{
	unlink($stateFilePath);
}
