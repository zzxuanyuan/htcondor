universe   = local
executable = x_sleep.pl
log = job_core_perremove-true_local.log
output = job_core_perremove-true_local.out
error = job_core_perremove-true_local.err
periodic_remove = (CurrentTime - QDate) > 2
Notification = NEVER
arguments  = 3
queue

