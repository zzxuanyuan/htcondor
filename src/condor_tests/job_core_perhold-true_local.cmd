universe   = local
executable = x_sleep.pl
log = job_core_perhold-true_local.log
output = job_core_perhold-true_local.out
error = job_core_perhold-true_local.err
hold	= false
#periodic_hold = (CurrentTime - QDate) > 0
periodic_hold = JobStatus == 2
Notification = NEVER
arguments  = 40
queue

