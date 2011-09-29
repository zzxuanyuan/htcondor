executable	= /bin/echo
arguments	= $(runnumber)
universe	= scheduler
log		= job_dagman_logfile_macro-A-$(runnumber).log
notification	= NEVER
output		= job_dagman_logfile_macro-A.out
error		= job_dagman_logfile_macro-A.err
queue
