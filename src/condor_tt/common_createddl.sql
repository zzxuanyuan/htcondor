/* 
   the script should be installed into the database that's used by quill
   and installed as the quillwriter user the quill daemon will connect to 
   for updating the database. The right permissions shoud have been 
   set up for the user for the creation of the following schema objects.
*/

CREATE TABLE cdb_users (
username varchar(30),
password character(32),
admin varchar(5));

CREATE TABLE  transfers (
globaljobid  	varchar(4000),
src_name  	varchar(4000),
src_host  	varchar(4000),
src_port	integer,
src_path 	varchar(4000),
src_daemon      varchar(30),
dst_name  	varchar(4000),
dst_host  	varchar(4000),
dst_port        integer,
dst_path  	varchar(4000),
dst_daemon  	varchar(30),
transfer_size_bytes   numeric(38),
elapsed  	numeric(38),
checksum    	varchar(256),
transfer_time	timestamp(3) with time zone,
last_modified	timestamp(3) with time zone,
is_encrypted    varchar(5)
);

CREATE TABLE files (
file_id  	int NOT NULL,
name  		varchar(4000), 
host  		varchar(4000),
path  		varchar(4000),
lastmodified    timestamp(3) with time zone,
file_size  	integer,
checksum  	varchar(32), 
PRIMARY KEY (file_id)
);

CREATE TABLE fileusages (
globaljobid  	varchar(4000),
file_id         int REFERENCES files(file_id),
usagetype  	varchar(4000));

CREATE sequence condor_seqfileid;

-- Added by pachu

CREATE TABLE machines_horizontal (
machine_id             varchar(4000) NOT NULL,
opsys                  varchar(4000),
arch                   varchar(4000),
ckptserver             varchar(4000),
state                  varchar(4000),
activity               varchar(4000),
keyboardidle           integer,
consoleidle            integer,
loadavg                real,
condorloadavg          real,
totalloadavg           real,
virtualmemory          integer,
memory                 integer,
totalvirtualmemory     integer,
cpubusytime            integer,
cpuisbusy              varchar(5),
rank                   varchar(4000),
currentrank            real,
requirements           varchar(4000),
clockmin               integer,
clockday               integer,
lastheardfrom          timestamp(3) with time zone,
enteredcurrentactivity timestamp(3) with time zone,
enteredcurrentstate    timestamp(3) with time zone,
updatesequencenumber   integer,
updatestotal           integer,
updatessequenced       integer,
updateslost            integer,
globaljobid            varchar(4000),
lastheardfrom_epoch    integer,
Primary Key (machine_id)
);

CREATE TABLE machines_horizontal_history (
machine_id             varchar(4000),
opsys                  varchar(4000),
arch                   varchar(4000),
ckptserver             varchar(4000),
state                  varchar(4000),
activity               varchar(4000),
keyboardidle           integer,
consoleidle            integer,
loadavg                real,
condorloadavg          real,
totalloadavg           real,
virtualmemory          integer,
memory                 integer,
totalvirtualmemory     integer,
cpubusytime            integer,
cpuisbusy              varchar(5),
rank                   varchar(4000),
currentrank            real,
requirements           varchar(4000),
clockmin               integer,
clockday               integer,
lastheardfrom          timestamp(3) with time zone,
enteredcurrentactivity timestamp(3) with time zone,
enteredcurrentstate    timestamp(3) with time zone,
updatesequencenumber   integer,
updatestotal           integer,
updatessequenced       integer,
updateslost            integer,
globaljobid            varchar(4000),
end_time	       timestamp(3) with time zone
);

-- END Added by Pachu

-- BEGIN Added by Ameet

CREATE SEQUENCE SeqRunId;

CREATE TABLE Runs (
run_id 	                NUMERIC(12) NOT NULL,
machine_id              varchar(4000),
scheddname	        varchar(4000),
cluster_id              integer,
proc_id			integer,
spid                    integer,
StartTS                 timestamp(3) with time zone,
EndTS                   timestamp(3) with time zone,
EndType                 smallint,
EndMessage              varchar(4000),
WasCheckpointed         varchar(5),
ImageSize               integer,
RunLocalUsageUser       integer,
RunLocalUsageSystem     integer,
RunRemoteUsageUser      integer,
RunRemoteUsageSystem    integer,
RunBytesSent            numeric(38),
RunBytesReceived        numeric(38), 
PRIMARY KEY (run_id));

-- END Added by Ameet

-- BEGIN Srinath

CREATE TABLE Rejects (
reject_time     timestamp(3) with time zone, -- Time the job was rejected
username        varchar(4000),
scheddname      varchar(4000),
cluster_id      integer,
proc_id		integer,
globaljobid     varchar(4000),
);

CREATE TABLE Matches (
match_time      timestamp(3) with time zone, -- Time the match was made
username        varchar(4000),
scheddname      varchar(4000),
cluster_id      integer,
proc_id		integer,
globaljobid     varchar(4000), 
machine_id      varchar(4000),
remote_user     varchar(4000),   -- The user that was preempted
remote_priority real       -- Preempted user's priority
);

--END Srinath

CREATE TABLE L_Jobstatus (
jobstatus	integer NOT NULL,
abbrev	        char(1),
description     varchar(4000),
primary key	(jobstatus)
);

INSERT INTO L_JobStatus VALUES(0, 'U', 'UNEXPANDED');
INSERT INTO L_JobStatus VALUES(1, 'I', 'IDLE');
INSERT INTO L_JobStatus VALUES(2, 'R', 'RUNNING');
INSERT INTO L_JobStatus VALUES(3, 'X', 'REMOVED');
INSERT INTO L_JobStatus VALUES(4, 'C', 'COMPLETED');
INSERT INTO L_JobStatus VALUES(5, 'H', 'HELD');
INSERT INTO L_JobStatus VALUES(6, 'E', 'SUBMISSION_ERROR');

--END Eric

CREATE TABLE throwns(
fileName       varchar(4000),
machine_id     varchar(4000),
log_size           integer,
throwTime      timestamp(3) with time zone
);

CREATE TABLE events (
scheddname      varchar(4000),
cluster_id      integer,
proc_id		integer,
runId     	numeric(12, 0),
eventType       integer,
eventTime       timestamp(3) with time zone,
description     varchar(4000)
);

CREATE TABLE L_eventType (
eventType     integer,
description   varchar(4000)
);

INSERT INTO L_eventType values (0, 'Job submitted');
INSERT INTO L_eventType values (1, 'Job now running');
INSERT INTO L_eventType values (2, 'Error in executable');
INSERT INTO L_eventType values (3, 'Job was checkpointed');
INSERT INTO L_eventType values (4, 'Job evicted from machine');
INSERT INTO L_eventType values (5, 'Job terminated');
INSERT INTO L_eventType values (6, 'Image size of job updated');
INSERT INTO L_eventType values (7, 'Shadow threw an exception');
INSERT INTO L_eventType values (8, 'Generic Log Event');
INSERT INTO L_eventType values (9, 'Job Aborted');
INSERT INTO L_eventType values (10, 'Job was suspended');
INSERT INTO L_eventType values (11, 'Job was unsuspended');
INSERT INTO L_eventType values (12, 'Job was held');
INSERT INTO L_eventType values (13, 'Job was released');
INSERT INTO L_eventType values (14, 'Parallel Node executed');
INSERT INTO L_eventType values (15, 'Parallel Node terminated');
INSERT INTO L_eventType values (16, 'POST script terminated');
INSERT INTO L_eventType values (17, 'Job Submitted to Globus');
INSERT INTO L_eventType values (18, 'Globus Submit failed');
INSERT INTO L_eventType values (19, 'Globus Resource Up');
INSERT INTO L_eventType values (20, 'Globus Resource Down');
INSERT INTO L_eventType values (21, 'Remote Error');
INSERT INTO L_eventType values (22, 'RSC socket lost');
INSERT INTO L_eventType values (23, 'RSC socket re-established');
INSERT INTO L_eventType values (24, 'RSC reconnect failure');

CREATE TABLE JobQueuePollingInfo (
scheddname           varchar(4000),
last_file_mtime      INTEGER, 
last_file_size       INTEGER, 
last_next_cmd_offset INTEGER, 
last_cmd_offset      INTEGER, 
last_cmd_type        SMALLINT, 
last_cmd_key         varchar(4000), 
last_cmd_mytype      varchar(4000), 
last_cmd_targettype  varchar(4000), 
last_cmd_name        varchar(4000), 
last_cmd_value       varchar(4000)
); 

CREATE INDEX JQ_I_Schedd ON JobQueuePollingInfo(scheddname);

CREATE TABLE Currencies(
datasource varchar(4000),
lastupdate timestamp(3) with time zone
);

CREATE TABLE Daemons_Horizontal (
MyType				VARCHAR(100) NOT NULL,
Name				VARCHAR(500) NOT NULL,
LastHeardFrom			TIMESTAMP(3) WITH TIME ZONE,
MonitorSelfTime			TIMESTAMP(3) WITH TIME ZONE,
MonitorSelfCPUUsage		numeric(38),
MonitorSelfImageSize		numeric(38),
MonitorSelfResidentSetSize	INTEGER,
MonitorSelfAge			INTEGER,
UpdateSequenceNumber		INTEGER,
UpdatesTotal			INTEGER,
UpdatesSequenced		INTEGER,
UpdatesLost			INTEGER,
UpdatesHistory			VARCHAR(4000),
lastheardfrom_epoch             integer,
PRIMARY KEY (MyType, Name)
);

CREATE TABLE Daemons_Horizontal_History (
MyType				VARCHAR(100),
Name				VARCHAR(500),
LastHeardFrom			TIMESTAMP(3) WITH TIME ZONE,
MonitorSelfTime			TIMESTAMP(3) WITH TIME ZONE,
MonitorSelfCPUUsage		numeric(38),
MonitorSelfImageSize		numeric(38),
MonitorSelfResidentSetSize	INTEGER,
MonitorSelfAge			INTEGER,
UpdateSequenceNumber		INTEGER,
UpdatesTotal			INTEGER,
UpdatesSequenced		INTEGER,
UpdatesLost			INTEGER,
UpdatesHistory			VARCHAR(4000),
EndTime				TIMESTAMP(3) WITH TIME ZONE
);

CREATE TABLE Schedds_Horizontal (
Name				VARCHAR(500) NOT NULL,
LastHeardFrom			TIMESTAMP(3) WITH TIME ZONE,
NumUsers			INTEGER,
TotalIdleJobs			INTEGER,
TotalRunningJobs		INTEGER,
TotalJobAds			INTEGER,
TotalHeldJobs			INTEGER,
TotalFlockedJobs		INTEGER,
TotalRemovedJobs		INTEGER,
PRIMARY KEY (Name)
);

CREATE TABLE Schedds_Horizontal_History (
Name				VARCHAR(500),
LastHeardFrom			TIMESTAMP(3) WITH TIME ZONE,
NumUsers			INTEGER,
TotalIdleJobs			INTEGER,
TotalRunningJobs		INTEGER,
TotalJobAds			INTEGER,
TotalHeldJobs			INTEGER,
TotalFlockedJobs		INTEGER,
TotalRemovedJobs		INTEGER,
EndTime				TIMESTAMP(3) WITH TIME ZONE
);

-- this table is used internally by the quill daemon for constructing a 
-- single tuple in a sql statement for updating database, end users 
-- don't need to access this table
CREATE TABLE dummy_single_row_table(a varchar(1));
INSERT INTO dummy_single_row_table VALUES ('x');

