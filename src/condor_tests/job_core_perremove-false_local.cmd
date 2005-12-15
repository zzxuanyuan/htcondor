universe   = local
executable = x_sleep.pl
log = job_core_perremove-false_local.log
output = job_core_perremove-false_local.out
error = job_core_perremove-false_local.err
Notification = NEVER
arguments  = 3
periodic_remove = (CurrentTime - QDate) < (0 )
queue

