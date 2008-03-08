#!/usr/bin/env perl
#
##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************


#
# Amazon EC2/S3 Control Tool
# V0.3 / 2008-Feb-13 / Jaeyoung Yoon / jyoon@cs.wisc.edu
#

use strict;
#use warnings;

use FindBin();
use lib "$FindBin::Bin";

use File::Basename;
use File::stat;

use Getopt::Std;
use Getopt::Long;
use Net::Amazon::EC2;
use Net::Amazon::S3;

#use Crypt::SSLeay
my $use_ssl = 0;

# program version information
my $progdesc = "Condor Amazon EC2/S3 Perl Tool";
my $version = "0.1";
my $verbose = undef;
my $progname = $0;
my $access_public_key = undef;
my $access_secret_key = undef;
my $owner_id = undef;
my $owner_displayname = undef;
my $xml_acl_string = undef; 

sub printerror
{
	print STDOUT "(ERROR) @_\n";
	print STDOUT "#ECODE=EC2SCRIPTERROR\n";
	print STDOUT "#ERROR\n";
	exit(1);
}

sub printverbose
{
	if( defined($verbose) ) {
		print STDERR "EC2/S3: @_\n";
	}
	return;
}

sub createErrorOutput
{
	my $error_string = $_[0];
	my $error_code = $_[1];

	if( $error_string ) {
		print "$error_string\n";
	}else {
		print "UnknownError\n";
	}

	if( $error_code ) {
		print "#ECODE=$error_code\n";
	}else {
		print "#ECODE=UNKNOWNERROR\n";
	}
	print "#ERROR\n";
	exit(1);
	return;
}

sub createSuccessOutput
{
	my $result_string = $_[0];

	print "$result_string\n";
	return;
}

sub printSuccessOutput
{
	print "#SUCCESS\n";
	return;
}

sub checkEC2Error
{

	my $ec2 = shift;
	my $need_define = shift;
	my $errors = shift;

	if( ! defined($errors) ) {
		if( defined ($ec2->{error}) ) {
			createErrorOutput( $ec2->{error}, $ec2->{errorcode} );
		}else {
			# It may be no error
			if( $need_define ) {
				createErrorOutput();
			}
			return;
		}
	}

	if( ! ref($errors) ) {
		# no reference at all
		return;
	}else {
		eval { my $tmperrors = $errors->errors };
		if($@) {
			# This is not an error object
			return;
		}else {
			# there is an error
			my $error = $errors->errors->[0];
			createErrorOutput( $error->message, $error->code );
		}
	}
}

# format : accessfile secretfile bucketname keyname
sub _set_acl_for_ec2 {
	printverbose "_set_acl_for_ec2 is called(@_)";

	my $accessfile = shift;
	my $secretfile = shift;
	my $bucketname = shift; 
	my $keyname = shift || '';

	if( ! defined($access_public_key) || ! defined($access_secret_key) ) {
		# Read access key file
		readAccessKey($accessfile, $secretfile);
	}

	my $s3 = Net::Amazon::S3->new( 
		{	aws_access_key_id		=> $access_public_key,
			aws_secret_access_key	=> $access_secret_key,
			retry			=> 1,
			# secure		=> 1,
			# timeout		=> 30,
		}
	);

	if( ! defined($s3) ) {
		printerror "Cannot allocate a new s3 handler"; 
	}

	if( ! defined($xml_acl_string) ) {
		# Get owner_id and owner_displayname

		my $tmpresponse = $s3->buckets;

		if( ! defined($tmpresponse) ) {
			# It means that error happened
			createErrorOutput($s3->errstr, $s3->err);
		}

		$owner_id = $tmpresponse->{owner_id};
		$owner_displayname = $tmpresponse->{owner_displayname};

		if ( ! $owner_id || ! $owner_displayname ) {
			printerror "Cannot get owner_id and displayname"; 
		}

		$xml_acl_string = 
		qq~<?xml version="1.0" encoding="UTF-8"?>
		<AccessControlPolicy xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
			<Owner>
				<ID>$owner_id</ID>
				<DisplayName>$owner_displayname</DisplayName>
			</Owner>
			<AccessControlList>
				<Grant>
					<Grantee xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:type="CanonicalUser">
					<ID>$owner_id</ID>
					<DisplayName>$owner_displayname</DisplayName>
					</Grantee>
					<Permission>FULL_CONTROL</Permission>
				</Grant>
				<Grant>
					<Grantee xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:type="CanonicalUser">
					<ID>6aa5a366c34c1cbe25dc49211496e913e0351eb0e8c37aa3477e40942ec6b97c</ID>
					<DisplayName>ec2-ami-retriever</DisplayName>
					</Grantee>
					<Permission>READ</Permission>
				</Grant>
			</AccessControlList>
		</AccessControlPolicy>~;
	}

	# Set acl 
	my $bucket = $s3->bucket($bucketname);
	$bucket->set_acl( 
		{
			key => $keyname,
			acl_xml => $xml_acl_string,
		}
	) or createErrorOutput($s3->errstr, $s3->err);

	# Get acl again 
	#my $acl = bucket->get_acl( 
	#	{
	#		key => $keyname,
	#	}
	#) or createErrorOutput($s3->errstr, $s3->err);
	#print "acl = $acl\n";

	return;	
}

sub createprocess
{
	defined(my $pid = fork ) or printerror "Cannot fork: $!";
	unless ( $pid ) {
		# Child process is here
		exec $progname, @ARGV;
		printerror "cannot exec $progname @ARGV: $!";
	}
#	print "fork=$pid\n";
}

sub waitallchilds
{
	my $kid;
	do {
		$kid = waitpid(-1, 0);
#		print "waitpid = $kid\n";
	} while $kid > 0;
}

sub usage()
{

print STDERR "$progdesc (version $version)\n";
print STDERR <<EOF;

Usage: $progname command [parameters]

EC2-Command         Parameters 
start           -a <accesskeyfile> -s <secretkeyfile> -id <AMI-id> [ -key <loginkeypair> -group <groupname> -userdata <base64 encoded data> -instancetype <m1.small/m1.large/m1.xlarge>  ]
stop            -a <accesskeyfile> -s <secretkeyfile> -id <instance-id>
reboot          -a <accesskeyfile> -s <secretkeyfile> -id <instance-id>
status          -a <accesskeyfile> -s <secretkeyfile> [ -instanceid <instance-id> -amiid <ami-id> -group <groupname> ]

images          -a <accesskeyfile> -s <secretkeyfile> [ -id <AMI-id> -owner <owner> -owner <anotherowner> -executableby <user> -executableby <anotheruser> -location <location on S3> ]
registerimage   -a <accesskeyfile> -s <secretkeyfile> -location <location on S3>
deregisterimage -a <accesskeyfile> -s <secretkeyfile> -id <ami-id>

groupnames      -a <accesskeyfile> -s <secretkeyfile>
creategroup     -a <accesskeyfile> -s <secretkeyfile> -group <groupname> -desc <groupDescription>
delgroup        -a <accesskeyfile> -s <secretkeyfile> -group <groupname>

grouprules      -a <accesskeyfile> -s <secretkeyfile> -group <groupname>
setgroupingress -a <accesskeyfile> -s <secretkeyfile> -group <groupname> -protocol <protocol> -fromport <fromport> -toport <toport> [ -iprange <iprange> ]
delgroupingress -a <accesskeyfile> -s <secretkeyfile> -group <groupname> -protocol <protocol> -fromport <fromport> -toport <toport> [ -iprange <iprange> ]

loginkeynames   -a <accesskeyfile> -s <secretkeyfile>
createloginkey  -a <accesskeyfile> -s <secretkeyfile> -keyname <keyname> -output <outputfile>
deleteloginkey  -a <accesskeyfile> -s <secretkeyfile> -keyname <keyname>
showlaunchpermission -a <accesskeyfile> -s <secretkeyfile> -id <ami-id>
addlaunchpermission -a <accesskeyfile> -s <secretkeyfile> -id <ami-id> -userid <userid>
removelaunchpermission -a <accesskeyfile> -s <secretkeyfile> -id <ami-id> -userid<userid>
resetlaunchpermission -a <accesskeyfile> -s <secretkeyfile> -id <ami-id>


#######################################################################################

S3-Command         Parameters 
listallbuckets  -a <accesskeyfile> -s <secretkeyfile>
createbucket    -a <accesskeyfile> -s <secretkeyfile> -bucket <bucketname>
listbucket      -a <accesskeyfile> -s <secretkeyfile> -bucket <bucketname> [ -prefix <prefix> -marker <marker> ]
deletebucket    -a <accesskeyfile> -s <secretkeyfile> -bucket <bucketname> [ -force ]
uploadfile      -a <accesskeyfile> -s <secretkeyfile> -file <filename> -bucket <bucketname> [ -keyname <keyname> -mimetype [mimetype] -ec2 ]
uploaddir       -a <accesskeyfile> -s <secretkeyfile> -dir <directory> -bucket <bucketname> [ -ec2 ]
downloadfile    -a <accesskeyfile> -s <secretkeyfile> -name <keyname> -bucket <bucketname> [ -output <outputfile> ]
downloadbucket  -a <accesskeyfile> -s <secretkeyfile> -bucket <bucketname> [ -outputdir <outputdir> ]
deletefile      -a <accesskeyfile> -s <secretkeyfile> -name <keyname> -bucket <bucketname>
deleteallfilesinbucket -a <accesskeyfile> -s <secretkeyfile> -bucket <bucketname>
setec2acl       -a <accesskeyfile> -s <secretkeyfile> -bucket <bucketname>

EOF
exit(1);
}

sub readAccessKey
{
	my $accesskeyfile = $_[0];
	my $privatekeyfile = $_[1];

	if( ! $accesskeyfile || ! $privatekeyfile ) {
		usage();
	}

	unless( -r $accesskeyfile ) { 
		printerror "Access key file $accesskeyfile is not readable.";
	}
	unless( -r $privatekeyfile ) { 
		printerror "Access key file $privatekeyfile is not readable.";
	}

	# Read access key file
	open(AWSKEYFILE, "$accesskeyfile")
		or printerror "Cannot open the file($accesskeyfile) : $!";

	my $key_line = <AWSKEYFILE>;
	chomp($key_line);
	close AWSKEYFILE;
	$access_public_key = $key_line;

	# Read private key file
	open(AWSKEYFILE, "$privatekeyfile")
		or printerror "Cannot open the file($privatekeyfile) : $!";

	$key_line = <AWSKEYFILE>;
	chomp($key_line);
	close AWSKEYFILE;
	$access_secret_key = $key_line;

	if( ! $access_public_key ) {
		printerror "File('$accesskeyfile') must contain the AWS access key";
	}
	if( ! $access_secret_key ) {
		printerror "File('$privatekeyfile') must contain the AWS secret access key";
	}
	return;
}

sub start
{
	# -a <accesskeyfile> -s <secretkeyfile> -id <AMI-id> [ -key <loginkeypair> -group <groupname> -userdata <base64 encoded data> -instancetype <m1.small/m1.large/m1.xlarge>  ]
	# On success, output will include "instanceID"
	# On failure, output will include "error code".
	#
	printverbose "start is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $amiid = undef;
	my $loginkeypair = undef;
	my $userdata = undef;
	my $instancetype = undef;
	my @groupname = ();

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'id=s' => \$amiid, 
			'id=s' => \$amiid, 
			'key=s' => \$loginkeypair, 
			'userdata=s' => \$userdata,
			'instancetype=s' => \$instancetype,
			'group=s@' => \@groupname )) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $amiid ) {
		printerror "You need to specify the AMI ID you want to start an instance of"; 
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $ec2 = Net::Amazon::EC2->new(
		AWSAccessKeyId => $access_public_key,
		SecretAccessKey => $access_secret_key,
		# debug => 1
	);

	if( ! defined($ec2) ) {
		printerror "Cannot allocate a new ec2 handler"; 
	}

	# Start a new instance from AMI with given ami id

	# create parameters
	my %input_params;
	$input_params{"ImageId"} = $amiid;
	$input_params{"MinCount"} = 1;
	$input_params{"MaxCount"} = 1;
	if( $loginkeypair ) {
		$input_params{"KeyName"} = $loginkeypair;
	}
	if( @groupname ) {
		$input_params{"SecurityGroup"} = [ @groupname ];
	}
	if( $userdata ) {
		$input_params{"UserData"} = $userdata;
	}
	if( $instancetype ) {
		$input_params{"instanceType"} = $instancetype;
		# m1.small 
		#	1 EC2 Compute Unit (1 virtual core with 1 EC2 Compute Unit). 32-bit, 1.7GB RAM, 160GB disk
		# m1.large
		# 	4 EC2 Compute Units (2 virtual core with 2 EC2 Compute Units each). 64-bit, 7.5GB RAM, 850GB disk
		# m1.xlarge
		# 	8 EC2 Compute Units (4 virtual core with 2 EC2 Compute Units each). 64-bit, 15GB RAM, 1690GB dis
	}

	my $instance = $ec2->run_instances( %input_params );
	checkEC2Error($ec2, 1, $instance);

	# Successfully created a new instance 
	createSuccessOutput( $instance->instances_set->[0]->instance_id );

	printSuccessOutput();
	printverbose "succeeded to start AMI($amiid)";
	return;
}

sub stop
{
	# -a <accesskeyfile> -s <secretkeyfile> -id <instance-id>
	# On success, output will include nothing.
	# On failure, output will include "error code".
	printverbose "stop is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $instanceid = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'id=s' => \$instanceid )) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $instanceid ) {
		printerror "You need to specify the instance ID you want to stop"; 
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $ec2 = Net::Amazon::EC2->new(
		AWSAccessKeyId => $access_public_key,
		SecretAccessKey => $access_secret_key,
		# debug => 1
	);

	if( ! defined($ec2) ) {
		printerror "Cannot allocate a new ec2 handler"; 
	}

	# Stop a running instance with given instance id

	# create parameters
	my %input_params;
	$input_params{"InstanceId"} = $instanceid;

	my $instance = $ec2->terminate_instances( %input_params );
	checkEC2Error($ec2, 1, $instance);

	printSuccessOutput();
	printverbose "succeeded to stop instance($instanceid)";
	return;
}

sub reboot
{
	# -a <accesskeyfile> -s <secretkeyfile> -id <instance-id>
	# On success, output will include nothing.
	# On failure, output will include "error code".
	printverbose "reboot is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $instanceid = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'id=s' => \$instanceid )) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $instanceid ) {
		printerror "You need to specify the instance ID you want to reboot"; 
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $ec2 = Net::Amazon::EC2->new(
		AWSAccessKeyId => $access_public_key,
		SecretAccessKey => $access_secret_key,
		# debug => 1
	);

	if( ! defined($ec2) ) {
		printerror "Cannot allocate a new ec2 handler"; 
	}

	# reboot a running instance with given instance id

	# create parameters
	my %input_params;
	$input_params{"InstanceId"} = $instanceid;

	my $instance = $ec2->reboot_instances( %input_params );
	checkEC2Error($ec2, 1, $instance);
	
	printSuccessOutput();
	printverbose "succeeded to reboot instance($instanceid)";
	return; 
}

sub status
{
	# -a <accesskeyfile> -s <secretkeyfile> [ -instanceid <instance-id> -amiid <ami-id> -group <groupname> ]
	# 
	# On success, output will have one line for each running instance.
	#
	# The one line format in an output file should be like 
	# "INSTANCEID=i-21789a48,AMIID=ami-2bb65342,STATUS=running,PRIVATEDNS=domU-12-31-36-00-2D-63.z-1.compute-1.internal,PUBLICDNS=ec2-72-44-51-9.z-1.compute-1.amazonaws.com,KEYNAME=gsg-keypair,GROUP=mytestgroup,GROUP=default,GROUP=mygroup"
	#
	# STATUS must be one of "pending", "running", "shutting-down", "terminated"
	# PRIVATEDNS and PUBLICDNS may be empty until the instance enters a running state
	# It is possible that there are multiple GROUP names.
	#
	# On failure, output will include "error code".
	
	printverbose "status is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $instanceid = undef;
	my $amiid = undef;
	my $groupname = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'instanceid=s' => \$instanceid, 
			'amiid=s' => \$amiid, 
			'group=s' => \$groupname )) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $ec2 = Net::Amazon::EC2->new(
		AWSAccessKeyId => $access_public_key,
		SecretAccessKey => $access_secret_key,
		# debug => 1
	);

	if( ! defined($ec2) ) {
		printerror "Cannot allocate a new ec2 handler"; 
	}

	# describe running instance(s)
	my $running_instances = $ec2->describe_instances();
	checkEC2Error($ec2, 0, $running_instances);

	#if( $instanceid ) {
	#	my %input_params;
	#	$input_params{"InstanceId"} = $instanceid;
	#	printverbose "instanceid = $instanceid\n";
	#	$running_instances = $ec2->describe_instances( %input_params );
	#}else {
	#	$running_instances = $ec2->describe_instances();
	#}

	if( ! defined($running_instances) ) {
		# It means there is no running VM.
		printSuccessOutput();
		printverbose "There is no running instance";
		return;
	}

	# "INSTANCEID,AMIID,STATUS,PRIVATEDNS,PUBLICDNS,KEYNAME,GROUP"
	foreach my $reservation (@$running_instances) { 
		my $instance = $reservation->instances_set->[0];

		my $result_instanceid = $instance->instance_id;
		my $result_amiid = $instance->image_id;
		my $result_status = $instance->instance_state->name;

		my $result_privatedns = undef;
		my $result_publicdns = undef;
		my $result_keyname = undef;
		my @result_groups = ();

		# if amiid is given, 
		# only instance having the amiid will be described.
		if( $amiid ) {
			if( $amiid ne $result_amiid ) {
				next;
			}
		}

		# if instanceid is given
		if( $instanceid ) {
			if( $instanceid ne $result_instanceid ) {
				next;
			}
		}

		if( $result_status eq "running" ) {
			my $result_dns_name = $instance->dns_name;

			if( $instance->private_dns_name ) {
				$result_privatedns = $instance->private_dns_name;
			}
			if( $instance->dns_name ) {
				$result_publicdns = $instance->dns_name;
			}
		}

		if( $instance->key_name ) {
			$result_keyname = $instance->key_name;
		}

		if( $reservation->group_set ) {
			@result_groups = $reservation->group_set;
		}

		my $result_string = "INSTANCEID=$result_instanceid,AMIID=$result_amiid,STATUS=$result_status";

		if( $result_privatedns ) {
			$result_string .= ",PRIVATEDNS=$result_privatedns";
		}
		if( $result_publicdns ) {
			$result_string .= ",PUBLICDNS=$result_publicdns";
		}
		if( $result_keyname ) {
			$result_string .= ",KEYNAME=$result_keyname";
		}

		my $group_match = 0;
		if( @result_groups ) {
			foreach my $one_group ( @result_groups ) {
				my $group_id = $one_group->group_id;

				if( $groupname ) {
					if( $groupname eq $group_id ) {
						$group_match = 1;
					}
				}
				$result_string .= ",GROUP=$group_id";
			}
		}

		# if group name is given, 
		# only instance having the group will be described.
		if( $groupname && @result_groups ) {
			if( $group_match ) {
				createSuccessOutput( $result_string );
			}
		}else {
			createSuccessOutput( $result_string );
		}
	}

	printSuccessOutput();
	printverbose "succeeded to describe running instance(s)";
	return;
}

sub images
{
	# -a <accesskeyfile> -s <secretkeyfile> [ -id <AMI-id> -owner <owner> -owner <anotherowner> -location <location on S3> ]
	# 
	# On success, output will have one line for each AMI.
	# The format of one line is AMIID<tab>LOCATION<tab>OWNERID
	# e.g.)ami-2bb65342	ec2-public-images/demo-paid-AMI.manifest.xml	amazon
	#
	# On failure, output will include "error code".

	printverbose "images is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $amiid = undef;
	my $location = undef;
	my @owners = ();
	my @executableby = ();

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'id=s' => \$amiid, 
			'owner=s@' => \@owners, 
			'executableby=s@' => \@executableby, 
			'location=s' => \$location )) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $ec2 = Net::Amazon::EC2->new(
		AWSAccessKeyId => $access_public_key,
		SecretAccessKey => $access_secret_key,
		# debug => 1
	);

	if( ! defined($ec2) ) {
		printerror "Cannot allocate a new ec2 handler"; 
	}

	# create parameters
	my %input_params;
	if( @owners ) {
		$input_params{"Owner"} = [ @owners ];
		#$input_params{"Owner"} = [ "amazon", "self" ];
	}

	if( @executableby ) {
		$input_params{"ExecutableBy"} = [ @executableby ];
		#$input_params{"ExecutableBy"} = [ "all" ];
	}

	if( $amiid ) {
		$input_params{"ImageId"} = $amiid;
	}

	# describe AMIs
	my $ami_images = $ec2->describe_images(%input_params);
	checkEC2Error($ec2, 0, $ami_images);

	if( ! defined($ami_images) ) {
		# It means there is no AMI
		printSuccessOutput();
		printverbose "There is no AMI";
		return;
	}

	foreach my $image ( @$ami_images ) { 
		my $result_imagestate = $image->image_state;

		# We are only interested in "available" images.
		# We will skip already "deregistered" images.
		if( $result_imagestate eq "available" ) {
			my $result_amiid = $image->image_id;
			my $result_ownerid = $image->image_owner_id;
			my $result_location = $image->image_location;

			# if location is given,
			# only AMI for the location will be described
			if( $location ) {
				if( $location ne $result_location ) {
					next;
				}
			}

			my $result_string = "$result_amiid\t$result_location\t$result_ownerid";
			createSuccessOutput( $result_string );
		}
	}

	printSuccessOutput();
	printverbose "succeeded to describe AMIs";
	return;
}

sub registerimage
{
	# -a <accesskeyfile> -s <secretkeyfile> -location <location on S3>
	# 
	# On success, output will include "AMI-ID"
	# On failure, output will include "error code".
	printverbose "registerimage is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $location = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'location=s' => \$location )) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $location ) {
		printerror "You need to specify the location of the AMI manifest on S3";
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $ec2 = Net::Amazon::EC2->new(
		AWSAccessKeyId => $access_public_key,
		SecretAccessKey => $access_secret_key,
		# debug => 1
	);

	if( ! defined($ec2) ) {
		printerror "Cannot allocate a new ec2 handler"; 
	}

	my %input_params;
	$input_params{"ImageLocation"} = $location;

	my $ami_image = $ec2->register_image(%input_params);
	checkEC2Error($ec2, 1, $ami_image);

	# Successfully registered AMI on S3 to EC2
	createSuccessOutput( $ami_image);

	printSuccessOutput();
	printverbose "succeeded to register AMI($location) to EC2";
	return;
}

sub deregisterimage
{
	# -a <accesskeyfile> -s <secretkeyfile> -id <ami-id>
	# 
	# On success, output will include nothing.
	# On failure, output will include "error code".
	printverbose "deregisterimage is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $amiid = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'id=s' => \$amiid )) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $amiid ) {
		printerror "You need to specify the image id of the AMI you want to deregister.";
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $ec2 = Net::Amazon::EC2->new(
		AWSAccessKeyId => $access_public_key,
		SecretAccessKey => $access_secret_key,
		# debug => 1
	);

	if( ! defined($ec2) ) {
		printerror "Cannot allocate a new ec2 handler"; 
	}

	# create parameters
	my %input_params;
	$input_params{"ImageId"} = $amiid;

	my $retval = $ec2->deregister_image(%input_params);
	checkEC2Error($ec2, 1, $retval);

	printSuccessOutput();
	printverbose "succeeded to deregister AMI($amiid) from EC2";
	return;
}

sub creategroup
{
	# -a <accesskeyfile> -s <secretkeyfile> -group <groupname> -desc <groupDescription>
	# 
	# On success, output will include nothing.
	# On failure, output will include "error code".

	printverbose "creategroup is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $groupname = undef;
	my $groupdesc = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'group=s' => \$groupname, 
			'desc=s' => \$groupdesc )) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $groupname ) {
		printerror "You need to specify the name of the new group to create.";
	}
	if( ! $groupdesc ) {
		printerror "You need to specify a short description of the new group.";
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $ec2 = Net::Amazon::EC2->new(
		AWSAccessKeyId => $access_public_key,
		SecretAccessKey => $access_secret_key,
		# debug => 1
	);

	if( ! defined($ec2) ) {
		printerror "Cannot allocate a new ec2 handler"; 
	}

	# create parameters
	my %input_params;
	$input_params{"GroupName"} = $groupname;
	$input_params{"GroupDescription"} = $groupdesc;

	# Create a new group
	my $retval = $ec2->create_security_group( %input_params );
	checkEC2Error($ec2, 1, $retval);

	printSuccessOutput();
	printverbose "succeeded to create a new group($groupname)";
	return;
}

sub groupnames
{
	# -a <accesskeyfile> -s <secretkeyfile>
	# 
	# On success, output will have one line for each group name.
	# The format of one line is <groupname><tab><group description>
	#
	# On failure, output will include "error code".

	printverbose "groupnames is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile )) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	# Amazon::EC2 0.06 version has a bug of listing groupnames
	# So we use old api for this command
	my $ec2 = Net::Amazon::EC2->new(
		AWSAccessKeyId => $access_public_key,
		SecretAccessKey => $access_secret_key,
		use_old_api => 1,
		#debug => 1
	);

	if( ! defined($ec2) ) {
		printerror "Cannot allocate a new ec2 handler"; 
	}

	# list all group names
	my $ami_groups = $ec2->describe_security_groups();

	# New API doesn't work well for group without any ip rule
	#checkEC2Error($ec2, 0, $ami_groups);
	#
	#if( ! defined($ami_groups) ) {
	#	# It means there is no group name
	#	printSuccessOutput();
	#	printverbose "There is no group";
	#	return;
	#}

	#foreach my $group ( @$ami_groups ) { 
	#	#<groupname><tab><group description>
	#	my $group_name = $group->group_name;
	#	my $group_desc = $group->group_description;
	#	my $result_string = "$group_name\t$group_desc";
	#	createSuccessOutput( $result_string );
	#}

	# Old version API
	if( ! defined($ami_groups) ) {
		if( defined($ec2->{error}) ) {
			# It means that error happened
			createErrorOutput( $ec2->{error}, $ec2->{errorcode} );
		}else {
			# It means there is no group name
			printSuccessOutput();
			printverbose "There is no group";
			return;
		}
	}

	foreach my $group ( @{$ami_groups} ) {
		#<groupname><tab><group description>
		my $result_string = "$group->{groupName}\t$group->{groupDescription}";
		createSuccessOutput( $result_string );
	}

	printSuccessOutput();
	printverbose "succeeded to describe group names";
	return;
}

sub delgroup
{
	# -a <accesskeyfile> -s <secretkeyfile> -group <groupname>
	# 
	# On success, output will include nothing.
	# On failure, output will include "error code".

	printverbose "delgroup is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $groupname = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'group=s' => \$groupname )) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $groupname ) {
		printerror "You need to specify the name of the new group to create.";
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $ec2 = Net::Amazon::EC2->new(
		AWSAccessKeyId => $access_public_key,
		SecretAccessKey => $access_secret_key,
		# debug => 1
	);

	if( ! defined($ec2) ) {
		printerror "Cannot allocate a new ec2 handler"; 
	}

	# create parameters
	my %input_params;
	$input_params{"GroupName"} = $groupname;

	# delete a given group
	my $retval = $ec2->delete_security_group( %input_params );
	checkEC2Error($ec2, 1, $retval);

	printSuccessOutput();
	printverbose "succeeded to delete group($groupname)";
	return;
}

sub grouprules
{
	# -a <accesskeyfile> -s <secretkeyfile> -group <groupname>
	# 
	# On success, output will have one line for each security rule for the group.
	# The format of one line is <sourceip><tab><protocol><tab><fromport><toport>
	# e.g.)0.0.0.0/0	tcp	1	1024
	#
	# On failure, output will include "error code".

	printverbose "grouprules is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $groupname = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'group=s' => \$groupname )) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $groupname ) {
		printerror "You need to specify the name of group to describe.";
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	# Amazon::EC2 0.06 version has a bug of listing groupnames
	# So we use old api for this command
	my $ec2 = Net::Amazon::EC2->new(
		AWSAccessKeyId => $access_public_key,
		SecretAccessKey => $access_secret_key,
		use_old_api => 1,
		# debug => 1
	);

	if( ! defined($ec2) ) {
		printerror "Cannot allocate a new ec2 handler"; 
	}

	# create parameters
	my %input_params;
	$input_params{"GroupName"} = $groupname;

	# list all ip security rules for the group
	my $group_rules = $ec2->describe_security_groups(%input_params);

	# New API doesn't work well for group without any ip rule
	#checkEC2Error($ec2, 1, $group_rules);
	#
	#foreach my $group ( @$group_rules ) {
	#	if( ! $group->ip_permissions ) {
	#		next;
	#	}
	#
	#	foreach my $iprule ( $group->ip_permissions ) {
	#		if( ! $iprule->ip_ranges ) {
	#			# If a rule is not for CIDR, we will just skip it.
	#			next;
	#		}
	#
	#		my $ip_protocol = $iprule->ip_protocol;
	#		my $from_port = $iprule->from_port;
	#		my $to_port = $iprule->to_port;
	#
	#		foreach my $cidrip ( $iprule->ip_ranges ) {
	#			if( $cidrip->cidr_ip ) {
	#				my $cidr_ip = $cidrip->cidr_ip;
	#
	#				my $result_string = "$cidr_ip";
	#				$result_string .= "\t";
	#				$result_string .= "$ip_protocol";
	#				$result_string .= "\t";
	#				$result_string .= "$from_port";
	#				$result_string .= "\t";
	#				$result_string .= "$to_port";
	#
	#				createSuccessOutput($result_string);
	#			}
	#		}
	#	}
	#}
	
	# Old version API
	if( ! defined($group_rules) ) {
		createErrorOutput( $ec2->{error}, $ec2->{errorcode} );
	}

	foreach my $group ( @{$group_rules} ) {
		if( ! exists $group->{ipPermissions}{item} ) {
			next;
		}
	
		foreach my $iprule ( @{$group->{ipPermissions}{item}} ) {
			if( ! exists $iprule->{ipRanges}{item} ) {
				# If a rule is not for CIDR, we will just skip it.
				next;
			}

			foreach my $cidrip ( @{$iprule->{ipRanges}{item}} ) {
				if( exists $cidrip->{cidrIp} ) {
					my $result_string = "$cidrip->{cidrIp}";
					$result_string .= "\t";
					$result_string .= "$iprule->{ipProtocol}";
					$result_string .= "\t";
					$result_string .= "$iprule->{fromPort}";
					$result_string .= "\t";
					$result_string .= "$iprule->{toPort}";

					createSuccessOutput($result_string);
				}
			}

			#my @test = keys %{ $iprule->{groups} };
			#if( exists $iprule->{groups}{item} ) {
			#	$iprule->{groups}{item}[0]{userId};
			#	$iprule->{groups}{item}[0]{groupName};
			#}
		
		}
	}
	

	printSuccessOutput();
	printverbose "succeeded to describe ip security rules for group($groupname).";
	return;
}

sub setgroupingress
{
	# -a <accesskeyfile> -s <secretkeyfile> -group <groupname> -protocol <protocol> -fromport <fromport> -toport <toport> [ -iprange <iprange> ]
	# 
	# On success, output will include nothing.
	# On failure, output will include "error code".

	printverbose "setgroupingress is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $groupname = undef;
	my $iprange = "0.0.0.0/0";
	my $protocol = undef;
	my $fromport = undef;
	my $toport = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'group=s' => \$groupname, 
			'iprange=s' => \$iprange, 
			'protocol=s' => \$protocol, 
			'fromport=s' => \$fromport, 
			'toport=s' => \$toport )) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $groupname ) {
		printerror "You need to specify the name of the group to add a security rule to.";
	}
	if( ! $iprange ) {
		printerror "You need to specify the source IP range(e.g. 0.0.0.0/0).";
	}
	if( ! $protocol ) {
		printerror "You need to specify IP protocol(tcp,udp,or icmp).";
	}
	$protocol = lc($protocol);

	if( $protocol eq "icmp" ) {
		$fromport = -1;
		$toport = -1;
	}
	if( ! defined($fromport) ) {
		printerror "You need to specify the beginning of port range to add access for.";
	}
	if( ! defined($toport) ) {
		printerror "You need to specify the end of port range to add access for.";
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $ec2 = Net::Amazon::EC2->new(
		AWSAccessKeyId => $access_public_key,
		SecretAccessKey => $access_secret_key,
		# debug => 1
	);

	if( ! defined($ec2) ) {
		printerror "Cannot allocate a new ec2 handler"; 
	}

	# create parameters
	my %input_params;
	$input_params{"GroupName"} = $groupname;
	$input_params{"CidrIp"} = $iprange;
	$input_params{"IpProtocol"} = $protocol;
	$input_params{"FromPort"} = $fromport;
	$input_params{"ToPort"} = $toport;

	# add ip security rule to the group
	my $retval = $ec2->authorize_security_group_ingress(%input_params);
	checkEC2Error($ec2, 1, $retval);
	
	printSuccessOutput();
	printverbose "succeeded to add a ip security rule for group($groupname)";
	return;
}

sub delgroupingress
{
	# -a <accesskeyfile> -s <secretkeyfile> -group <groupname> -protocol <protocol> -fromport <fromport> -toport <toport> [ -iprange <iprange> ]
	# 
	# On success, output will include nothing.
	# On failure, output will include "error code".

	printverbose "delgroupingress is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $groupname = undef;
	my $iprange = "0.0.0.0/0";
	my $protocol = undef;
	my $fromport = undef;
	my $toport = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'group=s' => \$groupname, 
			'iprange=s' => \$iprange, 
			'protocol=s' => \$protocol, 
			'fromport=s' => \$fromport, 
			'toport=s' => \$toport )) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $groupname ) {
		printerror "You need to specify the name of the group to delete a security rule from.";
	}
	if( ! $iprange ) {
		printerror "You need to specify the source IP range(e.g. 0.0.0.0/0).";
	}
	if( ! $protocol ) {
		printerror "You need to specify IP protocol(tcp,udp,or icmp).";
	}
	$protocol = lc($protocol);

	if( $protocol eq "icmp" ) {
		$fromport = -1;
		$toport = -1;
	}
	if( ! defined($fromport) ) {
		printerror "You need to specify the beginning of port range in the rule to delete."
	}
	if( ! defined($toport) ) {
		printerror "You need to specify the end of port range in the rule to delete";
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $ec2 = Net::Amazon::EC2->new(
		AWSAccessKeyId => $access_public_key,
		SecretAccessKey => $access_secret_key,
		# debug => 1
	);

	if( ! defined($ec2) ) {
		printerror "Cannot allocate a new ec2 handler"; 
	}

	# create parameters
	my %input_params;
	$input_params{"GroupName"} = $groupname;
	$input_params{"CidrIp"} = $iprange;
	$input_params{"IpProtocol"} = $protocol;
	$input_params{"FromPort"} = $fromport;
	$input_params{"ToPort"} = $toport;

	# add ip security rule to the group
	my $retval = $ec2->revoke_security_group_ingress(%input_params);
	checkEC2Error($ec2, 1, $retval);
	
	printSuccessOutput();
	printverbose "succeeded to delete a ip security rule for group($groupname)";
	return;
}

sub loginkeynames
{
	# -a <accesskeyfile> -s <secretkeyfile>
	#
	# On success, output will have one line for each login key.
	# The format of one line is <keyname><tab><key fingerprint>
	#
	# On failure, output will include "error code"
	
	printverbose "loginkeynames is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile )) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $ec2 = Net::Amazon::EC2->new(
		AWSAccessKeyId => $access_public_key,
		SecretAccessKey => $access_secret_key,
		# debug => 1
	);

	if( ! defined($ec2) ) {
		printerror "Cannot allocate a new ec2 handler"; 
	}

	# list all login keys
	my $login_keys = $ec2->describe_key_pairs();
	checkEC2Error($ec2, 0, $login_keys);

	if( ! defined($login_keys) ) {
		# It means there is no login key
		printSuccessOutput();
		printverbose "There is no login key pair";
		return;
	}

	foreach my $login_key ( @$login_keys ) { 
		#<keyname><tab><key fingerprint>
		my $key_name = $login_key->key_name;
		my $key_fingerprint = $login_key->key_fingerprint;
		my $result_string = "$key_name\t$key_fingerprint";
		createSuccessOutput( $result_string );
	}

	printSuccessOutput();
	printverbose "succeeded to describe login keys";
	return;
}

sub createloginkey
{
	# -a <accesskeyfile> -s <secretkeyfile> -keyname <keyname> -output <outputfile>
	# 
	# On success, outputfile will have an unencrypted PEM encoded RSA private key.
	# 
	# On failure, outputfile will include nothing. Error string will be printed at the screen.

	printverbose "createloginkey is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $keyname = undef;
	my $outputfile = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'keyname=s' => \$keyname, 
			'output=s' => \$outputfile ) ) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $keyname ) {
		printerror "You need to specify a name for this key.";
	}
	if( ! $outputfile ) {
		printerror "You need to specify the output file into which result private key will be saved.";
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $ec2 = Net::Amazon::EC2->new(
		AWSAccessKeyId => $access_public_key,
		SecretAccessKey => $access_secret_key,
		# debug => 1
	);

	if( ! defined($ec2) ) {
		printerror "Cannot allocate a new ec2 handler"; 
	}

	# create parameters
	my %input_params;
	$input_params{"KeyName"} = $keyname;

	# create login key pair
	my $keypair = $ec2->create_key_pair(%input_params);
	checkEC2Error($ec2, 1, $keypair);

	my $key_material = $keypair->key_material;	
	open KEYOUTPUT, "> $outputfile"
		or printerror "Cannot create the outputfile('$outputfile') : $!";
	print KEYOUTPUT "$key_material";
	close KEYOUTPUT;

	# For security, the key output file will be set to be only readable by owner.
	chmod 0600, $outputfile;

	printSuccessOutput();
	printverbose "succeeded to create a login key($keyname)";

	return;
}

sub deleteloginkey
{
	# -a <accesskeyfile> -s <secretkeyfile> -keyname <keyname>
	# 
	# On success, output will include nothing
	# On failure, output will include "error code".

	printverbose "deleteloginkey is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $keyname = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'keyname=s' => \$keyname)) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $keyname ) {
		printerror "You need to specify a name for this key.";
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $ec2 = Net::Amazon::EC2->new(
		AWSAccessKeyId => $access_public_key,
		SecretAccessKey => $access_secret_key,
		# debug => 1
	);

	if( ! defined($ec2) ) {
		printerror "Cannot allocate a new ec2 handler"; 
	}

	# create parameters
	my %input_params;
	$input_params{"KeyName"} = $keyname;

	# delete login key pair
	my $retval = $ec2->delete_key_pair(%input_params);
	checkEC2Error($ec2, 1, $retval);
	
	printSuccessOutput();
	printverbose "succeeded to delete a login key($keyname)";

	return;
}

sub showlaunchpermission
{
	# -a <accesskeyfile> -s <secretkeyfile> -id <ami-id>
	# 
	# On success, output will have one line like this
	# 	<user_id><tab><user_id>
	#
	# On failure, output will include "error code".

	printverbose "showlaunchpermission is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $amiid = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'id=s' => \$amiid )) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $amiid ) {
		printerror "You need to specify the image id of the AMI that you want to describe attributes for.";
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $ec2 = Net::Amazon::EC2->new(
		AWSAccessKeyId => $access_public_key,
		SecretAccessKey => $access_secret_key,
		# debug => 1
	);

	if( ! defined($ec2) ) {
		printerror "Cannot allocate a new ec2 handler"; 
	}

	# create parameters
	my %input_params;
	$input_params{"ImageId"} = $amiid;
	$input_params{"Attribute"} = "launchPermission";

	my $attributes = $ec2->describe_image_attribute(%input_params);
	checkEC2Error($ec2, 1, $attributes);

	if( $attributes->launch_permissions ) {
		my $result_string = undef;
		foreach my $permission ( $attributes->launch_permissions ) {
			if( $permission->user_id ) {
				my $user_id = $permission->user_id;
				if( defined( $result_string ) ) {
					$result_string .= "\t";
				}
				$result_string .= "$user_id";
			}
		}
		createSuccessOutput($result_string);
	}

	printSuccessOutput();
	printverbose "succeeded to describe image attributes for AMI($amiid)";

	return;
}

sub addlaunchpermission
{
	# -a <accesskeyfile> -s <secretkeyfile> -id <ami-id> -userid <userid>
	# 
	# On success, output will include nothing
	# On failure, output will include "error code".

	printverbose "addlaunchpermission is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $amiid = undef;
	my $userid = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'id=s' => \$amiid,
			'userid=s' => \$userid )) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $amiid ) {
		printerror "You need to specify the image id of the AMI that you want to modify launch permission.";
	}
	if( ! $userid ) {
		printerror "You need to specify the user id that you want to add launch permission to.";
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $ec2 = Net::Amazon::EC2->new(
		AWSAccessKeyId => $access_public_key,
		SecretAccessKey => $access_secret_key,
		# debug => 1
	);

	if( ! defined($ec2) ) {
		printerror "Cannot allocate a new ec2 handler"; 
	}

	# create parameters
	my %input_params;
	$input_params{"ImageId"} = $amiid;
	$input_params{"Attribute"} = "launchPermission";
	$input_params{"OperationType"} = "add";
	$input_params{"UserId"} = $userid;
	$input_params{"UserGroup"} = "all";

	my $retval = $ec2->modify_image_attribute(%input_params);
	checkEC2Error($ec2, 1, $retval);

	printSuccessOutput();
	printverbose "succeeded to add launch permission to user($userid) for AMI($amiid)";

	return;
}

sub removelaunchpermission
{
	# -a <accesskeyfile> -s <secretkeyfile> -id <ami-id> -userid <userid>
	# 
	# On success, output will include nothing
	# On failure, output will include "error code".

	printverbose "removelaunchpermission is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $amiid = undef;
	my $userid = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'id=s' => \$amiid,
			'userid=s' => \$userid )) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $amiid ) {
		printerror "You need to specify the image id of the AMI that you want to modify launch permission.";
	}
	if( ! $userid ) {
		printerror "You need to specify the user id that you want to remove launch permission from.";
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $ec2 = Net::Amazon::EC2->new(
		AWSAccessKeyId => $access_public_key,
		SecretAccessKey => $access_secret_key,
		# debug => 1
	);

	if( ! defined($ec2) ) {
		printerror "Cannot allocate a new ec2 handler"; 
	}

	# create parameters
	my %input_params;
	$input_params{"ImageId"} = $amiid;
	$input_params{"Attribute"} = "launchPermission";
	$input_params{"OperationType"} = "remove";
	$input_params{"UserId"} = $userid;
	$input_params{"UserGroup"} = "all";

	my $retval = $ec2->modify_image_attribute(%input_params);
	checkEC2Error($ec2, 1, $retval);

	printSuccessOutput();
	printverbose "succeeded to remove launch permission from user($userid) for AMI($amiid)";

	return;
}

sub resetlaunchpermission
{
	# -a <accesskeyfile> -s <secretkeyfile> -id <ami-id>
	# 
	# On success, output will include nothing
	# On failure, output will include "error code".

	printverbose "resetlaunchpermission is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $amiid = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'id=s' => \$amiid )) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $amiid ) {
		printerror "You need to specify the image id of the AMI that you want to reset launch permission.";
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $ec2 = Net::Amazon::EC2->new(
		AWSAccessKeyId => $access_public_key,
		SecretAccessKey => $access_secret_key,
		# debug => 1
	);

	if( ! defined($ec2) ) {
		printerror "Cannot allocate a new ec2 handler"; 
	}

	# create parameters
	my %input_params;
	$input_params{"ImageId"} = $amiid;
	$input_params{"Attribute"} = "launchPermission";

	my $retval = $ec2->reset_image_attribute(%input_params);
	checkEC2Error($ec2, 1, $retval);

	printSuccessOutput();
	printverbose "succeeded to reset launch permission for AMI($amiid)";

	return;
}

#########################
###### S3 Commands ######
#########################

sub listallbuckets
{
	# -a <accesskeyfile> -s <secretkeyfile>
	# 
	# On success, output will have one line for each bucket name
	# On failure, output will include "error code".

	printverbose "listallbuckets is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile)) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $s3 = Net::Amazon::S3->new( 
		{	aws_access_key_id		=> $access_public_key,
			aws_secret_access_key	=> $access_secret_key,
			retry			=> 1,
			# secure		=> 1,
			# timeout		=> 30,
		}
	);

	if( ! defined($s3) ) {
		printerror "Cannot allocate a new s3 handler"; 
	}

	# list all buckets that I own
	my $response = $s3->buckets;

	if( ! defined($response) ) {
		# It means that error happened
		createErrorOutput($s3->errstr, $s3->err);
	}

	foreach my $bucket ( @{ $response->{buckets} } ) {
		createSuccessOutput( $bucket->bucket );
	}

	printSuccessOutput();
	printverbose "succeeded to list all bucket names";
	return;
}

sub uploadfile
{
	# -a <accesskeyfile> -s <secretkeyfile> -file <filename> -bucket <bucketname> [ -keyname <keyname> -mimetype [mimetype] -ec2 ]
	# On success, output will include nothing.
	# On failure, output will include "error code".

	printverbose "uploadfile is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $bucketname = undef;
	my $filename = undef;
	my $keyname = undef;
	my $mimetype = undef;
	my $quietflag = 0;
	my $ec2flag = 0;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'file=s' => \$filename,
			'bucket=s' => \$bucketname,
			'keyname=s' => \$keyname,
			'mimetype=s' => \$mimetype,
			'quiet' => \$quietflag,
			'ec2' => \$ec2flag)) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $bucketname ) {
		printerror "You need to specify the bucket name";
	}
	if( ! $filename ) {
		printerror "You need to specify the file to upload";
	}
	if( ! $keyname ) {
		$keyname = basename($filename);
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $s3 = Net::Amazon::S3->new( 
		{	aws_access_key_id		=> $access_public_key,
			aws_secret_access_key	=> $access_secret_key,
			retry			=> 1,
			# secure		=> 1,
			# timeout		=> 30,
		}
	);

	if( ! defined($s3) ) {
		printerror "Cannot allocate a new s3 handler"; 
	}

	my $bucket = $s3->bucket($bucketname );

	# store a file in the bucket
	if( $mimetype ) {
		$bucket->add_key_filename( $keyname, $filename,
			{ content_type => $mimetype, },
		) or createErrorOutput($s3->errstr, $s3->err);
	}else {
		$bucket->add_key_filename($keyname, $filename) 
			or createErrorOutput($s3->errstr, $s3->err);
	}
	
	if( $ec2flag == 1 ) {
		# Set file ACL to allow EC2 read access
		_set_acl_for_ec2($accessfile,$secretfile,$bucketname,$keyname);
	}

	if( $quietflag == 0 ) {
		printSuccessOutput();
	}
	printverbose "succeeded to put file($filename) with key($keyname) in bucket($bucketname)";

	return;
}

sub uploaddir
{
	# -a <accesskeyfile> -s <secretkeyfile> -dir <directory> -bucket <bucketname> [ -ec2 -fork ]
	# On success, output will include nothing.
	# On failure, output will include "error code".

	printverbose "uploaddir is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $bucketname = undef;
	my $dirname = undef;
	my $ec2flag = 0;
	my $forkflag = 0;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'dir=s' => \$dirname,
			'bucket=s' => \$bucketname,
			'fork' => \$forkflag,
			'ec2' => \$ec2flag)) {
	   	usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $bucketname ) {
		printerror "You need to specify the bucket name";
	}
	if( ! $dirname ) {
		printerror "You need to specify the directory to upload";
	}

	if( $ec2flag == 1 ) {
		# Set bucket ACL to allow EC2 read access
		_set_acl_for_ec2($accessfile,$secretfile,$bucketname);
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);

	opendir DH, $dirname 
		or printerror "Cannot open $dirname: $!";

	my @ORIARGV = @ARGV;
	while(my $onefile = readdir DH) {
		next if $onefile =~ /^\./; # skip over dot files
		next if -d "$dirname/$onefile"; # skip over directories

		@ARGV = ("uploadfile", "-a", "$accessfile", "-s", "$secretfile", "-file", "$dirname/$onefile", "-bucket", "$bucketname", "-keyname", "$onefile", "-ec2", "-quiet");

		if( $forkflag == 1 ) {
			createprocess();
		}else {
			uploadfile();
		}
	}
	@ARGV = @ORIARGV;
	if( $forkflag == 1 ) {
		waitallchilds();
	}

	printSuccessOutput();
	printverbose "succeeded to upload all files in $dirname into bucket($bucketname)";
	return;
}

sub downloadfile
{
	# -a <accesskeyfile> -s <secretkeyfile> -name <keyname> -bucket <bucketname> [ -output <outputfile> ]
	# On success, output will include nothing.
	# On failure, output will include "error code".

	printverbose "downloadfile is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $bucketname = undef;
	my $keyname = undef;
	my $outputfile = undef;
	my $quietflag = 0;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'name=s' => \$keyname,
			'bucket=s' => \$bucketname,
			'output=s' => \$outputfile,
			'quiet' => \$quietflag)) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $keyname ) {
		printerror "You need to specify the keyname of file to download";
	}
	if( ! $bucketname ) {
		printerror "You need to specify the bucket name";
	}
	if( ! $outputfile ) {
		$outputfile = $keyname;
	}
	
	if ( -e $outputfile ) {
		printerror "file($outputfile) already exists"; 
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $s3 = Net::Amazon::S3->new( 
		{	aws_access_key_id		=> $access_public_key,
			aws_secret_access_key	=> $access_secret_key,
			retry			=> 1,
			# secure		=> 1,
			# timeout		=> 30,
		}
	);

	if( ! defined($s3) ) {
		printerror "Cannot allocate a new s3 handler"; 
	}

	my $bucket = $s3->bucket($bucketname );

	$bucket->get_key_filename($keyname, 'GET', $outputfile )
		or createErrorOutput($s3->errstr, $s3->err);

	if( $quietflag == 0 ) {
		printSuccessOutput();
	}
	printverbose "succeeded to get object($keyname) from bucket($bucketname)";

	return;
}

sub downloadbucket
{
	# -a <accesskeyfile> -s <secretkeyfile> -bucket <bucketname> [ -outputdir <outputdir> ]
	# On success, output will include nothing.
	# On failure, output will include "error code".

	printverbose "downloadbucket is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $bucketname = undef;
	my $outputdir = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'bucket=s' => \$bucketname,
			'outputdir=s' => \$outputdir)) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $bucketname ) {
		printerror "You need to specify the bucket name";
	}
	if( ! $outputdir ) {
		$outputdir = cwd();
	}else {
		if( ! -d "$outputdir" ) {
			mkdir("$outputdir") || printerror "Cannot mkdir '$outputdir'";
		}
	}
	
	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $s3 = Net::Amazon::S3->new( 
		{	aws_access_key_id		=> $access_public_key,
			aws_secret_access_key	=> $access_secret_key,
			retry			=> 1,
			# secure		=> 1,
			# timeout		=> 30,
		}
	);

	if( ! defined($s3) ) {
		printerror "Cannot allocate a new s3 handler"; 
	}

	my $bucket = $s3->bucket($bucketname );

	my $response = $bucket->list_all
		or createErrorOutput($s3->errstr, $s3->err);

	my @ORIARGV = @ARGV;
	foreach my $key ( @{ $response->{keys} } ) {
		my $key_name = $key->{key};
		my $key_size = $key->{size};
		@ARGV = ("downloadfile", "-a", "$accessfile", "-s", "$secretfile", "-name", "$key_name", "-bucket", "$bucketname", "-output", "$outputdir/$key_name", "-quiet");
		downloadfile();
		#createprocess();
	}
	@ARGV = @ORIARGV;
	#waitallchilds();

	printSuccessOutput();
	printverbose "succeeded to get all objects from bucket($bucketname)";

	return;
}

sub deletefile
{
	# -a <accesskeyfile> -s <secretkeyfile> -name <keyname> -bucket <bucketname>
	# On success, output will include nothing.
	# On failure, output will include "error code".

	printverbose "deletefile is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $bucketname = undef;
	my $keyname = undef;
	my $quietflag = 0;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'name=s' => \$keyname,
			'bucket=s' => \$bucketname,
			'quiet' => \$quietflag)) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $keyname ) {
		printerror "You need to specify the keyname of file to download";
	}
	if( ! $bucketname ) {
		printerror "You need to specify the bucket name";
	}
	
	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $s3 = Net::Amazon::S3->new( 
		{	aws_access_key_id		=> $access_public_key,
			aws_secret_access_key	=> $access_secret_key,
			retry			=> 1,
			# secure		=> 1,
			# timeout		=> 30,
		}
	);

	if( ! defined($s3) ) {
		printerror "Cannot allocate a new s3 handler"; 
	}

	my $bucket = $s3->bucket($bucketname );

	$bucket->delete_key($keyname) 
		or createErrorOutput($s3->errstr, $s3->err);

	if( $quietflag == 0 ) {
		printSuccessOutput();
	}
	printverbose "succeeded to delete object($keyname) from bucket($bucketname)";

	return;
}

sub deleteallfilesinbucket
{
	# -a <accesskeyfile> -s <secretkeyfile> -bucket <bucketname>
	# On success, output will include nothing.
	# On failure, output will include "error code".

	printverbose "deleteallfilesinbucket is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $bucketname = undef;
	my $quietflag = 0;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'bucket=s' => \$bucketname,
			'quiet' => \$quietflag)) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $bucketname ) {
		printerror "You need to specify the bucket name to list";
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $s3 = Net::Amazon::S3->new( 
		{	aws_access_key_id		=> $access_public_key,
			aws_secret_access_key	=> $access_secret_key,
			retry			=> 1,
			# secure		=> 1,
			# timeout		=> 30,
		}
	);

	if( ! defined($s3) ) {
		printerror "Cannot allocate a new s3 handler"; 
	}

	my $bucket = $s3->bucket($bucketname );

	# list bucket
	my $response = $bucket->list_all;

	my $hasbucket = 1;
	if( ! $response ) {
		$hasbucket = 0;
		if( $s3->err ne "NoSuchBucket" ) {
			createErrorOutput($s3->errstr, $s3->err);
		}
	}

	if( $hasbucket == 1 ) {
		my @ORIARGV = @ARGV;
		foreach my $key ( @{ $response->{keys} } ) {
			my $key_name = $key->{key};
			my $key_size = $key->{size};
			@ARGV = ("deletefile", "-a", "$accessfile", "-s", "$secretfile", "-name", "$key_name", "-bucket", "$bucketname", "-quiet");
			deletefile();
			#createprocess();
		}
		@ARGV = @ORIARGV;
		#waitallchilds();
	}

	if( $quietflag == 0 ) {
		printSuccessOutput();
	}
	printverbose "succeeded to delete all files in bucket($bucketname)";
	return;
}


sub createbucket
{
	# -a <accesskeyfile> -s <secretkeyfile> -bucket <bucketname>
	# 
	# On success, output will include nothing.
	# On failure, output will include "error code".

	printverbose "createbucket is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $bucketname = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'bucket=s' => \$bucketname)) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $bucketname ) {
		printerror "You need to specify the bucket name to create.";
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $s3 = Net::Amazon::S3->new( 
		{	aws_access_key_id		=> $access_public_key,
			aws_secret_access_key	=> $access_secret_key,
			retry			=> 1,
			# secure		=> 1,
			# timeout		=> 30,
		}
	);

	if( ! defined($s3) ) {
		printerror "Cannot allocate a new s3 handler"; 
	}

	my $bucket = $s3->add_bucket( { bucket => $bucketname } )
		or createErrorOutput($s3->errstr, $s3->err);

	printSuccessOutput();
	printverbose "succeeded to create bucket($bucketname)";
	return;
}

sub listbucket
{
	# -a <accesskeyfile> -s <secretkeyfile> -bucket <bucketname> [ -prefix <prefix> -marker <marker> ]
	# On success, output will have one line for each key in bucket.
	# The format of one line is <key><tab><size>
	# On failure, output will include "error code".

	printverbose "listbucket is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $bucketname = undef;
	my $prefix = undef;
	my $marker = undef; 

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'bucket=s' => \$bucketname,
			'prefix=s' => \$prefix,
			'marker=s' => \$marker)) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $bucketname ) {
		printerror "You need to specify the bucket name to list";
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $s3 = Net::Amazon::S3->new( 
		{	aws_access_key_id		=> $access_public_key,
			aws_secret_access_key	=> $access_secret_key,
			retry			=> 1,
			# secure		=> 1,
			# timeout		=> 30,
		}
	);

	if( ! defined($s3) ) {
		printerror "Cannot allocate a new s3 handler"; 
	}

	my $bucket = $s3->bucket($bucketname );

	# list bucket
	my $response = undef;
	if( $prefix ) {
		$response = $bucket->list( 
			{	bucket => $bucketname, 
				prefix => $prefix,
			}
		);
	}elsif( $marker ) {
		$response = $bucket->list(
			{	bucket => $bucketname, 
				marker => $marker,
			}
		);
	}else {
		$response = $bucket->list_all;
	}

	if( ! $response ) {
		createErrorOutput($s3->errstr, $s3->err);
	}

	foreach my $key ( @{ $response->{keys} } ) {
		my $key_name = $key->{key};
		my $key_size = $key->{size};

		my $result_string = "$key_name\t$key_size";
		createSuccessOutput( $result_string );
	}

	printSuccessOutput();
	printverbose "succeeded to list bucket($bucketname)";
	return;
}

sub deletebucket
{
	# -a <accesskeyfile> -s <secretkeyfile> -bucket <bucketname> [ -force ]
	# 
	# On success, output will include nothing.
	# On failure, output will include "error code".

	printverbose "deletebucket is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $bucketname = undef;
	my $forceflag = 0;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'bucket=s' => \$bucketname,
			'force' => \$forceflag)) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $bucketname ) {
		printerror "You need to specify the bucket name to delete.";
	}

	my $ORIARGV = @ARGV;
	if( $forceflag == 1 ) {
		# First, we will try to delete all files in the bucket
		@ARGV = ("deleteallfilesinbucket", "-a", "$accessfile", "-s", "$secretfile", "-bucket", "$bucketname", "-quiet");
	   	deleteallfilesinbucket();
		@ARGV = $ORIARGV;
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	my $s3 = Net::Amazon::S3->new( 
		{	aws_access_key_id		=> $access_public_key,
			aws_secret_access_key	=> $access_secret_key,
			retry			=> 1,
			# secure		=> 1,
			# timeout		=> 30,
		}
	);

	if( ! defined($s3) ) {
		printerror "Cannot allocate a new s3 handler"; 
	}

	my $bucket = $s3->bucket($bucketname );

	# delete bucket with name
	my $response = $bucket->delete_bucket;

	if( ! $response ) {
		if( $s3->err ne "NoSuchBucket" ) {
			createErrorOutput($s3->errstr, $s3->err);
		}
	}

	printSuccessOutput();
	printverbose "succeeded to delete bucket($bucketname)";
	return;
}

sub setec2acl
{
	# -a <accesskeyfile> -s <secretkeyfile> -bucket <bucketname>
	# On success, output will include nothing.
	# On failure, output will include "error code".

	printverbose "setec2acl is called(@ARGV)";

	my $accessfile = undef;
	my $secretfile = undef;
	my $bucketname = undef;

	if( !GetOptions ('a=s' => \$accessfile, 
			's=s' => \$secretfile,
			'bucket=s' => \$bucketname)) {
		usage();
	}

	if( ! $accessfile ) {
		printerror "You need to specify the account key file.";
	}
	if( ! $secretfile ) {
		printerror "You need to specify the private key file.";
	}
	if( ! $bucketname ) {
		printerror "You need to specify the bucket name to list";
	}

	# Read access key file
	readAccessKey($accessfile, $secretfile);		

	# Set bucket ACL to allow EC2 read access
	_set_acl_for_ec2($accessfile,$secretfile,$bucketname);

	my $s3 = Net::Amazon::S3->new( 
		{	aws_access_key_id		=> $access_public_key,
			aws_secret_access_key	=> $access_secret_key,
			retry			=> 1,
			# secure		=> 1,
			# timeout		=> 30,
		}
	);

	if( ! defined($s3) ) {
		printerror "Cannot allocate a new s3 handler"; 
	}

	# list bucket
	my $bucket = $s3->bucket($bucketname );
	my $response = $bucket->list_all
		or createErrorOutput($s3->errstr, $s3->err);

	foreach my $key ( @{ $response->{keys} } ) {
		my $key_name = $key->{key};
		my $key_size = $key->{size};

		# Set file ACL to allow EC2 read access
		_set_acl_for_ec2($accessfile,$secretfile,$bucketname, $key_name);
	}

	printSuccessOutput();
	printverbose "succeeded to set acl for ec2 to bucket($bucketname)";
	return;
}

if ($#ARGV < 0 || $ARGV[0] eq "--help" ) { usage(); }
#### EC2 command
elsif ($ARGV[0] eq "start") { start(); }
elsif ($ARGV[0] eq "stop") { stop(); }
elsif ($ARGV[0] eq "reboot") { reboot(); }
elsif ($ARGV[0] eq "status") { status(); }
elsif ($ARGV[0] eq "images") { images(); }
elsif ($ARGV[0] eq "registerimage") { registerimage(); }
elsif ($ARGV[0] eq "deregisterimage") { deregisterimage(); }
elsif ($ARGV[0] eq "groupnames") { groupnames(); }
elsif ($ARGV[0] eq "creategroup") { creategroup(); }
elsif ($ARGV[0] eq "delgroup") { delgroup(); }
elsif ($ARGV[0] eq "grouprules") { grouprules(); }
elsif ($ARGV[0] eq "setgroupingress") { setgroupingress(); }
elsif ($ARGV[0] eq "delgroupingress") { delgroupingress(); }
elsif ($ARGV[0] eq "loginkeynames") { loginkeynames(); }
elsif ($ARGV[0] eq "createloginkey") { createloginkey(); }
elsif ($ARGV[0] eq "deleteloginkey") { deleteloginkey(); }
elsif ($ARGV[0] eq "showlaunchpermission") { showlaunchpermission(); }
elsif ($ARGV[0] eq "addlaunchpermission") { addlaunchpermission(); }
elsif ($ARGV[0] eq "removelaunchpermission") { removelaunchpermission(); }
elsif ($ARGV[0] eq "resetlaunchpermission") { resetlaunchpermission(); }

#### S3 command
elsif ($ARGV[0] eq "listallbuckets") { listallbuckets(); }
elsif ($ARGV[0] eq "createbucket") { createbucket(); }
elsif ($ARGV[0] eq "deletebucket") { deletebucket(); }
elsif ($ARGV[0] eq "listbucket") { listbucket(); }
elsif ($ARGV[0] eq "uploadfile") { uploadfile(); }
elsif ($ARGV[0] eq "uploaddir") { uploaddir(); }
elsif ($ARGV[0] eq "downloadfile") { downloadfile(); }
elsif ($ARGV[0] eq "downloadbucket") { downloadbucket(); }
elsif ($ARGV[0] eq "deletefile") { deletefile(); }
elsif ($ARGV[0] eq "deleteallfilesinbucket") { deleteallfilesinbucket(); }
elsif ($ARGV[0] eq "setec2acl") { setec2acl(); }
else { printerror "Unknown command \"$ARGV[0]\". See $progname --help."; }

