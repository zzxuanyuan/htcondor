#!/usr/bin/python

import sys
import string
import os

# Pre define the dictionary

cfgoptions = {

				'bgd-support': 'Enable support for HTC mode on the BlueGene machine'
				'checksum-md5':   Enable production and validation of MD5 checksums
#                          for released packages.
			 }

def print_usage(): 
    print 'configure.py '


# define the space of options..  
# Usage: ./configure [OPTION]... [VAR=VALUE]...
#
# To assign environment variables (e.g., CC, CFLAGS...), specify them as
# VAR=VALUE.  See below for descriptions of some of the useful variables.
#
# Defaults for the options are specified in brackets.
#
# Configuration:
#  -h, --help              display this help and exit
#
# Installation directories:
#  --prefix=PREFIX         install architecture-independent files in PREFIX
#                          [/usr/local]
#
#Optional Features:
#  --disable-option-checking  ignore unrecognized --enable/--with options
#  --disable-FEATURE       do not include FEATURE (same as --enable-FEATURE=no)
#  --enable-FEATURE[=ARG]  include FEATURE [ARG=yes]
#  --enable-bgd-support    Enable support for HTC mode on the BlueGene machine.
#  --enable-checksum-md5   Enable production and validation of MD5 checksums
#                          for released packages.
#  --enable-checksum-sha1  Enable production and validation of SHA1 checksums
#                          for released packages.
#  --enable-job-hooks      Support for invoking hooks throughout the workflow
#                          of a job
#  --enable-hibernation    Support for Condor-controlled hibernation
#  --enable-rpm            determine if we try to make rpms (default: yes)
#  --enable-malloc-debug   enable memory allocation debugging (default: no)
#  --enable-nest           enable NeST functionality (default: no)
#  --enable-quill          enable Quill functionality (default: yes)
#  --enable-kbdd           enable KBDD functionality (default: platform
#                          dependent)
#  --enable-hdfs           enable hadoop filesystem functionality (default:
#                          yes)
#  --enable-static         determine if we do static linking (default: yes)
#  --enable-ssh-to-job     Support for condor_ssh_to_job
#  --enable-stork          enable Stork functionality (default: platform
#                          dependent)
#  --enable-lease-manager  enable lease manager functionality (default:
#                          platform dependent)
#
#Optional Packages:
#  --with-PACKAGE[=ARG]    use PACKAGE [ARG=yes]
#  --without-PACKAGE       do not use PACKAGE (same as --with-PACKAGE=no)
#  --with-buildid          Add a build identification string to the Condor
#                          Version String
#  --with-externals=DIR    Directory in which to build and install external
#                          programs needed for building Condor (default is to
#                          build in ../externals)
#  --with-purecachedir=DIR cache directory for objects instrumented with Purify
#                          (default is $TMPDIR)
#  --with-vmware=DIR       full path to directory where the vmware program is
#                          located
#  --with-x                use the X Window System
#  --with-linuxlibcheaders[=DIR]
#                          (soft requirement)
#  --with-glibc[=DIR]      (soft requirement)
#  --with-coredumper[=DIR] (soft requirement)
#  --with-drmaa[=DIR]      (soft requirement)
#  --with-krb5[=DIR]       use krb5 (provides Kerberos support) (soft
#                          requirement)
#  --with-openssl[=DIR]    use OpenSSL (provides authentication and encryption
#                          support) (soft requirement)
#  --with-globus[=DIR]     (soft requirement)
#  --with-unicoregahp[=DIR]
#                          (soft requirement)
#  --with-zlib[=DIR]       use zlib (provides compression support)
#                          ($_cv_zlib_requirement requirement)
#  --with-classads[=DIR]   use new ClassAds (provides -better-analyze and more)
#                          (soft requirement)
#  --with-srb[=DIR]        (soft requirement)
#  --with-expat[=DIR]      (soft requirement)
#  --with-voms[=DIR]       (soft requirement)
#  --with-cream[=DIR]      (soft requirement)
#  --with-pcre[=DIR]       use PCRE (provides regular expression support) (hard
#                          requirement)
#  --with-blahp[=DIR]      (soft requirement)
#  --with-man[=DIR]        include the man pages (required for a release) (soft
#                          requirement)
#  --with-gcb[=DIR]        (soft requirement)
#  --with-oci=[ARG]        use Oracle OCI API from given Oracle home
#                          (ARG=path); use existing ORACLE_HOME (ARG=yes);
#                          disable Oracle OCI support (ARG=no)
#  --with-oci-include=[DIR]
#                          use Oracle OCI API headers from given path
#  --with-oci-lib=[DIR]    use Oracle OCI API libraries from given path
#  --with-postgresql[=DIR] use PostgreSQL (provides Quill and other support)
#                          (soft requirement)
#  --with-curl[=DIR]       (soft requirement)
#  --with-hadoop[=DIR]     (soft requirement)
#  --with-libxml2[=DIR]    (soft requirement)
#  --with-libvirt[=DIR]    (soft requirement)
#  --with-gsoap[=DIR]      use gSOAP (enables Birdbath interface) (soft
#                          requirement)
# The following are are the two map <string, string> 

# these configure and cflags arguments must be manually
#normal_args = ['--enable-threadsafe']



os.system(command_string)
