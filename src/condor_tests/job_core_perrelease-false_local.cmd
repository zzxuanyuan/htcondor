universe   = local
executable = x_sleep.pl
log = job_core_perrelease-false_local.log
output = job_core_perrelease-false_local.out
error = job_core_perrelease-false_local.err
hold	= True
periodic_release = (CurrentTime - QDate) < 0
Notification = NEVER
arguments  = 10
queue

