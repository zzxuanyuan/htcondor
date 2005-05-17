#!/bin/sh

sshd_cleanup() {
	/bin/rm -f $hostkey ${hostkey}.pub	${idkey} ${idkey}.pub sshd.out contact
}

trap sshd_cleanup 15

# note the sshd requires full path
# These should be moved to the config file
SSHD=`pwd`/sshd
KEYGEN=/s/openssh/bin/ssh-keygen
PORT=4444

CONDOR_REMOTE_SPOOL_DIR=$CONDOR_REMOTE_SPOOL_DIR
CONDOR_PROCNO=$1
CONDOR_NPROCS=$2
CONDOR_CHIRP=./condor_chirp

# Remove these lines when we don't transfer these
chmod 755 sshd
chmod 755 condor_chirp

# Create the host key. 
/bin/rm -f hostkey hostkey.pub
$KEYGEN -q -f hostkey -t rsa -N '' 
if [ $? -ne 0 ]
then
	echo ssh keygenerator $KEYGEN returned error $? exiting
	exit -1
fi

hostkey=`pwd`/hostkey
idkey=`pwd`/$CONDOR_PROCNO.key

# Create the identity key
$KEYGEN -q -f $idkey -t rsa -N '' 
if [ $? -ne 0 ]
then
	echo ssh keygenerator $KEYGEN returned error $? exiting
	exit -1
fi

# Send the identity keys back home
$CONDOR_CHIRP put -perm 0700 $idkey $CONDOR_REMOTE_SPOOL_DIR/$CONDOR_PROCNO.key
if [ $? -ne 0 ]
then
	echo error $? chirp putting identity keys back
	exit -1
fi

# ssh needs full paths to all of its arguments
# Start up sshd
done=0

while [ $done -eq 0 ]
do

# Try to launch sshd on this port
$SSHD -p$PORT -oAuthorizedKeysFile=${idkey}.pub -h$hostkey -De -f/dev/null -oStrictModes=no -oPidFile=/dev/null -oAcceptEnv=_CONDOR < /dev/null > sshd.out 2>&1 &

pid=$!

# Give sshd some time
sleep 2
if grep "^Server listening on 0.0.0.0 port" sshd.out > /dev/null 2>&1
then
	done=1
else
		# it is probably dead now
		#kill -9 $pid > /dev/null 2>&1
		PORT=`expr $PORT + 1`
fi

done

# Don't need this anymore
/bin/rm sshd.out

# create contact file
hostname=`hostname`
currentDir=`pwd`
user=`whoami`

echo "$CONDOR_PROCNO $hostname $PORT $user $currentDir"  |
	$CONDOR_CHIRP put -mode cwa - $CONDOR_REMOTE_SPOOL_DIR/contact 

if [ $? -ne 0 ]
then
	echo error $? chirp putting contact info back to submit machine
	exit -1
fi

# On the head node, grep for the contact file
# and the keys
if [ $CONDOR_PROCNO -eq 0 ]
then
	done=0

	# Need to poll the contact file until all nodes have
	# reported in
	while [ $done -eq 0 ]
	do
			/bin/rm -f contact
			$CONDOR_CHIRP fetch $CONDOR_REMOTE_SPOOL_DIR/contact contact
			lines=`wc -l contact | awk '{print $1}'`
			if [ $lines -eq $CONDOR_NPROCS ]
			then
				done=1
				node=0
				while [ $node -ne $CONDOR_NPROCS ]
				do
						$CONDOR_CHIRP fetch $CONDOR_REMOTE_SPOOL_DIR/$node.key $node.key
						node=`expr $node + 1`
				done
				chmod 0700 *.key
			else
				sleep 1
			fi
	done
fi

# We'll source in this file in the MPI startup scripts,
# so we can wait and sshd_cleanup over there as needed
#wait
#sshd_cleanup
