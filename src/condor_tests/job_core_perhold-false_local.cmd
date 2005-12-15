universe   = local
executable = x_sleep.pl
log = job_core_perhold-false_local.log
output = job_core_perhold-false_local.out
error = job_core_perhold-false_local.err
hold	= false
periodic_hold = (CurrentTime - QDate) < 0
Notification = NEVER
arguments  = 4
queue

