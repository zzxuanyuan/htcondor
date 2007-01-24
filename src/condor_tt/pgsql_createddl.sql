/*
Before installing this script, the following must have been prepared
1. quillreader account has been created
2. pl/pgSQL language has been created with "createlang plpgsql [dbname]"
*/


CREATE TABLE Error_Sqllog (
LogName   varchar(100),
Host      varchar(50),
LastModified timestamp(3) with time zone,
ErrorSql  text,
LogBody   text
);

CREATE INDEX Error_Sqllog_idx ON Error_Sqllog (LogName, Host, LastModified);

CREATE VIEW AGG_User_Jobs_Fin_Last_Day AS
  SELECT h.owner, count(*) as jobs_completed 
    FROM history_horizontal h 
    WHERE h.jobstatus = 4 
      AND h.completiondate >= (current_timestamp - interval '24 hour')
    GROUP BY h.owner;

-- Jobs that have historically flocked in to this pool for execution
-- (an anti-join between machine_classad history and jobs)
CREATE VIEW HISTORY_JOBS_FLOCKED_IN AS 
SELECT DISTINCT globaljobid
FROM machine_classad_history 
WHERE SUBSTRING(globaljobid FROM 1 FOR (POSITION('#' IN globaljobid)-1)) 
      NOT IN (SELECT DISTINCT scheddname 
              FROM history_horizontal UNION 
              SELECT DISTINCT scheddname 
              FROM clusterads_horizontal);

-- Jobs that are currently flocking in to this pool for execution 
-- (an anti-join between machine_classad and jobs)
CREATE VIEW CURRENT_JOBS_FLOCKED_IN AS 
SELECT DISTINCT globaljobid 
FROM machine_classad
WHERE SUBSTRING(globaljobid FROM 1 FOR (POSITION('#' IN globaljobid)-1)) 
      NOT IN (SELECT DISTINCT scheddname 
              FROM history_horizontal UNION 
              SELECT DISTINCT scheddname 
              FROM clusterads_horizontal);

-- Jobs that have historically flocked out to another pool for execution
-- (an anti-join between runs table and machine_classad)
-- The predicate "R.machine_id != R.scheddname" is added because some
-- jobs are executed locally on the schedd machine even if it's not 
-- a normal executing host.
CREATE VIEW History_JOBS_FLOCKED_OUT AS
SELECT DISTINCT scheddname, cluster_id, proc
FROM runs R 
WHERE R.endts IS NOT NULL AND
   R.machine_id != R.scheddname AND
   R.machine_id NOT IN 
  (SELECT DISTINCT substring(M.machine_id from (position('@' in M.machine_id)+1)) FROM machine_classad M);

-- Jobs that are currently flocking out to another pool for execution
-- (an anti-join between runs table and machine_classad)
-- machines must have reported less than 10 minutes ago to be counted
-- toward this pool.
CREATE VIEW CURRENT_JOBS_FLOCKED_OUT AS
SELECT DISTINCT R.scheddname, R.cluster_id, R.proc
FROM runs R, clusterads_horizontal C 
WHERE R.endts IS NULL AND
   R.machine_id != R.scheddname AND
   R.machine_id NOT IN 
  (SELECT DISTINCT substring(M.machine_id from (position('@' in M.machine_id)+1)) FROM machine_classad M where M.lastheardfrom >= now() - interval '10 minutes') AND R.scheddname = C.scheddname AND R.cluster_id = C.cluster_id;

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
proc         integer,
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
DELETE FROM machine_history
WHERE start_time < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

-- purge machine classads older than resourceHistoryDuration days
DELETE FROM machine_classad_history
WHERE lastheardfrom < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

-- purge daemon vertical attributes older than certain days
DELETE FROM daemon_vertical_history
WHERE lastheardfrom < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

-- purge daemon classads older than certain days
DELETE FROM daemon_horizontal_history
WHERE lastheardfrom < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

-- purge schedd vertical attributes older than certain days
DELETE FROM schedd_vertical_history
WHERE lastheardfrom < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

-- purge schedd classads older than certain days
DELETE FROM schedd_horizontal_history
WHERE lastheardfrom < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

-- purge master vertical attributes older than certain days
DELETE FROM master_vertical_history
WHERE lastheardfrom < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

-- purge negotiator vertical attributes older than certain days
DELETE FROM negotiator_vertical_history
WHERE lastheardfrom < 
      (current_timestamp - 
       cast (resourceHistoryDuration || ' day' as interval));

/* second purge job run history data */

-- find the set of jobs for which the run history are going to be purged
INSERT INTO History_Jobs_To_Purge 
SELECT scheddname, cluster_id, proc, globaljobid
FROM History_Horizontal
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
                    H.proc = Runs.proc);

-- purge rejects data for jobs older than certain days
DELETE FROM Rejects
WHERE exists (SELECT * 
              FROM History_Jobs_To_Purge AS H
              WHERE H.scheddname = Rejects.scheddname AND
                    H.cluster_id = Rejects.cluster_id AND
                    H.proc = Rejects.proc);

-- purge matches data for jobs older than certain days
DELETE FROM Matches
WHERE exists (SELECT * 
              FROM History_Jobs_To_Purge AS H
              WHERE H.scheddname = Matches.scheddname AND
                    H.cluster_id = Matches.cluster_id AND
                    H.proc = Matches.proc);

-- purge events data for jobs older than certain days
DELETE FROM Events
WHERE exists (SELECT * 
              FROM History_Jobs_To_Purge AS H
              WHERE H.scheddname = Events.scheddname AND
                    H.cluster_id = Events.cluster_id AND
                    H.proc = Events.proc);

TRUNCATE TABLE History_Jobs_To_Purge;

/* third purge job history data */

-- find the set of jobs for which history data are to be purged
INSERT INTO History_Jobs_To_Purge 
SELECT scheddname, cluster_id, proc, globaljobid
FROM History_Horizontal
WHERE enteredhistorytable < 
      (current_timestamp - 
       cast (jobHistoryDuration || ' day' as interval));

-- purge vertical attributes for jobs older than certain days
DELETE FROM History_Vertical
WHERE exists (SELECT * 
              FROM History_Jobs_To_Purge AS H
              WHERE H.scheddname = History_Vertical.scheddname AND
                    H.cluster_id = History_Vertical.cluster_id AND
                    H.proc = History_Vertical.proc);

-- purge classads for jobs older than certain days
DELETE FROM History_Horizontal
WHERE History_Horizontal.globaljobid IN (SELECT globaljobid 
                        FROM History_Jobs_To_Purge);


-- purge log thrown events older than jobHistoryDuration
-- The thrown table doesn't fall precisely into any of the categories 
-- but we don't want the table to grow unbounded either.
DELETE FROM thrown
WHERE thrown.throwTime < 
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
analyze THROWN;
analyze EVENTS;
analyze L_EVENTTYPE;
analyze GENERIC;
analyze JOBQUEUEPOLLINGINFO;
analyze CURRENCY;
analyze DAEMON_VERTICAL;
analyze DAEMON_HORIZONTAL_HISTORY;
analyze DAEMON_VERTICAL_HISTORY;
analyze SCHEDD_HORIZONTAL;
analyze SCHEDD_HORIZONTAL_HISTORY;
analyze SCHEDD_VERTICAL;
analyze SCHEDD_VERTICAL_HISTORY;
analyze MASTER_VERTICAL;
analyze MASTER_VERTICAL_HISTORY;
analyze NEGOTIATOR_VERTICAL;
analyze NEGOTIATOR_VERTICAL_HISTORY;
analyze DUMMY_SINGLE_ROW_TABLE;
analyze CDB_USERS;
analyze TRANSFERS;
analyze FILES;
analyze FILEUSAGES;
analyze MACHINE;
analyze MACHINE_HISTORY;
analyze MACHINE_CLASSAD_HISTORY;
analyze CLUSTERADS_HORIZONTAL;
analyze PROCADS_HORIZONTAL;
analyze CLUSTERADS_VERTICAL;
analyze PROCADS_VERTICAL;
analyze HISTORY_VERTICAL;
analyze HISTORY_HORIZONTAL;
analyze MACHINE_CLASSAD;
analyze DAEMON_HORIZONTAL;
analyze HISTORY_JOBS_TO_PURGE;

SELECT SUM(relpages)*8192/(1024*1024) INTO totalUsedMB
FROM pg_class
WHERE relname IN ('procads_vertical', 'history_vertical', 'clusterads_vertical', 'procads_horizontal', 'clusterads_horizontal', 'history_horizontal', 'files', 'fileusages', 'schedd_vertical', 'schedd_horizontal', 'runs', 'master_vertical', 'machine', 'machine_classad', 'daemon_vertical', 'daemon_horizontal', 'transfers', 'schedd_vertical_history', 'schedd_horizontal_history', 'rejects', 'negotiator_vertical_history', 'matches', 'master_vertical_history', 'machine_history', 'machine_classad_history', 'events', 'daemon_vertical_history', 'daemon_horizontal_history');

RAISE NOTICE 'totalUsedMB=% MegaBytes', totalUsedMB;

UPDATE quillDBMonitor SET dbsize = totalUsedMB;

END;
$$ LANGUAGE plpgsql;

-- grant read access on relevant tables to quillreader
grant select on cdb_users to quillreader;
grant select on transfers to quillreader;
grant select on files to quillreader;
grant select on fileusages to quillreader;
grant select on machine to quillreader;
grant select on machine_history to quillreader;
grant select on machine_classad_history to quillreader;
grant select on ClusterAds_Horizontal to quillreader;
grant select on ProcAds_Horizontal to quillreader;
grant select on ClusterAds_Vertical to quillreader;
grant select on ProcAds_Vertical to quillreader;
grant select on History_Vertical to quillreader;
grant select on History_Horizontal to quillreader;
grant select on Runs to quillreader;
grant select on Rejects to quillreader;
grant select on Matches to quillreader;
grant select on AGG_User_Jobs_Waiting to quillreader;
grant select on AGG_User_Jobs_Held to quillreader;
grant select on AGG_User_Jobs_Running to quillreader;
grant select on L_Jobstatus to quillreader;
grant select on thrown to quillreader;
grant select on events to quillreader;
grant select on L_eventType to quillreader;
grant select on Generic to quillreader;
grant select on JobQueuePollingInfo to quillreader;
grant select on Currency to quillreader;
grant select on Daemon_Horizontal to quillreader;
grant select on Daemon_Vertical to quillreader;
grant select on Daemon_Horizontal_History to quillreader;
grant select on Daemon_Vertical_History to quillreader;
grant select on Schedd_Horizontal to quillreader;
grant select on Schedd_Horizontal_History to quillreader;
grant select on Schedd_Vertical to quillreader;
grant select on Schedd_Vertical_History to quillreader;
grant select on Master_Vertical to quillreader;
grant select on Master_Vertical_History to quillreader;
grant select on Negotiator_Vertical to quillreader;
grant select on Negotiator_Vertical_History to quillreader;
grant select on Error_Sqllog to quillreader;
grant select on AGG_User_Jobs_Fin_Last_Day to quillreader;
grant select on HISTORY_JOBS_FLOCKED_IN to quillreader;
grant select on CURRENT_JOBS_FLOCKED_IN to quillreader;
grant select on History_JOBS_FLOCKED_OUT to quillreader;
grant select on CURRENT_JOBS_FLOCKED_OUT to quillreader;
grant select on quill_schema_version to quillreader;

-- the creation of the schema version table should be the last step 
-- because it is used by quill daemon to decide whether we have the 
-- right schema objects for it to operate correctly
CREATE TABLE quill_schema_version (
major int, 
minor int, 
back_to_major int, 
back_to_minor int);

INSERT INTO quill_schema_version (major, minor, back_to_major, back_to_minor) VALUES (2,0,2,0);
