executable   = /bin/echo
#TEMPTEMP arguments    = "-n $(XXX)' ' >> job_dagman_depth_first.order"
arguments    = "-n nodename' ' >> job_dagman_depth_first.order"
universe     = scheduler
output       = $(job).out
error        = $(job).err
log          = job_dagman_depth_first.log
Notification = NEVER
queue
