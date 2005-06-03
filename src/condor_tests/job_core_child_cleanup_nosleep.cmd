universe = vanilla
executable = job_core_child_cleanup_nosleep.pl
log = job_core_child_cleanup_nosleep.log
output = job_core_child_cleanup_nosleep.out
error = job_core_child_cleanup_nosleep.err
arguments = $$(OPSYS)  0
getenv=false
Notification = NEVER
queue

