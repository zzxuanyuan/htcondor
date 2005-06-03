universe = vanilla
executable = job_core_child_cleanup.pl
log = job_core_child_cleanup.log
output = job_core_child_cleanup.out
error = job_core_child_cleanup.err
arguments = $$(OPSYS) 10
getenv=false
Notification = NEVER
queue

