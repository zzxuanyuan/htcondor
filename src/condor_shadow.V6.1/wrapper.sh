#! /bin/sh
export PATH=$CONDOR_PARALLEL_RSH_DIR:$PATH
#printenv > /tmp/wrapper.log.$$
$CONDOR_PARALLEL_USER_SHELL -c "$*"
#exec $*
