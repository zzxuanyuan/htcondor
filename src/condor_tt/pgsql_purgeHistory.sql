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
