#!/bin/env perl

use Common qw(RemoveAllFiles);

my $hadMonitoringSystemDirectory = $ENV{MONITORING_HOME} || $ENV{PWD};
my $archiveLogsDirectory          = $hadMonitoringSystemDirectory . "/ArchiveLogs";

&RemoveAllFiles($archiveLogsDirectory);
