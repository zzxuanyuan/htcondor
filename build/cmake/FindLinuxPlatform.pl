#!/usr/bin/perl
use strict;
use warnings;

my $platform;
my $release;
my $dist_name;
my $major_ver;


if( -f "/etc/redhat-release")  {
	$release = `cat /etc/redhat-release`;
	if ($release=~/CentOS/i) {
		$release=~/.*?(\d+).*/;
		$major_ver=$1;
		$dist_name="rhel";
		#printf "On CentOS $major_ver\n"
	}
	elsif ($release=~/Red Hat/i) {
		$release=~/.*?(\d+).*/;
		$major_ver=$1;
		$dist_name="rhel";
		#printf "On Redhat $major_ver\n";

	}elsif ($release=~/Scientific Linux/i) {
		$release=~/.*?(\d+).*/;
		$major_ver=$1;
		$dist_name="rhel";
		#printf "On Scientific Linux $major_ver\n";

	}elsif ($release=~/Fedora/i) {
		$release=~/.*?(\d+).*/;
		$major_ver=$1;
		$dist_name="fc";
		#printf "On Fedora $major_ver\n";
	}
} elsif ( -f "/etc/debian_version") {
		$release = `cat /etc/debian_version`;
		$release=~/(\d+).*/;
		$major_ver=$1;
		$dist_name="deb";
		#printf "On Debian $major_ver\n";
}else {
	print "************************************************************\n";
	print "Cannot detect platform\n";
	print "************************************************************\n";
	exit 1;

}
print STDOUT "$dist_name\n";
print STDERR "$major_ver\n";


#$ENV{'DIST'}=$dist_name;
#$ENV{'MAJOR_VER'}=$major_ver;
