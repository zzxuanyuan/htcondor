#!/usr/bin/env perl
use Socket;

use constant DEFAULT_COLLECTOR_PORT => 9618;

PrintUsage();
my $command  = `which condor_config_val`;
my $userName = `whoami`;
chomp($command);
chomp($userName);
my $commandFound = `echo $command | grep "Command not found"`;

FailWithMessage("condor_config_val/condor_status are not present in your path.\n" .
                "Please, insert the directory of condor_config_val/condor_status into the path\n")
if $commandFound ne "";

####################
# Global variables #
####################

# Finding IPs of all condor masters
my @machinesAux = `condor_status -master -l -format \"\%s\\n\" MasterIpAddr`;
my @machines    = ();

delete $machinesAux[$#machinesAux];
chomp(@machinesAux);

map { $_ =~ /<(.*)>/; $_ = $1;} @machinesAux;

FailWithMessage("No master daemons found in the current pool. Please, raise the pool masters.\n")
if @machinesAux == 0;

print "All masters of the pool are        : ", join(',', @machinesAux), "\n";

my $currentHadList = `$command HAD_LIST`;

chomp($currentHadList);

# Finding IPs of all had condor masters
my @hadList             = split(/,/, EatSpaces($currentHadList));
	
@hadList                = HostnameToIp(@hadList);
@hadListWithoutPorts    = @hadList;

map { my ($ip, $port) = &IpPortSplit($_); $_ = $ip} @hadListWithoutPorts;

@hadListWithoutPorts    = sort(@hadListWithoutPorts);

foreach my $hadWithoutPort (@hadListWithoutPorts)
{
	foreach my $machineAux (@machinesAux)
	{
		if("$machineAux" =~ "$hadWithoutPort")
		{
			push(@machines, $machineAux);

			last;
		}
	}
}
# Pre-fetching primary had IP
my $primaryHad = $hadList[0];
my ($primaryHadIp, $primaryHadPort) = IpPortSplit($primaryHad);
print "The masters on the HAD machines are: ", join(',', @machines), "\n";
print "Local(on this machine) HAD_LIST is : ", join(',', @hadList), "\n";

# Test number counter
my $ordinalNumber = 1;
# Sanity check number 1: identity of HAD_LISTs on all the master daemon machines
# and HAD_ARGS port matches the port, specified in HAD_LIST
$ordinalNumber = IncreaseOrdinalNumber($ordinalNumber);

my $firstMachine;
my @correctList;
foreach my $machine (@machines)
{
	$firstMachine  = $machine if !defined($firstMachine);
	print("$machine is being processed\n");

	my @hadList = `$command -address \"<$machine>\" HAD_LIST`;
	chomp(@hadList);

	print "HAD_LIST in $machine : ", join(',', @hadList), "\n";

	@hadList = split(/,/, EatSpaces("@hadList"));
	@hadList = HostnameToIp(@hadList);
	FailWithMessage("HAD_LIST is empty at $machine") if ("@hadList" eq "");
	@correctList = @hadList if !defined(@correctList);
	FailWithMessage("HAD_LISTs are not identical on $firstMachine and $machine (@hadList vs. @correctList)")
	if("@hadList" ne "@correctList");

	my ($ip, $port)    = IpPortSplit($machine);
	my $currentHadArgs = `$command -address \"<$machine>\" HAD_ARGS`;
	
	chomp($currentHadArgs);
	FailWithMessage("HAD_ARGS are not defined at $machine") if ($currentHadArgs =~ "^Not defined");

	my @hadArgs = split(/ /, $currentHadArgs);

	$currentHadArgs = "";

	foreach my $index (0 .. $#hadArgs - 1)
	{
		if($hadArgs[$index] eq '-p')
		{
			$currentHadArgs = $hadArgs[$index + 1];
			last;
		}
	}
	
	print "HAD_ARGS in $machine : ", $currentHadArgs, "\n";

	my $hadPort = "";

	foreach my $had (@hadList)
	{
		my ($hadIp, $port) = IpPortSplit($had);

		$hadPort = $port if $hadIp eq $ip;
	}
	
	FailWithMessage("HAD_ARGS port is not consistent with that of HAD_LIST at $machine ($currentHadArgs and $hadPort)") 
	if ($currentHadArgs ne $hadPort);
}
print("SUCCESSFUL\n");
# Sanity check number 2: HAD_USE_PRIMARY cannot be equal to true in one machine configuration file
# and to false (or undefined) in another
#$ordinalNumber = IncreaseOrdinalNumber($ordinalNumber);
#
#my $firstHadUsePrimary;
#my $firstMachine;
#
#foreach my $machine (@machines)
#{
#	my ($ip, $port) = IpPortSplit($machine);
#
#	print("$machine is being processed\n");
#	
#	my @currentHadUsePrimary = split(/ /, `$command -address \"<$machine>\" HAD_USE_PRIMARY`);
#	
#	chomp(@currentHadUsePrimary);
#
#	my $hadUsePrimary        = join(' ', @currentHadUsePrimary);
#	
#	print("HAD_USE_PRIMARY in $machine : " . $hadUsePrimary . "\n");
#	$hadUsePrimary = "false" if $hadUsePrimary eq "Not defined";
#
#	FailWithMessage("HAD_USE_PRIMARY value $hadUsePrimary in $machine is not valid, must be one of\n" .
#	                "{true, false} or undefined at all") 
#	if $hadUsePrimary  ne "true"        && 
#	   $hadUsePrimary  ne "false"       &&
#	   $$hadUsePrimary ne "Not defined";
#
#	if(!defined($firstHadUsePrimary))
#	{
#		$firstHadUsePrimary = $hadUsePrimary;
#		$firstMachine       = $machine;
#	}
#
#	if($firstHadUsePrimary ne $hadUsePrimary)
#	{
#		FailWithMessage("HAD_USE_PRIMARY definitions are contradictory on $firstMachine and $machine");
#	}
#}
#
#print("SUCCESSFUL\n");
# Sanity check number 2: HAD_CONNECTION_TIMEOUT is the same on all the machines
$ordinalNumber = IncreaseOrdinalNumber($ordinalNumber);

my $hadCycleMachine;
my $hadCycleInterval;
foreach my $machine (@machines)
{
	print("$machine is being processed\n");
    my $currentHadCycleInterval = `$command -address \"<$machine>\" HAD_CONNECTION_TIMEOUT`;
	chomp($currentHadCycleInterval);

	FailWithMessage("HAD_CONNECTION_TIMEOUT is not defined at $machine") if($currentHadCycleInterval =~ "^Not defined");
	$currentHadCycleInterval = EatSpaces($currentHadCycleInterval);
	print "HAD_CONNECTION_TIMEOUT in $machine : ", $currentHadCycleInterval, "\n";
	$hadCycleMachine  = $machine                 if !defined($hadCycleMachine);
	$hadCycleInterval = $currentHadCycleInterval if !defined($hadCycleInterval);
	FailWithMessage("HAD_CONNECTION_TIMEOUT is defined differently at $machine and $hadCycleMachine " .
	"($currentHadCycleInterval and $hadCycleInterval)") if ($currentHadCycleInterval != $hadCycleInterval);
}
print("SUCCESSFUL\n");
# Sanity check number 3: COLLECTOR_HOST names correspond to the HAD_LIST ip addresses
# and COLLECTOR_ARGS port matches the port, specified in COLLECTOR_HOST
$ordinalNumber = IncreaseOrdinalNumber($ordinalNumber);

foreach my $machine (@machines)
{
	print("$machine is being processed\n");
	my $currentCollectorList = `$command -address \"<$machine>\" COLLECTOR_HOST`;
	chomp($currentCollectorList);
	print "COLLECTOR_HOST in $machine : ", $currentCollectorList, "\n";
	FailWithMessage("HAD_LIST is not defined at $machine") if ($currentCollectorList =~ "^Not defined");
	
	my @collectorList = split(/,/, EatSpaces($currentCollectorList));

	# Taking care of the case when the port is not specified and the default collector port is used
	map { $_ .= ":@{[DEFAULT_COLLECTOR_PORT]}" if $_ !~ ":"; } @collectorList;
	@collectorList = HostnameToIp(@collectorList);
	
	my $currentCollectorArgs = `$command -address \"<$machine>\" COLLECTOR_ARGS`;
	
	chomp($currentCollectorArgs);

	$currentCollectorArgs = "-p @{[DEFAULT_COLLECTOR_PORT]}" if ($currentCollectorArgs =~ "^Not defined");

	my @collectorArgs = split(/ /, $currentCollectorArgs);

	$currentCollectorArgs = "";

	foreach my $index (0 .. $#collectorArgs - 1)
	{
		if($collectorArgs[$index] eq '-p')
		{
			$currentCollectorArgs = $collectorArgs[$index + 1];
			last;
		}
	}

	print "COLLECTOR_ARGS in $machine : ", $currentCollectorArgs, "\n";

	if($currentCollectorArgs eq "")
	{
		$currentCollectorArgs = DEFAULT_COLLECTOR_PORT;
		print "COLLECTOR_ARGS defaults to @{[DEFAULT_COLLECTOR_PORT]} since it is not specified\n";
	}

	foreach my $collector (@collectorList)
	{
		my ($ip, $port) = &IpPortSplit($collector);

		FailWithMessage("COLLECTOR_ARGS and COLLECTOR_HOST port are contradictory in $machine : ".
		                "($currentCollectorArgs vs. $port)") if $machine =~ $ip && $port != $currentCollectorArgs;
	}

	@collectorListWithoutPorts = HostnameToIp(@collectorList);
	@collectorListWithoutPorts = sort(@collectorListWithoutPorts);

	# Taking care of the case when the port is not specified and the default collector port is used
	map { my ($ip, $port) = &IpPortSplit($_);
		  $_ = $ip} @collectorListWithoutPorts;
	
	FailWithMessage("COLLECTOR_HOST is different from HAD_LIST at $machine (" . "@collectorList" . " and " . "@hadList" . 
")") 
	if ("@hadListWithoutPorts" ne "@collectorListWithoutPorts");
}
print("SUCCESSFUL\n");
# Sanity check number 4: DAEMON_LIST and DC_DAEMON_LIST contain HAD, NEGOTIATOR and COLLECTOR daemons to babysit
$ordinalNumber = IncreaseOrdinalNumber($ordinalNumber);
foreach my $machine (@machines)
{
	print("$machine is being processed\n");
	my $currentDaemonList = `$command -address \"<$machine>\" DAEMON_LIST`;
	chomp($currentDaemonList);
	FailWithMessage("DAEMON_LIST is not defined at $machine") if ($currentDaemonList =~ "^Not defined");
	print "DAEMON_LIST in $machine    : ", $currentDaemonList, "\n";
	$currentDaemonList = EatSpaces($currentDaemonList);
	my @daemonList = split(/,/, $currentDaemonList);
	my $containsHad        = ((' ' . join(' ', @daemonList) . ' ') =~ ' HAD ');
	my $containsCollector  = ((' ' . join(' ', @daemonList) . ' ') =~ ' COLLECTOR ');
	my $containsNegotiator = ((' ' . join(' ', @daemonList) . ' ') =~ ' NEGOTIATOR ');
	FailWithMessage("DAEMON_LIST at $machine doesn't contain HAD but contains COLLECTOR")  if !$containsHad && 
$containsCollector;
	FailWithMessage("DAEMON_LIST at $machine doesn't contain COLLECTOR but contains HAD")  if $containsHad  && 
!$containsCollector;
	FailWithMessage("DAEMON_LIST at $machine doesn't contain HAD but contains NEGOTIATOR") if !$containsHad && 
$containsNegotiator;
	FailWithMessage("DAEMON_LIST at $machine doesn't contain NEGOTIATOR but contains HAD") if $containsHad  && 
!$containsNegotiator;

	my $currentDCDaemonList = `$command -address \"<$machine>\" DC_DAEMON_LIST`;

	chomp($currentDCDaemonList);
	FailWithMessage("DC_DAEMON_LIST is not defined at $machine") if ($currentDCDaemonList =~ "^Not defined");
	print "DC_DAEMON_LIST in $machine : ", $currentDCDaemonList, "\n";
	$currentDCDaemonList = EatSpaces($currentDCDaemonList);

	my @dcDaemonList = split(/,/, $currentDCDaemonList);

	my $dcContainsHad        = ((' ' . join(' ', @dcDaemonList) . ' ') =~ ' HAD ');
	my $dcContainsCollector  = ((' ' . join(' ', @dcDaemonList) . ' ') =~ ' COLLECTOR ');
	my $dcContainsNegotiator = ((' ' . join(' ', @dcDaemonList) . ' ') =~ ' NEGOTIATOR ');

	FailWithMessage("DC_DAEMON_LIST at $machine doesn't contain HAD but contains COLLECTOR")  if !$dcContainsHad && 
$dcContainsCollector;
	FailWithMessage("DC_DAEMON_LIST at $machine doesn't contain COLLECTOR but contains HAD")  if $dcContainsHad  && 
!$dcContainsCollector;
	FailWithMessage("DC_DAEMON_LIST at $machine doesn't contain HAD but contains NEGOTIATOR") if !$dcContainsHad && 
$dcContainsNegotiator;
	FailWithMessage("DC_DAEMON_LIST at $machine doesn't contain NEGOTIATOR but contains HAD") if $dcContainsHad  && 
!$dcContainsNegotiator;
}
print("SUCCESSFUL\n");
# Sanity check number 5: HAD_ARGS and HAD_LIST definitions are consistent in their ports numbers
#$ordinalNumber = IncreaseOrdinalNumber($ordinalNumber);
#
#foreach my $machine (@machines)
#{
#	my ($ip, $port) = IpPortSplit($machine);
#
#	print("$machine is being processed\n");
#
#	my $currentHadArgs = `$command -address \"<$machine>\" HAD_ARGS`;
#	
#	chomp($currentHadArgs);
#	FailWithMessage("HAD_ARGS are not defined at $machine") if ($currentHadArgs =~ "^Not defined");
#
#	my @hadArgs = split(/ /, $currentHadArgs);
#
#	$currentHadArgs = "";
#
#	foreach my $index (0 .. $#hadArgs - 1)
#	{
#		if($hadArgs[$index] eq '-p')
#		{
#			$currentHadArgs = $hadArgs[$index + 1];
#			last;
#		}
#	}
#	
#	print "HAD_ARGS in $machine : ", $currentHadArgs, "\n";
#
#	my $hadPort = "";
#
#	foreach my $had (@hadList)
#	{
#		my ($hadIp, $port) = IpPortSplit($had);
#
#		$hadPort = $port if $hadIp eq $ip;
#	}
#	
#	FailWithMessage("HAD_ARGS port is not consistent with that of HAD_LIST at $machine ($currentHadArgs and $hadPort)") 
#	if ($currentHadArgs ne $hadPort);
#}
#
#print("SUCCESSFUL\n");

# Sanity check number 5: HOSTALLOW_NEGOTIATOR and HOSTALLOW_ADMINISTRATOR contain all machines 
# specified in COLLECTOR_HOST definition for an appropriate machine

$ordinalNumber = IncreaseOrdinalNumber($ordinalNumber);

foreach my $machine (@machines)
{
	print("$machine is being processed\n");

	my $currentHostallowNegotiator    = `$command -address \"<$machine>\" HOSTALLOW_NEGOTIATOR`;

	chomp($currentHostallowNegotiator);
	print "HOSTALLOW_NEGOTIATOR in $machine    : ", $currentHostallowNegotiator, "\n";

	my @hostallowNegotiator           = split(/,/, EatSpaces($currentHostallowNegotiator));
	
	if("@hostallowNegotiator" ne '*')
	{
		@hostallowNegotiator = HostnameToIp(@hostallowNegotiator);
	
		foreach my $had (@hadList)
		{
			my ($hadIp, $hadPort)     = IpPortSplit($had);
			my $isHostContained = 0;

			foreach my $hostallow (@hostallowNegotiator)
			{
				my ($hostallowIp, $hostallowPort) = IpPortSplit($hostallow);

				if($hadIp eq $hostallowIp)
				{
					$isHostContained = 1;
					last;
				}
			}

			FailWithMessage("$ip not contained in HOSTALLOW_NEGOTIATOR of $machine, although specified in 
COLLECTOR_HOST")
			if $isHostContained == 0;
		}
	}
	else
	{
		print "HOSTALLOW_NEGOTIATOR contains all machines from COLLECTOR_HOST since it is equal to *\n";
	}

	my $currentHostallowAdministrator = `$command -address \"<$machine>\" HOSTALLOW_ADMINISTRATOR`;

	chomp($currentHostallowAdministrator);
	print "HOSTALLOW_ADMINISTRATOR in $machine : ", $currentHostallowAdministrator, "\n";

	my @hostallowAdministrator        = split(/,/, EatSpaces($currentHostallowAdministrator));

	if("@hostallowAdministrator" ne '*')
	{
		@hostallowAdministrator = HostnameToIp(@hostallowAdministrator);

		foreach my $had (@hadList)
		{
			my ($hadIp, $hadPort)     = IpPortSplit($had);
			my $isHostContained = 0;

			foreach my $hostallow (@hostallowAdministrator)
			{
				my ($hostallowIp, $hostallowPort) = IpPortSplit($hostallow);

				if($hadIp eq $hostallowIp)
				{
					$isHostContained = 1;
					last;
				}
			}

			FailWithMessage("$ip not contained in HOSTALLOW_ADMINISTRATOR of $machine, although specified in 
COLLECTOR_HOST")
			if $isHostContained == 0;
		}
	}
	else
	{
		print "HOSTALLOW_ADMINISTRATOR contains all machines from COLLECTOR_HOST since it is equal to *\n";
	}
}

print("SUCCESSFUL\n");

# Sanity check number 6: REPLICATION_HOST entries correspond to the entries of HAD_LIST, i.e.
# nth IP in REPLICATION_LIST is identical to this of HAD_LIST. Besides, it checks that REPLICATION_ARGS 
# port matches the port, specified in REPLICATION_LIST for each machine. This check is vacuously successful,
# when the replication is disabled by configuration on all the machines
                                        
$ordinalNumber = IncreaseOrdinalNumber($ordinalNumber);

foreach my $machine (@machines)
{
	print("$machine is being processed\n");

	my ($machineIp, $machinePort) = IpPortSplit($machine);
	my $hadUseReplication = `$command -address \"<$machine>\" HAD_USE_REPLICATION`;

	chomp($hadUseReplication);
	$hadUseReplication = uc($hadUseReplication);

	print("HAD_USE_REPLICATION value on $machine : " . $hadUseReplication . "\n");
	next if $hadUseReplication ne 'TRUE';

	my @hadList = `$command -address \"<$machine>\" HAD_LIST`;
        chomp(@hadList);
        
        print "HAD_LIST in $machine                  : ", join(',', @hadList), "\n";
        
        @hadList = split(/,/, EatSpaces("@hadList"));
        @hadList = HostnameToIp(@hadList);  

	my @replicationList = `$command -address \"<$machine>\" REPLICATION_LIST`;
	chomp(@replicationList);

	print "REPLICATION_LIST in $machine          : ", join(',', @replicationList), "\n";

	@replicationList = split(/,/, EatSpaces("@replicationList"));
	@replicationList = HostnameToIp(@replicationList);

	FailWithMessage("HAD_LIST and REPLICATION_LIST on $machine are of different lengths: " . 
			join(',', @hadList) . " vs. " . join(',', @replicationList) . "\n")
                        if @hadList != @replicationList;
	my $replicationArgs = `$command -address \"<$machine>\" REPLICATION_ARGS`;
	
	chomp($replicationArgs);
	$replicationArgs =~ /-p ([0-9]+)/;
	my $replicationArgsPort = $1;

	FailWithMessage("REPLICATION_ARGS port is not defined in $machine\n")
	if !defined($replicationArgsPort);

	print "REPLICATION_ARGS port in $machine     : ", $replicationArgsPort , "\n";
	my $listIndex = 0;

	while($listIndex < @hadList)
        {
                my ($hadIp, $hadPort)                 = IpPortSplit($hadList[$listIndex]);
	        my ($replicationIp, $replicationPort) = IpPortSplit($replicationList[$listIndex]);

		FailWithMessage("IP of entry #" . $listIndex . " in HAD_LIST does not correspond " .
				"to that of the REPLICATION_LIST: " .
				$hadIp . " vs. " . $replicationIp . "\n")
		if $hadIp ne $replicationIp;

		FailWithMessage("REPLICATION_ARGS port and REPLICATION_LIST entry for $machine do not correspond:\n" .
				$replicationArgsPort . " vs. " . $replicationPort . "\n")
		if $machineIp eq $replicationIp && $replicationPort != $replicationArgsPort;

		$listIndex ++;
        }
}

print("SUCCESSFUL\n");

#########################
# Auxiliary subroutines #
#########################

# Prints information message before starting the script
sub PrintUsage()
{
	print("\nThis script tests whether the running condor pool with high availability\n" . 
	      "daemons is well configured. All the checks are done by applying condor_config_val\n" .
		  "command on running master daemons in HAD-enabled machines and by applying\n" . 
		  "condor_status command. Namely, it tests the following configuration options:\n\n" .
		  "1. Identity of HAD_LISTs in all the pool machines configuration files\n" .
		  "and HAD_ARGS port matches the port, specified in HAD_LIST\n" .
#		  "2. HAD_USE_PRIMARY cannot be equal to true in one machine configuration file\n" .
#		  "and false (or undefined) in another\n" .
		  "2. HAD_CONNECTION_TIMEOUT is the same on all the machines\n" .
		  "3. COLLECTOR_HOST names correspond to the HAD_LIST ip addresses\n" .
		  "and COLLECTOR_ARGS port matches the port, specified in COLLECTOR_HOST\n" .
		  "4. DAEMON_LIST and DC_DAEMON_LIST of HAD-enabled machine contain HAD,\n" . 
		  "NEGOTIATOR and COLLECTOR daemons to babysit\n" .
		  "5. HOSTALLOW_NEGOTIATOR and HOSTALLOW_ADMINISTRATOR contain all machines specified in\n" .
		  "COLLECTOR_HOST definition for an appropriate machine\n" .
		  "6. REPLICATION_HOST entries correspond to the entries of HAD_LIST, i.e.\n" .
		  "nth IP in REPLICATION_LIST is identical to this of HAD_LIST.\n" . 
		  "Besides, it checks that REPLICATION_ARGS port matches the port, specified in REPLICATION_LIST\n" .
		  "for each machine. This check is vacuously successful, when the replication is disabled by\n" . 
		  "configuration on all the machines\n" .
		  "\n");
}

sub IncreaseOrdinalNumber
{
	my $ordinalNumber = shift;

	print("\nSanity check number $ordinalNumber started:\n");
	$ordinalNumber ++;

	return $ordinalNumber;
}
sub FailWithMessage
{
	($message) = @_;
	print("FAILED: $message\n");
	exit;
}

# Return separately the IP and port from machine of type: AAA.BBB.CCC.DDD:PPPP or <AAA.BBB.CCC.DDD:PPPP>
sub IpPortSplit
{
	my $machine =  shift;

	$machine    =~ s/<//;
	$machine    =~ s/>//;
	($ip, $port) =  split(':',$machine);

	return $ip, $port;
}

# Erases all spaces from the parameter
sub EatSpaces
{
	$_ = shift;

	tr/\n //d;

	return $_;
}

# Turns the given hostname:port or <hostname:port> list to ip:port list
# If the list is specified without ports then it is transformed to ip:0 list
sub HostnameToIp
{
	my (@list) = @_;

	return map {
		 tr/<>//d; 
		 /(.*):(.*)/; 
		 $_ = $1 || $_; 
		 ($name, $aliases, $addressType, $length, @addresses) = gethostbyname("$_");
		 ($first, $second, $third, $fourth) = unpack('C4',$addresses[0]);
		 $_ = "$first.$second.$third.$fourth:$2" || "$first.$second.$third.$fourth:0";} 
	@list;
}
