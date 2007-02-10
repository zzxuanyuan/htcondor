/*
Before installing this script, the following must have been prepared
1. quillreader account has been created
2. pl/pgSQL language has been created with "createlang plpgsql [dbname]"
*/

CREATE TABLE machines_vertical (
machine_id varchar(4000) NOT NULL,
attr       varchar(2000) NOT NULL, 
val        text, 
start_time timestamp(3) with time zone, 
Primary Key (machine_id, attr)
);

CREATE TABLE machines_vertical_history (
machine_id varchar(4000),
attr       varchar(4000), 
val        text, 
start_time timestamp(3) with time zone, 
end_time   timestamp(3) with time zone
);

CREATE TABLE ClusterAds_Horizontal(
scheddname          varchar(4000) NOT NULL,
cluster_id             integer NOT NULL,
owner               varchar(30),
jobstatus           integer,
jobprio             integer,
imagesize           numeric(38),
qdate               timestamp(3) with time zone,
remoteusercpu       numeric(38),
remotewallclocktime numeric(38),
cmd                 text,
args                text,
jobuniverse         integer,
primary key(scheddname,cluster_id)
);

CREATE INDEX CA_H_I_owner ON ClusterAds_Horizontal (owner);

CREATE TABLE ProcAds_Horizontal(
scheddname		varchar(4000) NOT NULL,
cluster_id	 	integer NOT NULL,
proc_id 			integer NOT NULL,
jobstatus 		integer,
imagesize 		numeric(38),
remoteusercpu	        numeric(38),
remotewallclocktime 	numeric(38),
remotehost              varchar(4000),
globaljobid        	varchar(4000),
jobprio            	integer,
args                    text,
shadowbday              timestamp(3) with time zone,
enteredcurrentstatus    timestamp(3) with time zone,
numrestarts             integer,
primary key(scheddname,cluster_id,proc_id)
);

CREATE TABLE Jobs_Horizontal_History (
scheddname   varchar(4000) NOT NULL,
scheddbirthdate     integer NOT NULL,
cluster_id              integer NOT NULL,
proc_id                    integer NOT NULL,
qdate                   integer, -- condor_history requires an integer for qdate
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
cmd                     text,
minhosts                integer,
maxhosts                integer,
jobprio                 integer,
user_j                	varchar(4000),
env                     varchar(4000),
userlog                 varchar(4000),
coresize                numeric(38),
killsig                 varchar(4000),
rank                    varchar(4000),
in_j	              	varchar(4000),
transferin              varchar(5),
out                     varchar(4000),
transferout             varchar(5),
err                     varchar(4000),
transfererr             varchar(5),
shouldtransferfiles     varchar(4000),
transferfiles           varchar(4000),
executablesize          numeric(38),
diskusage               integer,
requirements            varchar(4000),
filesystemdomain        varchar(4000),
args                    text,
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
completiondate          integer, -- condor_history requires an integer 
enteredhistorytable     timestamp(3) with time zone
primary key		(scheddname,scheddbirthdate, cluster_id, proc_id)
);

CREATE INDEX Hist_H_I_owner ON Jobs_Horizontal_History (owner);

CREATE TABLE ClusterAds_Vertical (
scheddname	varchar(4000) NOT NULL,
cluster_id		integer NOT NULL,
attr	        varchar(2000) NOT NULL,
val		text,
primary key (scheddname,cluster_id, attr)
);

CREATE TABLE ProcAds_Vertical (
scheddname	varchar(4000) NOT NULL,
cluster_id	integer NOT NULL,
proc_id		integer NOT NULL,
attr	        varchar(2000) NOT NULL,
val		text,
primary key (scheddname,cluster_id, proc_id, attr)
);

CREATE TABLE Jobs_Vertical_History (
scheddname	varchar(4000) NOT NULL,
scheddbirthdate integer NOT NULL,
cluster_id	integer NOT NULL,
proc_id		integer NOT NULL,
attr		varchar(2000) NOT NULL,
val		text,
primary key (scheddname,scheddbirthdate, cluster_id, proc_id, attr)
);

CREATE TABLE GenericMessages (
eventType	varchar(4000),
eventKey	varchar(4000),
eventTime	timestamp(3) with time zone,
eventLoc        varchar(4000),
attName	        varchar(4000),
attValue	text,
attType	varchar(4000)
);

CREATE TABLE Daemons_Vertical (
MyType				VARCHAR(100) NOT NULL,
Name				VARCHAR(500) NOT NULL,
attr				VARCHAR(4000) NOT NULL,
val				text,
LastHeardFrom			TIMESTAMP(3) WITH TIME ZONE,
PRIMARY KEY (MyType, Name, attr)
);

CREATE TABLE Daemons_Vertical_History (
MyType				VARCHAR(100),
Name				VARCHAR(500),
LastHeardFrom			TIMESTAMP(3) WITH TIME ZONE,
attr				VARCHAR(4000),
val				text,
EndTime				TIMESTAMP(3) WITH TIME ZONE
);

CREATE TABLE Schedds_Vertical (
Name				VARCHAR(500) NOT NULL,
attr				VARCHAR(4000) NOT NULL,
val				text,
LastHeardFrom			TIMESTAMP(3) WITH TIME ZONE,
PRIMARY KEY (Name, attr)
);

CREATE TABLE Schedds_Vertical_History (
Name				VARCHAR(500),
LastHeardFrom			TIMESTAMP(3) WITH TIME ZONE,
attr				VARCHAR(4000),
val				text,
EndTime				TIMESTAMP(3) WITH TIME ZONE
);

CREATE TABLE Masters_Vertical (
Name				VARCHAR(500) NOT NULL,
attr				VARCHAR(4000) NOT NULL,
val				text,
LastHeardFrom			TIMESTAMP(3) WITH TIME ZONE,
PRIMARY KEY (Name, attr)
);

CREATE TABLE Masters_Vertical_History (
Name				VARCHAR(500),
LastHeardFrom			TIMESTAMP(3) WITH TIME ZONE,
attr				VARCHAR(4000),
val				text,
EndTime				TIMESTAMP(3) WITH TIME ZONE
);

CREATE TABLE Negotiators_Vertical (
Name				VARCHAR(500) NOT NULL,
attr				VARCHAR(4000) NOT NULL,
val				text,
LastHeardFrom			TIMESTAMP(3) WITH TIME ZONE,
PRIMARY KEY (Name, attr)
);

CREATE TABLE Negotiators_Vertical_History (
Name				VARCHAR(500),
LastHeardFrom			TIMESTAMP(3) WITH TIME ZONE,
attr				VARCHAR(4000),
val				text,
EndTime				TIMESTAMP(3) WITH TIME ZONE
);

CREATE TABLE Error_Sqllogs (
LogName   varchar(100),
Host      varchar(50),
LastModified timestamp(3) with time zone,
ErrorSql  text,
LogBody   text
);

CREATE INDEX Error_Sqllog_idx ON Error_Sqllogs (LogName, Host, LastModified);

CREATE VIEW AGG_User_Jobs_Waiting AS
  SELECT c.owner, count(*) AS jobs_waiting
    FROM clusterads_horizontal c, procads_horizontal p
    WHERE c.cluster_id = p.cluster_id
      AND (p.jobstatus IS NULL OR p.jobstatus = 0 OR p.jobstatus = 1)
    GROUP BY c.owner; 

CREATE VIEW AGG_User_Jobs_Held AS
  SELECT c.owner, count(*) as jobs_held
    FROM clusterads_horizontal c, procads_horizontal p
    WHERE c.cluster_id = p.cluster_id
      AND (p.jobstatus=5)
    GROUP BY c.owner;

CREATE VIEW AGG_User_Jobs_Running AS
  SELECT c.owner, count(*) as jobs_running
    FROM clusterads_horizontal c, procads_horizontal p
    WHERE c.cluster_id = p.cluster_id
      AND (p.jobstatus=2)
    GROUP BY c.owner;

CREATE VIEW AGG_User_Jobs_Fin_Last_Day AS
  SELECT h.owner, count(*) as jobs_completed 
    FROM jobs_horizontal_history h 
    WHERE h.jobstatus = 4 
      AND h.completiondate >= (current_timestamp - interval '24 hour')
    GROUP BY h.owner;

-- Jobs that have historically flocked in to this pool for execution
-- (an anti-join between machine_classad history and jobs)
CREATE VIEW HISTORY_JOBS_FLOCKED_IN AS 
SELECT DISTINCT globaljobid
FROM machines_horizontal_history 
WHERE SUBSTRING(globaljobid FROM 1 FOR (POSITION('#' IN globaljobid)-1)) 
      NOT IN (SELECT DISTINCT scheddname 
              FROM jobs_horizontal_history UNION 
              SELECT DISTINCT scheddname 
              FROM clusterads_horizontal);

-- Jobs that are currently flocking in to this pool for execution 
-- (an anti-join between machine_classad and jobs)
CREATE VIEW CURRENT_JOBS_FLOCKED_IN AS 
SELECT DISTINCT globaljobid 
FROM machines_horizontal
WHERE SUBSTRING(globaljobid FROM 1 FOR (POSITION('#' IN globaljobid)-1)) 
      NOT IN (SELECT DISTINCT scheddname 
              FROM jobs_horizontal_history UNION 
              SELECT DISTINCT scheddname 
              FROM clusterads_horizontal);

-- Jobs that have historically flocked out to another pool for execution
-- (an anti-join between runs table and machine_classad)
-- The predicate "R.machine_id != R.scheddname" is added because some
-- jobs are executed locally on the schedd machine even if it's not 
-- a normal executing host.
CREATE VIEW History_JOBS_FLOCKED_OUT AS
SELECT DISTINCT scheddname, cluster_id, proc_id
FROM runs R 
WHERE R.endts IS NOT NULL AND
   R.machine_id != R.scheddname AND
   R.machine_id NOT IN 
  (SELECT DISTINCT substring(M.machine_id from (position('@' in M.machine_id)+1)) FROM machines_horizontal M);

-- Jobs that are currently flocking out to another pool for execution
-- (an anti-join between runs table and machine_classad)
-- machines must have reported less than 10 minutes ago to be counted
-- toward this pool.
CREATE VIEW CURRENT_JOBS_FLOCKED_OUT AS
SELECT DISTINCT R.scheddname, R.cluster_id, R.proc_id
FROM runs R, clusterads_horizontal C 
WHERE R.endts IS NULL AND
   R.machine_id != R.scheddname AND
   R.machine_id NOT IN 
  (SELECT DISTINCT substring(M.machine_id from (position('@' in M.machine_id)+1)) FROM machines_horizontal M where M.lastheardfrom >= now() - interval '10 minutes') AND R.scheddname = C.scheddname AND R.cluster_id = C.cluster_id;

/*
quill_purgeHistory for PostgresSQL database.

quill_purgeHistory does the following:
1. purge resource history data (e.g. machine history) that are older than 
   resourceHistoryDuration days

2. purge job run history data (e.g. runs, matchs, ...) that are older than 
   runHistoryDuration days

3. purge job history data that are older than 
   jobHistoryDuration days

4. check total size of all tables and see if it's bigger than 
   75% of quillDBSpaceLimit and if so, update the flag spaceShortageWarning
   to true and set the exact percentage of usage.

-- resource history data: no need to keep them for long
--   machine_history, machine_classad_history, 
--   daemon_horizontal_history, daemon_vertical_history, 
--   schedd_horizontal_history, schedd_vertical_history
--   master_vertical_history, negotiator_vertical_history

-- job run history data: purge when they are very old
--   transfers, fileusages, files, runs, events, rejects, matches

-- important job history data should be kept as long as possible
--   history_vertical, history_horizontal, thrown (log thrown events)

-- never purge current "operational data": 
--   machine, machine_classad, clusterads_horizontal, procads_horizontal, 
--   clusterads_vertical, procads_vertical, thrown, daemon_horizontal
--   daemon_vertical, schedd_horizontal, schedd_vertical, master_vertical
--   negotiator_vertical

-- resourceHistoryDuration, runHistoryDuration, jobHistoryDuration 
-- parameters are all in number of days
-- quillDBSpaceLimit parameter is in number of gigabytes

*/

-- dbsize is in unit of megabytes
DROP TABLE quillDBMonitor;
CREATE TABLE quillDBMonitor (
dbsize    integer
);

DELETE FROM quillDBMonitor;
INSERT INTO quillDBMonitor (dbsize) VALUES (0);

DROP TABLE History_Jobs_To_Purge;
CREATE TABLE History_Jobs_To_Purge(
scheddname   varchar(4000),
cluster_id   integer, 
proc_id         integer,
globaljobid  varchar(4000));

CREATE OR REPLACE FUNCTION
quill_purgeHistory(
resourceHistoryDuration integer,
runHistoryDuration integer,
jobHistoryDuration integer) RETURNS void AS $$
DECLARE
totalUsedMB NUMERIC;
BEGIN

/* first purge resource history data */

-- purge machine vertical attributes older than resourceHistoryDuration days
DELETE FROM machines_vertical_history
WHERE start_time < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

-- purge machine classads older than resourceHistoryDuration days
DELETE FROM machines_horizontal_history
WHERE lastheardfrom < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

-- purge daemon vertical attributes older than certain days
DELETE FROM daemons_vertical_history
WHERE lastheardfrom < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

-- purge daemon classads older than certain days
DELETE FROM daemons_horizontal_history
WHERE lastheardfrom < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

-- purge schedd vertical attributes older than certain days
DELETE FROM schedds_vertical_history
WHERE lastheardfrom < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

-- purge schedd classads older than certain days
DELETE FROM schedds_horizontal_history
WHERE lastheardfrom < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

-- purge master vertical attributes older than certain days
DELETE FROM masters_vertical_history
WHERE lastheardfrom < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

-- purge negotiator vertical attributes older than certain days
DELETE FROM negotiators_vertical_history
WHERE lastheardfrom < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

/* second purge job run history data */

-- find the set of jobs for which the run history are going to be purged
INSERT INTO History_Jobs_To_Purge 
SELECT scheddname, cluster_id, proc_id, globaljobid
FROM Jobs_Horizontal_History
WHERE enteredhistorytable < 
      (current_timestamp - 
       cast (runHistoryDuration || ' day' as interval));

-- purge transfers data related to jobs older than certain days
DELETE FROM transfers 
WHERE globaljobid IN (SELECT globaljobid 
                      FROM History_Jobs_To_Purge);

-- purge fileusages related to jobs older than certain days
DELETE FROM fileusages
WHERE globaljobid IN (SELECT globaljobid 
                      FROM History_Jobs_To_Purge);

-- purge files that are not referenced any more
DELETE FROM files 
WHERE NOT EXISTS (SELECT *
                  FROM fileusages 
                  WHERE fileusages.file_id = files.file_id);

-- purge run data for jobs older than certain days
DELETE FROM Runs 
WHERE exists (SELECT * 
              FROM History_Jobs_To_Purge AS H
              WHERE H.scheddname = Runs.scheddname AND
                    H.cluster_id = Runs.cluster_id AND
                    H.proc_id = Runs.proc_id);

-- purge rejects data for jobs older than certain days
DELETE FROM Rejects
WHERE exists (SELECT * 
              FROM History_Jobs_To_Purge AS H
              WHERE H.scheddname = Rejects.scheddname AND
                    H.cluster_id = Rejects.cluster_id AND
                    H.proc_id = Rejects.proc_id);

-- purge matches data for jobs older than certain days
DELETE FROM Matches
WHERE exists (SELECT * 
              FROM History_Jobs_To_Purge AS H
              WHERE H.scheddname = Matches.scheddname AND
                    H.cluster_id = Matches.cluster_id AND
                    H.proc_id = Matches.proc_id);

-- purge events data for jobs older than certain days
DELETE FROM Events
WHERE exists (SELECT * 
              FROM History_Jobs_To_Purge AS H
              WHERE H.scheddname = Events.scheddname AND
                    H.cluster_id = Events.cluster_id AND
                    H.proc_id = Events.proc_id);

TRUNCATE TABLE History_Jobs_To_Purge;

/* third purge job history data */

-- find the set of jobs for which history data are to be purged
INSERT INTO History_Jobs_To_Purge 
SELECT scheddname, cluster_id, proc_id, globaljobid
FROM Jobs_Horizontal_History
WHERE enteredhistorytable < 
      (current_timestamp - 
       cast (jobHistoryDuration || ' day' as interval));

-- purge vertical attributes for jobs older than certain days
DELETE FROM Jobs_Vertical_History
WHERE exists (SELECT * 
              FROM History_Jobs_To_Purge AS H
              WHERE H.scheddname = Jobs_Vertical_History.scheddname AND
                    H.cluster_id = Jobs_Vertical_History.cluster_id AND
                    H.proc_id = Jobs_Vertical_History.proc_id);

-- purge classads for jobs older than certain days
DELETE FROM Jobs_Horizontal_History
WHERE Jobs_Horizontal_History.globaljobid IN (SELECT globaljobid 
                        FROM History_Jobs_To_Purge);


-- purge log thrown events older than jobHistoryDuration
-- The thrown table doesn't fall precisely into any of the categories 
-- but we don't want the table to grow unbounded either.
DELETE FROM throwns
WHERE throwns.throwTime < 
     (current_timestamp - 
       cast (jobHistoryDuration || ' day' as interval));

TRUNCATE TABLE History_Jobs_To_Purge;

/* lastly check if db size is above 75 percentage of specified limit */
-- one caveat: index size is not counted in the usage calculation
-- analyze tables first to have correct statistics 

analyze RUNS;
analyze REJECTS;
analyze MATCHES;
analyze L_JOBSTATUS;
analyze THROWNs;
analyze EVENTS;
analyze L_EVENTTYPE;
analyze GENERICMessages;
analyze JOBQUEUEPOLLINGINFO;
analyze CURRENCIES;
analyze DAEMONS_VERTICAL;
analyze DAEMONS_HORIZONTAL_HISTORY;
analyze DAEMONS_VERTICAL_HISTORY;
analyze SCHEDDS_HORIZONTAL;
analyze SCHEDDS_HORIZONTAL_HISTORY;
analyze SCHEDDS_VERTICAL;
analyze SCHEDDS_VERTICAL_HISTORY;
analyze MASTERS_VERTICAL;
analyze MASTERS_VERTICAL_HISTORY;
analyze NEGOTIATORS_VERTICAL;
analyze NEGOTIATORS_VERTICAL_HISTORY;
analyze DUMMY_SINGLE_ROW_TABLE;
analyze CDB_USERS;
analyze TRANSFERS;
analyze FILES;
analyze FILEUSAGES;
analyze MACHINES_VERTICAL;
analyze MACHINES_VERTICAL_HISTORY;
analyze MACHINES_HORIZONTAL_HISTORY;
analyze CLUSTERADS_HORIZONTAL;
analyze PROCADS_HORIZONTAL;
analyze CLUSTERADS_VERTICAL;
analyze PROCADS_VERTICAL;
analyze JOBS_VERTICAL_HISTORY;
analyze JOBS_HORIZONTAL_HISTORY;
analyze MACHINES_HORIZONTAL;
analyze DAEMONS_HORIZONTAL;
analyze HISTORY_JOBS_TO_PURGE;

SELECT ROUND(SUM(relpages)*8192/(1024*1024)) INTO totalUsedMB
FROM pg_class
WHERE relname IN ('procads_vertical', 'jobs_vertical_history', 'clusterads_vertical', 'procads_horizontal', 'clusterads_horizontal', 'jobs_horizontal_history', 'files', 'fileusages', 'schedds_vertical', 'schedds_horizontal', 'runs', 'masters_vertical', 'machines_vertical', 'machines_horizontal', 'daemons_vertical', 'daemons_horizontal', 'transfers', 'schedds_vertical_history', 'schedds_horizontal_history', 'rejects', 'negotiators_vertical_history', 'matches', 'masters_vertical_history', 'machines_vertical_history', 'machines_horizontal_history', 'events', 'daemons_vertical_history', 'daemons_horizontal_history');

RAISE NOTICE 'totalUsedMB=% MegaBytes', totalUsedMB;

UPDATE quillDBMonitor SET dbsize = totalUsedMB;

END;
$$ LANGUAGE plpgsql;

-- grant read access on relevant tables to quillreader
grant select on cdb_users to quillreader;
grant select on transfers to quillreader;
grant select on files to quillreader;
grant select on fileusages to quillreader;
grant select on machines_vertical to quillreader;
grant select on machines_vertical_history to quillreader;
grant select on machines_horizontal_history to quillreader;
grant select on ClusterAds_Horizontal to quillreader;
grant select on ProcAds_Horizontal to quillreader;
grant select on ClusterAds_Vertical to quillreader;
grant select on ProcAds_Vertical to quillreader;
grant select on Jobs_Vertical_History to quillreader;
grant select on Jobs_Horizontal_History to quillreader;
grant select on Runs to quillreader;
grant select on Rejects to quillreader;
grant select on Matches to quillreader;
grant select on AGG_User_Jobs_Waiting to quillreader;
grant select on AGG_User_Jobs_Held to quillreader;
grant select on AGG_User_Jobs_Running to quillreader;
grant select on L_Jobstatus to quillreader;
grant select on throwns to quillreader;
grant select on events to quillreader;
grant select on L_eventType to quillreader;
grant select on GenericMessages to quillreader;
grant select on JobQueuePollingInfo to quillreader;
grant select on Currencies to quillreader;
grant select on Daemons_Horizontal to quillreader;
grant select on Daemons_Vertical to quillreader;
grant select on Daemons_Horizontal_History to quillreader;
grant select on Daemons_Vertical_History to quillreader;
grant select on Schedds_Horizontal to quillreader;
grant select on Schedds_Horizontal_History to quillreader;
grant select on Schedds_Vertical to quillreader;
grant select on Schedds_Vertical_History to quillreader;
grant select on Masters_Vertical to quillreader;
grant select on Masters_Vertical_History to quillreader;
grant select on Negotiators_Vertical to quillreader;
grant select on Negotiators_Vertical_History to quillreader;
grant select on Error_Sqllogs to quillreader;
grant select on AGG_User_Jobs_Fin_Last_Day to quillreader;
grant select on HISTORY_JOBS_FLOCKED_IN to quillreader;
grant select on CURRENT_JOBS_FLOCKED_IN to quillreader;
grant select on History_JOBS_FLOCKED_OUT to quillreader;
grant select on CURRENT_JOBS_FLOCKED_OUT to quillreader;

-- the creation of the schema version table should be the last step 
-- because it is used by quill daemon to decide whether we have the 
-- right schema objects for it to operate correctly
CREATE TABLE quill_schema_version (
major int, 
minor int, 
back_to_major int, 
back_to_minor int);

grant select on quill_schema_version to quillreader;

DELETE FROM quill_schema_version;
INSERT INTO quill_schema_version (major, minor, back_to_major, back_to_minor) VALUES (2,0,2,0);

