universe   = local
executable = x_sleep.pl
log = job_core_perrelease-true_local.log
output = job_core_perrelease-true_local.out
error = job_core_perrelease-true_local.err
hold	= True
periodic_release = (CurrentTime - QDate) > 3
Notification = NEVER
arguments  = 6
queue

