#!/usr/bin/perl -w

# This is a simple script to operate a ca from the commandline.
# Sometimes it's nice to have an online CA, sometimes it's nice to do
# everything from the openssl command line, and sometimes you just
# want to make some certs from the commandline.  This script exists
# for the latter case.  It assumes things about what you want to do,
# and just works.  YMMV.

# TODO: insist that the user provide a filename.

use Getopt::Long;
use Term::ReadKey;
use strict;

my $usage = "$0 [--host|--user] --cn=\"Common Name\" \n" .
    "        --email=\"user\@domain.tld\" --filename=\"basename\"\n".
    "        --ca_dir=\"/path/to/ca/dir\"\n";

# options include; host vs user cert, specify cn, email address, 
# specify key file name.

my $host = '';
my $user = '';
my $cn = ''; 
my $email = '';
my $filename = '';
my $ca_dir = '';

my $result = GetOptions('host' => \$host,
			'user' => \$user,
			'cn=s' => \$cn,
			'filename=s' => \$filename,
			'email=s' => \$email,
			'ca_dir=s' => \$ca_dir );

# First we have to prompt for a password for the CA.
my $ca_password = read_length_input("CA Password", 4, 1);

print STDERR "Validating.\n";

if(not -d $ca_dir or not -f "$ca_dir/openssl.cnf") {
    die "Inappropriate CA directory: '$ca_dir'.\n" . 
	"Specify --ca_dir=[valid CA directory with openssl.cnf file].\n$usage";
}

my $type = '';
if($user and $host or not $user and not $host) {
    die "Specify one of either --user or --host.\n$usage";
}

my $user_password = '';
if($user) {
    $type = 'user';
    $user_password = read_length_input("User key password", 4, 1);
} else {
    $type = 'host';
}

my $input_file = "$filename.txt";
my $key_file = "$filename.key";
my $req_file = "$filename.req";
my $cert_file = "$filename.crt";
if(-e $key_file) {
    die "Key file exists: '$key_file'\n$usage";
}
if(-e $req_file) {
    die "Request file exists: '$req_file'\n$usage";
}
if(-e $cert_file) {
    die "Certificate file exists: '$cert_file'\n$usage";
}

# How to validate common name?
if(length($cn) < 5) {
    die "Common Name too short: '$cn'\n$usage";
}

# Stronger validation of email addresses?
# See: http://www.ex-parrot.com/~pdw/Mail-RFC822-Address.html
# see also perlfaq9
if($type eq 'user' and not $email =~ /^[\w.-]+\@(?:[\w-]+\.)+\w+$/) {
    die "Invalid email address: '$email'\n$usage";
}

if($type eq 'user' and $user_password eq '') {
    die "User can't have null password.\n$usage";
}
if($type eq 'host' and $user_password ne '') {
    die "Host must have null password.\n$usage";
}

# create input file
# This input file is created as per "DISTINGUISHED NAME AND ATTRIBUTE SECTION FORMAT" section of the openssl req man page.  prompt=no
open CNF, "$ca_dir/openssl.cnf" 
    or die "Can't open openssl config in '$ca_dir/openssl.cnf': $!\n$usage";
my($C, $ST, $L, $O, $OU, $CN);
while(<CNF>) {
    if(/^countryName_default\s+=\s+(.*)/) {
	$C = $1;
	next;
    }
    if(/^stateOrProvinceName_default\s+=\s+(.*)/) {
	$ST = $1;
	next;
    }
    if(/^localityName_default\s+=\s+(.*)/) {
	$L = $1;
	next;
    }
    if(/^0.organizationName_default\s+=\s+(.*)/) {
	$O = $1;
	next;
    }
    if(/^organizationalUnitName_default\s+=\s+(.*)/) {
	$OU = $1;
	next;
    }
}

my $subj = "/C=$C/ST=$ST/L=$L/O=$O/OU=$OU/CN=$cn";

# This part is superfluous; not used in the present implementation.
open FF, ">$input_file" or die "Can't open '$input_file' for writing: $!";
print FF <<ENDCNFHEAD;
RANDFILE               = $ca_dir/private/ca.db.rand

[ req ]
default_bits           = 1024
distinguished_name     = req_distinguished_name
prompt                 = no

[ req_distinguished_name ]
C                      = $C
ST                     = $ST
L                      = $L
O                      = $O
OU                     = $OU
CN                     = $cn
ENDCNFHEAD
    
if($email) {
    print FF "emailAddress           = $email\n";
    $subj .= "/Email=$email";
}

close FF;

# make the reqs and keys
if($type eq 'user') {
#    Not used: see above
#    my $req_cmd = "openssl req -newkey rsa:1024 -keyout $key_file -passout stdin -batch -config $input_file -out $req_file";
    my $req_cmd = "openssl req -newkey rsa:1024 -keyout $key_file -passout stdin -batch -config $ca_dir/openssl.cnf -subj \"$subj\" -out $req_file";
    print "Running command: '$req_cmd'\n";
    #open REQ, "|$req_cmd" or die "Can't run openssl req: $!";
    #print REQ $user_password;
    #close REQ; 
    print `$req_cmd`;
} else {
    my $req_cmd = "openssl req -newkey rsa:1024 -keyout $key_file -nodes -config $ca_dir/openssl.cnf -subj \"$subj\" -out $req_file -batch";
    print "Running command: '$req_cmd'\n";
    &do_cmd($req_cmd);
}
unlink($input_file);
my $ca_cmd = "openssl ca -config $ca_dir/openssl.cnf -passin stdin -batch -out $cert_file -infiles $req_file";
print "Running command: '$ca_cmd'\n";
open CA, "|$ca_cmd" or die "Can't run openssl ca: $!";
print CA "$ca_password";
close CA;


sub do_cmd {
    my($cmd) = @_;
    #print "$cmd\n";
    print `$cmd`;
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
