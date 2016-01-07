#!/usr/bin/perl

use strict;
use warnings;

use Time::Local;

my $STARTLOG = "Started Negotiation Cycle";
my $ENDLOG = "Finished Negotiation Cycle";

sub openLogFile {
    my $filename = shift;

    open(my $LF, "<$filename") or die("Cannot open LogFile \n");
    return $LF;
}

sub closeLogFile {
    my $fh = shift;

    close($fh);
}

sub getNegotiatorLogFile {
    chomp(my $logdir = `condor_config_val log`);
    # how to handle windows?
    return $logdir . "/NegotiatorLog";
}

sub getNegotiationCycles {
    my $fh = shift;
    my @cycles;
    my $item = {};

    while (my $line = <$fh>) {
        if ($line =~ m/$STARTLOG/) {
            if (not exists $item->{start}) {
                $item->{start} = $line;
            } else {
                push @cycles, $item;
                $item = {};

                $item->{start} = $line;
            }
        } elsif ($line =~ m/$ENDLOG/) {
            $item->{stop} = $line;
            push @cycles, $item;
            $item = {};
        }
    }

    return @cycles;
}

sub getIncompleteNegotiationCycles {
    my $fh = shift;
    my @cycles;

    while (my $line = <$fh>) {
        if ($line =~ m/$STARTLOG/) {
            push @cycles, $line;
        } elsif ($line =~ m/$ENDLOG/) {
            pop @cycles;
        }
    }

    return @cycles;
}

sub extractEpochMsec {
    my $log = shift;
    my $ts = '(?P<month>\d+)\/(?P<day>\d+)\/(?P<year>\d+).(?P<hour>\d+):(?P<min>\d+):(?P<sec>\d+)(\.(?P<msec>\d+)|)';
    my $epoch = 0;

    if ($log =~ m/$ts/) {
        my $msec = 0;
        $epoch = timelocal($+{sec}, $+{min}, $+{hour}, $+{day}, $+{month}, $+{year});
        $msec = $+{msec} if exists $+{msec};
        $epoch = $epoch * 1000 + $msec;
    }

    return $epoch;
}

sub main {
    my $handle = openLogFile(getNegotiatorLogFile());

    my @cycles = getNegotiationCycles($handle);
    my $diff = 0;
    my $count = 0;

    foreach my $c (@cycles) {
        #print "$c->{start}";
        #print "$c->{stop}";
        if (exists $c->{start} and exists $c->{stop}) {
            $diff += extractEpochMsec($c->{stop}) - extractEpochMsec($c->{start});
            $count += 1;
        } 
    }

    if ($count == 0) {
        print("No logs present\n");
    } else {
        print("average cycle time is " . $diff / $count . "ms. No. of Cycles is $count\n");
    }

    my @incompleteCycles = getIncompleteNegotiationCycles($handle);

    my $nIncompleteCycles = @incompleteCycles;
    
    print("No. of Incomplete cycles is $nIncompleteCycles\n");	
    foreach my $c (@incompleteCycles) {
        my $ts = extractEpochMsec($c);
        print("$ts\n");
    }
    close($handle);
}

main();
