#!/usr/bin/perl -w

# $Id: install_minica.pl,v 1.1.2.2 2007/07/18 13:35:08 alderman Exp $

# Collect information from user: 

# The input (provided by the administrator) is details like the
# subject name defaults, the location for the directory structure used
# to store the ('openssl ca' based) CA, and the password for the
# private key.  The script creates the private key, self-signed cert,
# directory structure, and custom openssl.cnf file.

use File::Basename; # for parsing the directory name
use Cwd; # for creating a default directory name 
use Term::ReadKey; # for turning off echo on passwords
use strict;

my($openssl_bin) = find_openssl_bin();
if($openssl_bin eq '') {
    die "Can't find openssl!";
}

# the third arg means, this is a password
my($password) = read_length_input("CA Password", 4, 1);
my($password_chk) = read_length_input("CA Password (again)", 4, 1);
if(not ($password eq $password_chk)) {
    die "Try again: passwords don't match.\n";
}
my($ca_name) = read_length_input("CA Name", 1);
my($dir_name) = read_directory_input("Directory name"); 
my($country_name) = read_fixed_input("Country Name (2 letter code)", 2, "US");
my($state_name) = read_input("State or Province Name (full name)", "Wisconsin");
my($locality_name) = read_input("Locality Name (eg, city)", "Madison");
my($org_name) = read_input("Organization Name (eg, company)", "Condor Project");
my($org_unit) = read_input("Organizational Unit Name (eg, section)", "Security Research");
my($key_size) = read_set_input("Key size (1024, 2048, 4096)", 
			       1024, [1024, 2048, 4096]);
my($validity_period) =  read_range_input("Validity period in days", 
					 365, 1, 365*10);

#print "Password: $password\n";
#print "Name: $ca_name\n";
#print "Size: $key_size\n";
#print "Period: $validity_period\n";


my($openssl_cnf) = get_raw_openssl_cnf();

$openssl_cnf = modify_openssl_cnf($openssl_cnf, $dir_name, 
				  $country_name, 
				  $state_name, $locality_name, 
				  $org_name, $org_unit);

# now we begin

# create directory and directory structure
if(create_directories($dir_name)) {
    die "Can't create and populate miniCA directory.\n";
}

# write openssl.cnf
if(write_openssl_cnf($openssl_cnf, $dir_name)) {
    die "Can't write openssl.cnf file.\n";
}

do_openssl_keygen($dir_name, $ca_name, $key_size, $openssl_bin, 
		  $password, $validity_period);

sub find_openssl_bin {
    my(@path) = split /:/, $ENV{PATH};
    foreach my $path (@path) {
	opendir PATHDIR, $path or next;
	my(@files) = readdir PATHDIR;
	foreach my $file (@files) {
	    if($file eq 'openssl') {
		return "$path/$file";
	    }
	}
    }
    return '';
}#"#for effin emacs

# create key and self-signed cert.
sub do_openssl_keygen {
    my($dir_name, $ca_name, $key_size, $openssl_bin,
       $password, $validity_period) = @_;

    my $genrsa_cmd = "$openssl_bin genrsa -des3 -passout stdin -out $dir_name/private/root-ca.key $key_size";
    print "Executing command: '$genrsa_cmd'\n";
    open(GENRSA, "|$genrsa_cmd") or die "Can't run openssl genrsa: $!";
    print GENRSA "$password\n";
    close GENRSA;

    # TODO: This should be converted to the other method of doing 
    # this: see operate_ca.pl
    my $subj = "/C=$country_name/ST=$state_name/L=$locality_name/O=$org_name/OU=$org_unit/CN=$ca_name";
    my $req_cmd = "$openssl_bin req -new -x509 -days $validity_period -key $dir_name/private/root-ca.key -passin stdin -out $dir_name/root-ca.crt -config $dir_name/openssl.cnf -batch -subj '$subj'";
    print "Executing Command: $req_cmd\n";
    open(REQ, "|$req_cmd") or die "Can't run openssl req: $!";
    print REQ "$password\n";
    close REQ;

    # handle errors

}

sub docmd {
    my($cmd) = @_;
    print "Executing command: $cmd\n";
    print `$cmd`;
    return $?>>8;
}

sub write_openssl_cnf {
    my($openssl_cnf, $dir_name) = @_;
    if(not open(OC, ">$dir_name/openssl.cnf")) {
	print STDERR "Can't open openssl.cnf for writing: $!.\n";
	return 1;
    }
    print OC $openssl_cnf;
    close OC;
    return 0;
}

# maybe we should do something more intelligent on failure?
sub create_directories {
    my($ca_root_dir) = @_;

    # Create directory for CA files.
    if(not -d $ca_root_dir) {
	if(not (mkdir $ca_root_dir, 0700)) {
	    print STDERR "Can't create '$ca_root_dir' directory: $!.\n";
	    return 1;
	}
    }

    # Create directory for sensitive files.
    if(not -d "$ca_root_dir/private") {
	if(not (mkdir "$ca_root_dir/private", 0700)) {
	    print STDERR "Can't create '$ca_root_dir/private' directory: $!.\n";
	    return 1;
	}
    }


    # Create directory for certificate files.
    if(not -d "$ca_root_dir/ca.db.certs") {
	if(not (mkdir "$ca_root_dir/ca.db.certs", 0700)) {
	    print STDERR "Can't create '$ca_root_dir/ca.db.certs': $!\n";
	    return 1;
	}
    }

    # Initialize serial if it isn't already created.
    if(not -f "$ca_root_dir/ca.db.serial") {
	if(not(open SERIAL, ">$ca_root_dir/ca.db.serial")) {
	    print STDERR "Can't create serial: $!\n";
	    return 1;
	}
	
	print SERIAL "01\n";
	close SERIAL;
    }

    # Create index file if not present.
    if(not -f "$ca_root_dir/ca.db.index") {
	if(not (open INDEX, ">$ca_root_dir/ca.db.index")) {
	    print STDERR "Can't create index: $!\n";
	    return 1;
	}
	close INDEX;
    }

    # Initialize random number file.
    if(not -f "$ca_root_dir/private/ca.db.rand") {
	if(not (open RAND, ">$ca_root_dir/private/ca.db.rand")) {
	    print STDERR "Can't create randfile: $!\n";
	    return 1;
	}
	my($r) = int(rand(90)+10);
	print RAND "$r\n";
	close RAND;
    }
    return 0;
}

sub read_input {
    my($title, $default, $ispass) = @_;
    # Just basic for now - add readline?
    my($rv);
    my($defp) = defined($default) ? " [$default]" : '';
    print "Enter $title$defp: ";
    if($ispass) {
	ReadMode('noecho');
	$rv = ReadLine(0);
	ReadMode('restore');
	print "\n";
    } else {
	$rv = <>;
    }
    chomp $rv;
    if(defined($default) and $rv eq '') {
	$rv = $default;
    }
    return $rv;
}

sub read_directory_input {
    my($title) = @_;
    my $done = 0;
    my $rv = '';
    my $default = getcwd;
    while(not $done) {
	$rv = read_input($title, $default);
	my($parent) = dirname($rv);
	if(not -d $parent) {
	    print "$parent is not a directory.\n";
	    next;
	}
	if(not -w $parent) {
	    print "$parent is not writable.\n";
	    next;
	}
	print "$parent is a writable directory, ok.\n";
	last;
    }
    return $rv;
}

sub read_length_input {
    my($title, $minlen, $ispass) = @_;
    my($rv) = ('');
    while(length($rv) < $minlen) {
	$rv = read_input($title, undef, $ispass);
	if(length($rv) < $minlen) {
	    print "$title may not be shorter than $minlen chars.\n";
	}
    }
    return $rv;
}

sub read_fixed_input {
    my($title, $len, $default) = @_;
    my($rv) = ('');
    while(length($rv) != $len) {
	$rv = read_input($title, $default, 0);
	if(length($rv) != $len) {
	    print "$title must be $len chars.\n";
	}
    }
    return $rv;
}


sub read_range_input {
    my($title, $default, $min, $max) = @_;
    my $rv = '';
    while($rv eq '' or $rv < $min or $rv > $max) {
	$rv = read_input($title, $default);
	if($rv eq '') {
	    $rv = $default;
	}
    }
    return $rv;
}

sub read_set_input {
    my($title, $default, $set) = @_;
    my $rv = '';
    my(@set) = @$set;
    my(%set);
    foreach my $item (@set) {
	#print STDERR "Defining $item\n";
	$set{$item} = 1;
    }
    while($rv eq '' or not is_in_set($rv,\%set)) {
	$rv = read_input($title, $default);
	if($rv eq '') {
	    $rv = $default;
	}
    }
    return $rv;
}

sub is_in_set {
    my($cand, $set) = @_;
    #print join("|", keys(%$set));
    return defined($set->{$cand});
}

sub get_raw_openssl_cnf {
    # this is from http://cvs.openssl.org/getfile/openssl/apps/openssl.cnf?v=1.23.2.5
    # retrieved 5/17/2006 and should be updated to reflect the latest version.
    # Don't modify the source file: use the next function to make changes to the
    # version of the file that this script makes as output.  This allows us to keep up to
    # date by updating the file distributed with this script.
    #
    # TODO: find a way around a hardcoded path.
    my $cnfloc = $0;
    #/p/condor/workspaces/alderman/minica_setup_script/openssl.cnf" 
    if($cnfloc !~ /^\//) {
	$cnfloc = "./$cnfloc";
    }
    $cnfloc =~ s/(.*)\/.*/$1\/openssl.cnf/;
    print STDERR "Retrieving openssl.cnf from $cnfloc\n";
    open CNF, "$cnfloc" 
	or die "$0: Can't open '$cnfloc': $!";
    my $rv = join('',(<CNF>));
    close CNF;
    return $rv;
}

sub modify_openssl_cnf {
    my($openssl_cnf, $dir_name, $country_name, $state_name, $locality_name,
       $org_name, $org_unit) = @_;

    if(not ($openssl_cnf =~ s/(dir\s+=\s+)\.\/demoCA(\s+. Where.*)/$1$dir_name$2/)) {
	die "Couldn't modify the directory line in the openssl.cnf file.\n";
    }
    if(not ($openssl_cnf =~ s/(countryName_default\s+=\s+)AU.*/$1$country_name/)) {
	die "Couldn't modify the countryName_default line in the openssl.cnf file.\n";
    }
    if(not ($openssl_cnf =~ s/(stateOrProvinceName_default\s+=\s+)Some-State.*/$1$state_name/)) {
	die "Couldn't modify the stateOrPorvinceName_default line in the openssl.cnf file.\n";
    }
    if(not ($openssl_cnf =~ s/(localityName.*Locality.*)/$1\nlocalityName_default            = $locality_name/)) {
	die "Couldn't modify the localityName_default line in the openssl.cnf file.\n";
    }
    if(not ($openssl_cnf =~ s/(0.organizationName_default\s+=\s+)Internet .*/$1$org_name/)) {
	die "Couldn't modify the organizationName_default line in the openssl.cnf file.\n";
    }
    if(not ($openssl_cnf =~ s/.(organizationalUnitName_default\s+=).*/$1 $org_unit/)) {
	die "Couldn't modify the organizationalUnitName_default line in the openssl.cnf file.\n";
    }
    if(not ($openssl_cnf =~ s/(private_key.*)\/cakey.pem/$1\/root-ca.key/)) {
	die "Couldn't modify the private_key line in the openssl.cnf file.\n";
    }
    if(not ($openssl_cnf =~ s/(certificate.*)\/cacert.pem/$1\/root-ca.crt/)) {
	die "Couldn't modify the certificate line in the openssl.cnf file.\n";
    }
    if(not ($openssl_cnf =~ s/(certs\s+.*)\/certs/$1\/ca.db.certs/)) {
	die "Couldn't modify the certs line in the openssl.cnf file.\n";
    }
    if(not ($openssl_cnf =~ s/(new_certs_dir\s+.*)\/newcerts/$1\/ca.db.certs/)) {
	die "Couldn't modify the new_certs_dir line in the openssl.cnf file.\n";
    }
    # ca.db.rand
    if(not ($openssl_cnf =~ s/(RANDFILE\s+.*)\/.rand/$1\/ca.db.rand/)) {
	die "Couldn't modify the RANDFILE line in the openssl.cnf file.\n";
    }
    # ca.db.serial
    if(not ($openssl_cnf =~ s/(serial\s+.*)\/serial/$1\/ca.db.serial/)) {
	die "Couldn't modify the serial line in the openssl.cnf file.\n";
    }
    # ca.db.index
    if(not ($openssl_cnf =~ s/(database\s+.*)\/index.txt/$1\/ca.db.index/)) {
	die "Couldn't modify the database line in the openssl.cnf file.\n";
    }

    return $openssl_cnf;
}

__END__

    $openssl_cnf .= <<END_ADD_OPENSSL_CNF;

[ req_self_signed ]
C                      = $country_name
ST                     = $state_name
L                      = $locality_name
O                      = $org_name
OU                     = $org_unit
CN                     = $ca_name

END_ADD_OPENSSL_CNF
