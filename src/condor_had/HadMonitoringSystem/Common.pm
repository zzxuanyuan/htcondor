#!/bin/env perl

package Common; 

require Exporter;

@ISA = qw(Exporter);      # Take advantage of Exporter's capabilities

use Time::Local;
use POSIX qw(strftime);

use constant TRUE          => 1;
use constant FALSE         => 0;
use constant DOWN_STATUS   => 'Down';
use constant EXITING_EVENT => 'Exiting';
use constant MAX_INT       => 999999999999;

# Debugging levels
use constant INFO          => 0;
use constant DEBUG         => 1;

my $hadList                = "";
my $hadConnectionTimeout   = 0;
my $replicationInterval    = 0;
my $isPrimaryUsed          = TRUE;
my $replicationList        = "";
my $collectorHost          = "";
my $messagesPerStateFactor = 2;

@EXPORT_OK = qw(TRUE FALSE DOWN_STATUS EXITING_EVENT MAX_INT INFO DEBUG FindTimestamp ConvertTimestampToTime RemoveAllFiles $hadList $hadConnectionTimeout $replicationInterval $isPrimaryUsed $replicationList $collectorHost $messagesPerStateFactor);

##########################################################################################
## Function    : FindTimestamp                                                           #
## Arguments   : $line - line of daemon log file                                         #
##               $offset - timestamp offset from the remote machine                      #
## Return value: timestamp - in seconds from epoche time (00:00:00 UTC, January 1, 1970) #
## Description : returns timestamp of the given HAD log file line                        #
##########################################################################################
sub FindTimestamp
{       
        my ($line, $offset) = (@_);
        
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
        $timestamp += $offset;
 
        return $timestamp;
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
## Function    : RemoveAllFiles                                                            #
## Arguments   : $directory - the directory to remove the files from                       #
## Description : removes all files from the specified directory                            #
############################################################################################
sub RemoveAllFiles
{
        my $directory = shift;

        my @filePaths = glob("$directory/*");

        foreach my $filePath (@filePaths)
        {
                unlink($filePath) or warn "unlink: $!";
        }
}

1;
