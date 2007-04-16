/*
Before installing this script, the following must have been prepared
1. quillreader account has been created
*/

CREATE TABLE machines_vertical (
machine_id varchar(4000) NOT NULL,
attr       varchar(2000) NOT NULL, 
val        clob, 
start_time timestamp(3) with time zone, 
Primary Key (machine_id, attr)
);

CREATE TABLE machines_vertical_history (
machine_id varchar(4000),
attr       varchar(4000), 
val        clob, 
start_time timestamp(3) with time zone, 
end_time   timestamp(3) with time zone
);

CREATE TABLE clusterads_horizontal(
scheddname          varchar(4000) NOT NULL,
cluster_id          integer NOT NULL,
owner               varchar(30),
jobstatus           integer,
jobprio             integer,
imagesize           numeric(38),
qdate               timestamp(3) with time zone,
remoteusercpu       numeric(38),
remotewallclocktime numeric(38),
cmd                 clob,
args                clob,
jobuniverse         integer,
primary key(scheddname,cluster_id)
);

CREATE INDEX ca_h_i_owner ON clusterads_horizontal (owner);

CREATE TABLE procads_horizontal(
scheddname		varchar(4000) NOT NULL,
cluster_id	 	integer NOT NULL,
proc_id			integer NOT NULL,
jobstatus 		integer,
imagesize 		numeric(38),
remoteusercpu	        numeric(38),
remotewallclocktime 	numeric(38),
remotehost              varchar(4000),
globaljobid        	varchar(4000),
jobprio            	integer,
args                    clob,
shadowbday		timestamp(3) with time zone,
enteredcurrentstatus    timestamp(3) with time zone,
numrestarts             integer,
primary key(scheddname,cluster_id,proc_id)
);

CREATE TABLE jobs_horizontal_history (
scheddname   varchar(4000) NOT NULL,
scheddbirthdate     integer NOT NULL,
cluster_id              integer NOT NULL,
proc_id			integer NOT NULL,
qdate                   timestamp(3) with time zone,
owner                   varchar(30),
globaljobid             varchar(4000),
numckpts                integer,
numrestarts             integer,
numsystemholds          integer,
condorversion           varchar(4000),
condorplatform          varchar(4000),
rootdir                 varchar(4000),
Iwd                     varchar(4000),
jobuniverse             integer,
cmd                     clob,
minhosts                integer,
maxhosts                integer,
jobprio                 integer,
negotiation_user_name   varchar(4000),
env                     clob,
userlog                 varchar(4000),
coresize                numeric(38),
killsig                 varchar(4000),
stdin	              	varchar(4000),
transferin              varchar(5),
stdout                  varchar(4000),
transferout             varchar(5),
stderr                  varchar(4000),
transfererr             varchar(5),
shouldtransferfiles     varchar(4000),
transferfiles           varchar(4000),
executablesize          numeric(38),
diskusage               integer,
filesystemdomain        varchar(4000),
args                    clob,
lastmatchtime           timestamp(3) with time zone,
numjobmatches           integer,
jobstartdate            timestamp(3) with time zone,
jobcurrentstartdate     timestamp(3) with time zone,
jobruncount             integer,
filereadcount           numeric(38),
filereadbytes           numeric(38),
filewritecount          numeric(38),
filewritebytes          numeric(38),
fileseekcount           numeric(38),
totalsuspensions        integer,
imagesize               numeric(38),
exitstatus              integer,
localusercpu            numeric(38),
localsyscpu             numeric(38),
remoteusercpu           numeric(38),
remotesyscpu            numeric(38),
bytessent      	        numeric(38),
bytesrecvd              numeric(38),
rscbytessent            numeric(38),
rscbytesrecvd           numeric(38),
exitcode                integer,
jobstatus               integer,
enteredcurrentstatus    timestamp(3) with time zone,
remotewallclocktime     numeric(38),
lastremotehost          varchar(4000),
completiondate          timestamp(3) with time zone,
enteredhistorytable     timestamp(3) with time zone,
primary key		(scheddname,scheddbirthdate, cluster_id, proc_id)
);

CREATE INDEX hist_h_i_owner ON jobs_horizontal_history (owner);

CREATE TABLE clusterads_vertical (
scheddname	varchar(4000) NOT NULL,
cluster_id		integer NOT NULL,
attr	        varchar(2000) NOT NULL,
val		clob,
primary key (scheddname,cluster_id, attr)
);

CREATE TABLE procads_vertical (
scheddname	varchar(4000) NOT NULL,
cluster_id	integer NOT NULL,
proc_id		integer NOT NULL,
attr	        varchar(2000) NOT NULL,
val		clob,
primary key (scheddname,cluster_id, proc_id, attr)
);

CREATE TABLE jobs_vertical_history (
scheddname	varchar(4000) NOT NULL,
scheddbirthdate integer NOT NULL,
cluster_id	integer NOT NULL,
proc_id		integer NOT NULL,
attr		varchar(2000) NOT NULL,
val		clob,
primary key (scheddname,scheddbirthdate, cluster_id, proc_id, attr)
);

CREATE TABLE generic_messages (
eventtype	varchar(4000),
eventkey	varchar(4000),
eventtime	timestamp(3) with time zone,
eventloc        varchar(4000),
attname	        varchar(4000),
attvalue	clob,
atttype	varchar(4000)
);

CREATE TABLE daemons_vertical (
mytype				VARCHAR(100) NOT NULL,
name				VARCHAR(500) NOT NULL,
attr				VARCHAR(4000) NOT NULL,
val				clob,
lastreportedtime		TIMESTAMP(3) WITH TIME ZONE,
PRIMARY KEY (MyType, Name, attr)
);

CREATE TABLE daemons_vertical_history (
mytype				VARCHAR(100),
name				VARCHAR(500),
lastreportedtime		TIMESTAMP(3) WITH TIME ZONE,
attr				VARCHAR(4000),
val				clob,
endtime				TIMESTAMP(3) WITH TIME ZONE
);

CREATE TABLE error_sqllogs (
logname   varchar(100),
host      varchar(50),
lastmodified timestamp(3) with time zone,
errorsql  varchar(4000),
logbody   clob, 
errormessage varchar(4000)
);

CREATE INDEX error_sqllog_idx ON error_sqllogs (logname, host, lastmodified);

CREATE VIEW agg_user_jobs_waiting AS
  SELECT c.owner, count(*) AS jobs_waiting
    FROM clusterads_horizontal c, procads_horizontal p
    WHERE c.cluster_id = p.cluster_id
      AND (p.jobstatus IS NULL OR p.jobstatus = 0 OR p.jobstatus = 1)
    GROUP BY c.owner; 

CREATE VIEW agg_user_jobs_held AS
  SELECT c.owner, count(*) as jobs_held
    FROM clusterads_horizontal c, procads_horizontal p
    WHERE c.cluster_id = p.cluster_id
      AND (p.jobstatus=5)
    GROUP BY c.owner;

CREATE VIEW agg_user_jobs_running AS
  SELECT c.owner, count(*) as jobs_running
    FROM clusterads_horizontal c, procads_horizontal p
    WHERE c.cluster_id = p.cluster_id
      AND (p.jobstatus=2)
    GROUP BY c.owner;

CREATE VIEW agg_user_jobs_fin_last_day AS
  SELECT h.owner, count(*) as jobs_completed 
    FROM jobs_horizontal_history h 
    WHERE h.jobstatus = 4 
      AND h.completiondate >= (current_timestamp - to_dsinterval('1 00:00:00'))
    GROUP BY h.owner;

-- Jobs that have historically flocked in to this pool for execution
-- (an anti-join between machine_classad history and jobs)
CREATE VIEW history_jobs_flocked_in AS 
SELECT DISTINCT globaljobid
FROM machines_horizontal_history 
WHERE SUBSTR(globaljobid, 1, (INSTR('#', globaljobid)-1)) 
      NOT IN (SELECT DISTINCT scheddname 
              FROM jobs_horizontal_history UNION 
              SELECT DISTINCT scheddname 
              FROM clusterads_horizontal);

-- Jobs that are currently flocking in to this pool for execution 
-- (an anti-join between machine_classad and jobs)
CREATE VIEW current_jobs_flocked_in AS 
SELECT DISTINCT globaljobid 
FROM machines_horizontal
WHERE SUBSTR(globaljobid, 1, (INSTR('#', globaljobid)-1)) 
      NOT IN (SELECT DISTINCT scheddname 
              FROM jobs_horizontal_history UNION 
              SELECT DISTINCT scheddname 
              FROM clusterads_horizontal);

-- Jobs that have historically flocked out to another pool for execution
-- (an anti-join between runs table and machine_classad)
-- The predicate "R.machine_id != R.scheddname" is added because some
-- jobs are executed locally on the schedd machine even if it's not 
-- a normal executing host.
CREATE VIEW history_jobs_flocked_out AS
SELECT DISTINCT scheddname, cluster_id, proc_id
FROM runs R 
WHERE R.endts IS NOT NULL AND
   R.machine_id != R.scheddname AND
   R.machine_id NOT IN 
  (SELECT DISTINCT SUBSTR(M.machine_id, (INSTR('@', M.machine_id)+1)) FROM machines_horizontal M);

-- Jobs that are currently flocking out to another pool for execution
-- (an anti-join between runs table and machine_classad)
-- machines must have reported less than 10 minutes ago to be counted
-- toward this pool.
CREATE VIEW current_jobs_flocked_out AS
SELECT DISTINCT R.scheddname, R.cluster_id, R.proc_id
FROM runs R, clusterads_horizontal C 
WHERE R.endts IS NULL AND
   R.machine_id != R.scheddname AND
   R.machine_id NOT IN 
  (SELECT DISTINCT SUBSTR(M.machine_id, (INSTR('@', M.machine_id)+1)) FROM machines_horizontal M where M.lastreportedtime >= current_timestamp - to_dsinterval('0 00:10:00'))  AND R.scheddname = C.scheddname AND R.cluster_id = C.cluster_id;

/*
quill_purgehistory for Oracle database.

quill_purgehistory does the following:
1. purge resource history data (e.g. machine history) that are older than 
   resourceHistoryDuration days

2. purge job run history data (e.g. runs, matchs, ...) that are older than 
   runHistoryDuration days

3. purge job history data that are older than 
   jobHistoryDuration days

4. Compute the total space used by the quill database

-- resource history data: no need to keep them for long
--   machine_history, machine_classad_history, 
--   daemon_horizontal_history, daemon_vertical_history, 

-- job run history data: purge when they are very old
--   transfers, fileusages, files, runs, events, rejects, matches

-- important job history data should be kept as long as possible
--   history_vertical, history_horizontal, thrown (log thrown events)

-- never purge current "operational data": 
--   machine, machine_classad, clusterads_horizontal, procads_horizontal, 
--   clusterads_vertical, procads_vertical, thrown, daemon_horizontal
--   daemon_vertical

-- resourceHistoryDuration, runHistoryDuration, jobHistoryDuration 
-- parameters are all in number of days

*/

--SET SERVEROUTPUT ON;

-- dbsize is in unit of megabytes
CREATE TABLE quilldbmonitor (
dbsize    integer
);

DELETE FROM quilldbmonitor;
INSERT INTO quilldbmonitor (dbsize) VALUES (0);

CREATE GLOBAL TEMPORARY TABLE history_jobs_to_purge(
scheddname   varchar(4000),
cluster_id   integer, 
proc_id      integer,
globaljobid  varchar(4000)) ON COMMIT DELETE ROWS;

CREATE OR REPLACE PROCEDURE 
quill_purgehistory(
resourceHistoryDuration integer,
runHistoryDuration integer,
jobHistoryDuration integer) AS 
totalUsedMB NUMBER;
BEGIN

/* first purge resource history data */

-- purge maintenance log older than resourceHistoryDuration days
DELETE FROM maintenance_log 
WHERE eventts < 
      (current_timestamp - 
       to_dsinterval(resourceHistoryDuration || ' 00:00:00'));

-- purge machine vertical attributes older than resourceHistoryDuration days
DELETE FROM machines_vertical_history
WHERE start_time < 
      (current_timestamp - 
       to_dsinterval(resourceHistoryDuration || ' 00:00:00'));

-- purge machine classads older than resourceHistoryDuration days
DELETE FROM machines_horizontal_history
WHERE lastreportedtime < 
      (current_timestamp - 
       to_dsinterval(resourceHistoryDuration || ' 00:00:00'));

-- purge daemon vertical attributes older than certain days
DELETE FROM daemons_vertical_history
WHERE lastreportedtime < 
      (current_timestamp - 
       to_dsinterval(resourceHistoryDuration || ' 00:00:00'));

-- purge daemon classads older than certain days
DELETE FROM daemons_horizontal_history
WHERE lastreportedtime < 
      (current_timestamp - 
       to_dsinterval(resourceHistoryDuration || ' 00:00:00'));

-- purge submitters classads older than certain days
DELETE FROM submitters_horizontal_history
WHERE lastreportedtime < 
      (current_timestamp - 
       to_dsinterval(resourceHistoryDuration || ' 00:00:00'));

COMMIT;

/* second purge job run history data */

-- find the set of jobs for which the run history are going to be purged
INSERT INTO history_jobs_to_purge 
SELECT scheddname, cluster_id, proc_id, globaljobid
FROM jobs_horizontal_history
WHERE enteredhistorytable < 
      (current_timestamp - 
       to_dsinterval(runHistoryDuration || ' 00:00:00'));

-- purge transfers data related to jobs older than certain days
DELETE FROM transfers 
WHERE globaljobid IN (SELECT globaljobid 
                      FROM history_jobs_to_purge);

-- purge fileusages related to jobs older than certain days
DELETE FROM fileusages
WHERE globaljobid IN (SELECT globaljobid 
                      FROM history_jobs_to_purge);

-- purge files that are not referenced any more
DELETE FROM files 
WHERE NOT EXISTS (SELECT *
                  FROM fileusages 
                  WHERE fileusages.file_id = files.file_id);

-- purge run data for jobs older than certain days
DELETE FROM runs R
WHERE exists (SELECT * 
              FROM history_jobs_to_purge H
              WHERE H.scheddname = R.scheddname AND
                    H.cluster_id = R.cluster_id AND
                    H.proc_id = R.proc_id);

-- purge rejects data for jobs older than certain days
DELETE FROM rejects R
WHERE exists (SELECT * 
              FROM history_jobs_to_purge H
              WHERE H.scheddname = R.scheddname AND
                    H.cluster_id = R.cluster_id AND
                    H.proc_id = R.proc_id);

-- purge matches data for jobs older than certain days
DELETE FROM matches M
WHERE exists (SELECT * 
              FROM history_jobs_to_purge H
              WHERE H.scheddname = M.scheddname AND
                    H.cluster_id = M.cluster_id AND
                    H.proc_id = M.proc_id);

-- purge events data for jobs older than certain days
DELETE FROM events E
WHERE exists (SELECT * 
              FROM history_jobs_to_purge H
              WHERE H.scheddname = E.scheddname AND
                    H.cluster_id = E.cluster_id AND
                    H.proc_id = E.proc_id);

COMMIT; -- commit will truncate the temporary table History_Jobs_To_Purge

/* third purge job history data */

-- find the set of jobs for which history data are to be purged
INSERT INTO history_jobs_to_purge 
SELECT scheddname, cluster_id, proc_id, globaljobid
FROM jobs_horizontal_history
WHERE enteredhistorytable < 
      (current_timestamp - 
       to_dsinterval(jobHistoryDuration || ' 00:00:00'));

-- purge vertical attributes for jobs older than certain days
DELETE FROM jobs_vertical_history V
WHERE exists (SELECT * 
              FROM history_jobs_to_purge H
              WHERE H.scheddname = V.scheddname AND
                    H.cluster_id = V.cluster_id AND
                    H.proc_id = V.proc_id);

-- purge classads for jobs older than certain days
DELETE FROM jobs_horizontal_history H
WHERE H.globaljobid IN (SELECT globaljobid 
                        FROM history_jobs_to_purge);


-- purge log thrown events older than jobHistoryDuration
-- The thrown table doesn't fall precisely into any of the categories,
-- it may contain information about job history log that is truncated.
-- We don't want the table to grow unbounded either.
DELETE FROM throwns T
WHERE T.throwtime < 
     (current_timestamp - 
      to_dsinterval(jobHistoryDuration || ' 00:00:00'));

-- purge sql error events older than jobHistoryDuration
-- The error_sqllogs table doesn't fall precisely into any of the categories, 
-- it may contain information about job history log that causes a sql error.
-- We don't want the table to grow unbounded either.
DELETE FROM error_sqllogs S
WHERE S.lastmodified < 
     (current_timestamp - 
      to_dsinterval(jobHistoryDuration || ' 00:00:00'));

COMMIT;

/* lastly check if db size is above 75 percentage of specified limit */
-- one caveat: index size is not counted in the usage calculation
-- gather stats first to have correct statistics 

DBMS_STATS.GATHER_TABLE_STATS(null, 'maintenance_log');
DBMS_STATS.GATHER_TABLE_STATS(null, 'runs');
DBMS_STATS.GATHER_TABLE_STATS(null, 'rejects');
DBMS_STATS.GATHER_TABLE_STATS(null, 'matches');
DBMS_STATS.GATHER_TABLE_STATS(null, 'l_jobstatus');
DBMS_STATS.GATHER_TABLE_STATS(null, 'throwns');
DBMS_STATS.GATHER_TABLE_STATS(null, 'events');
DBMS_STATS.GATHER_TABLE_STATS(null, 'l_eventtype');
DBMS_STATS.GATHER_TABLE_STATS(null, 'generic_messages');
DBMS_STATS.GATHER_TABLE_STATS(null, 'jobqueuepollinginfo');
DBMS_STATS.GATHER_TABLE_STATS(null, 'currencies');
DBMS_STATS.GATHER_TABLE_STATS(null, 'daemons_vertical');
DBMS_STATS.GATHER_TABLE_STATS(null, 'daemons_horizontal_history');
DBMS_STATS.GATHER_TABLE_STATS(null, 'daemons_vertical_history');
DBMS_STATS.GATHER_TABLE_STATS(null, 'submitters_horizontal');
DBMS_STATS.GATHER_TABLE_STATS(null, 'submitters_horizontal_history');
DBMS_STATS.GATHER_TABLE_STATS(null, 'dummy_single_row_table');
DBMS_STATS.GATHER_TABLE_STATS(null, 'cdb_users');
DBMS_STATS.GATHER_TABLE_STATS(null, 'transfers');
DBMS_STATS.GATHER_TABLE_STATS(null, 'files');
DBMS_STATS.GATHER_TABLE_STATS(null, 'fileusages');
DBMS_STATS.GATHER_TABLE_STATS(null, 'machines_vertical');
DBMS_STATS.GATHER_TABLE_STATS(null, 'machines_vertical_history');
DBMS_STATS.GATHER_TABLE_STATS(null, 'clusterads_horizontal');
DBMS_STATS.GATHER_TABLE_STATS(null, 'procads_horizontal');
DBMS_STATS.GATHER_TABLE_STATS(null, 'clusterads_vertical');
DBMS_STATS.GATHER_TABLE_STATS(null, 'procads_vertical');
DBMS_STATS.GATHER_TABLE_STATS(null, 'jobs_vertical_history');
DBMS_STATS.GATHER_TABLE_STATS(null, 'jobs_horizontal_history');
DBMS_STATS.GATHER_TABLE_STATS(null, 'machines_horizontal');
DBMS_STATS.GATHER_TABLE_STATS(null, 'machines_horizontal_history');
DBMS_STATS.GATHER_TABLE_STATS(null, 'daemons_horizontal');
DBMS_STATS.GATHER_TABLE_STATS(null, 'history_jobs_to_purge');

SELECT ROUND(SUM(NUM_ROWS*AVG_ROW_LEN)/(1024*1024)) INTO totalUsedMB
FROM user_tables;

DBMS_OUTPUT.PUT_LINE('totalUsedMB=' || totalUsedMB || ' MegaBytes');

UPDATE quilldbmonitor SET dbsize = totalUsedMB;

-- finally record this in the maintenance_log table 
INSERT INTO maintenance_log(eventts,eventmsg) 
VALUES(current_timestamp, 'purged data');

COMMIT;

END;
/

-- grant read access to quillreader
grant select any table to quillreader;

-- the creation of the schema version table should be the last step 
-- because it is used by quill daemon to decide whether we have the 
-- right schema objects for it to operate correctly
CREATE TABLE quill_schema_version (
major int, 
minor int, 
back_to_major int, 
back_to_minor int);

DELETE FROM quill_schema_version;
INSERT INTO quill_schema_version (major, minor, back_to_major, back_to_minor) VALUES (2,0,2,0);
