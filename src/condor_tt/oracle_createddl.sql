/*
Before installing this script, the following must have been prepared
1. quillreader account has been created
*/


CREATE TABLE Error_Sqllogs (
LogName   varchar(100),
Host      varchar(50),
LastModified timestamp(3) with time zone,
ErrorSql  varchar(4000),
LogBody   clob
);

CREATE INDEX Error_Sqllog_idx ON Error_Sqllogs (LogName, Host, LastModified);

CREATE VIEW AGG_User_Jobs_Fin_Last_Day AS
  SELECT h.owner, count(*) as jobs_completed 
    FROM jobs_horizontal_history h 
    WHERE h.jobstatus = 4 
      AND (to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD') + to_dsinterval(floor(h.completiondate/86400) || ' ' || floor(mod(h.completiondate,86400)/3600) || ':' || floor(mod(h.completiondate, 3600)/60) || ':' || mod(h.completiondate, 60))) >= (current_timestamp - to_dsinterval('1 00:00:00'))
    GROUP BY h.owner;

-- Jobs that have historically flocked in to this pool for execution
-- (an anti-join between machine_classad history and jobs)
CREATE VIEW HISTORY_JOBS_FLOCKED_IN AS 
SELECT DISTINCT globaljobid
FROM machines_horizontal_history 
WHERE SUBSTR(globaljobid, 1, (INSTR('#', globaljobid)-1)) 
      NOT IN (SELECT DISTINCT scheddname 
              FROM jobs_horizontal_history UNION 
              SELECT DISTINCT scheddname 
              FROM clusterads_horizontal);

-- Jobs that are currently flocking in to this pool for execution 
-- (an anti-join between machine_classad and jobs)
CREATE VIEW CURRENT_JOBS_FLOCKED_IN AS 
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
CREATE VIEW History_JOBS_FLOCKED_OUT AS
SELECT DISTINCT scheddname, cluster_id, proc
FROM runs R 
WHERE R.endts IS NOT NULL AND
   R.machine_id != R.scheddname AND
   R.machine_id NOT IN 
  (SELECT DISTINCT SUBSTR(M.machine_id, (INSTR('@', M.machine_id)+1)) FROM machines_horizontal M);

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
  (SELECT DISTINCT SUBSTR(M.machine_id, (INSTR('@', M.machine_id)+1)) FROM machines_horizontal M where M.lastheardfrom >= current_timestamp - to_dsinterval('0 00:10:00'))  AND R.scheddname = C.scheddname AND R.cluster_id = C.cluster_id;

/*
quill_purgeHistory for Oracle database.

quill_purgeHistory does the following:
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

*/

--SET SERVEROUTPUT ON;

-- dbsize is in unit of megabytes
DROP TABLE quillDBMonitor;
CREATE TABLE quillDBMonitor (
dbsize    integer
);

DELETE FROM quillDBMonitor;
INSERT INTO quillDBMonitor (dbsize) VALUES (0);

DROP TABLE History_Jobs_To_Purge;
CREATE GLOBAL TEMPORARY TABLE History_Jobs_To_Purge(
scheddname   varchar(4000),
cluster_id   integer, 
proc         integer,
globaljobid  varchar(4000)) ON COMMIT DELETE ROWS;

CREATE OR REPLACE PROCEDURE 
quill_purgeHistory(
resourceHistoryDuration integer,
runHistoryDuration integer,
jobHistoryDuration integer) AS 
totalUsedMB NUMBER;
BEGIN

/* first purge resource history data */

-- purge machine vertical attributes older than resourceHistoryDuration days
DELETE FROM machines_vertical_history
WHERE start_time < 
      (current_timestamp - 
       to_dsinterval(resourceHistoryDuration || ' 00:00:00'));

-- purge machine classads older than resourceHistoryDuration days
DELETE FROM machines_horizontal_history
WHERE lastheardfrom < 
      (current_timestamp - 
       to_dsinterval(resourceHistoryDuration || ' 00:00:00'));

-- purge daemon vertical attributes older than certain days
DELETE FROM daemons_vertical_history
WHERE lastheardfrom < 
      (current_timestamp - 
       to_dsinterval(resourceHistoryDuration || ' 00:00:00'));

-- purge daemon classads older than certain days
DELETE FROM daemons_horizontal_history
WHERE lastheardfrom < 
      (current_timestamp - 
       to_dsinterval(resourceHistoryDuration || ' 00:00:00'));

-- purge schedd vertical attributes older than certain days
DELETE FROM schedds_vertical_history
WHERE lastheardfrom < 
      (current_timestamp - 
       to_dsinterval(resourceHistoryDuration || ' 00:00:00'));

-- purge schedd classads older than certain days
DELETE FROM schedds_horizontal_history
WHERE lastheardfrom < 
      (current_timestamp - 
       to_dsinterval(resourceHistoryDuration || ' 00:00:00'));

-- purge master vertical attributes older than certain days
DELETE FROM masters_vertical_history
WHERE lastheardfrom < 
      (current_timestamp - 
       to_dsinterval(resourceHistoryDuration || ' 00:00:00'));

-- purge negotiator vertical attributes older than certain days
DELETE FROM negotiators_vertical_history
WHERE lastheardfrom < 
      (current_timestamp - 
       to_dsinterval(resourceHistoryDuration || ' 00:00:00'));

COMMIT;

/* second purge job run history data */

-- find the set of jobs for which the run history are going to be purged
INSERT INTO History_Jobs_To_Purge 
SELECT scheddname, cluster_id, proc, globaljobid
FROM Jobs_Horizontal_History
WHERE enteredhistorytable < 
      (current_timestamp - 
       to_dsinterval(runHistoryDuration || ' 00:00:00'));

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
DELETE FROM Runs R
WHERE exists (SELECT * 
              FROM History_Jobs_To_Purge H
              WHERE H.scheddname = R.scheddname AND
                    H.cluster_id = R.cluster_id AND
                    H.proc = R.proc);

-- purge rejects data for jobs older than certain days
DELETE FROM Rejects R
WHERE exists (SELECT * 
              FROM History_Jobs_To_Purge H
              WHERE H.scheddname = R.scheddname AND
                    H.cluster_id = R.cluster_id AND
                    H.proc = R.proc);

-- purge matches data for jobs older than certain days
DELETE FROM matches M
WHERE exists (SELECT * 
              FROM History_Jobs_To_Purge H
              WHERE H.scheddname = M.scheddname AND
                    H.cluster_id = M.cluster_id AND
                    H.proc = M.proc);

-- purge events data for jobs older than certain days
DELETE FROM events E
WHERE exists (SELECT * 
              FROM History_Jobs_To_Purge H
              WHERE H.scheddname = E.scheddname AND
                    H.cluster_id = E.cluster_id AND
                    H.proc = E.proc);

COMMIT; -- commit will truncate the temporary table History_Jobs_To_Purge

/* third purge job history data */

-- find the set of jobs for which history data are to be purged
INSERT INTO History_Jobs_To_Purge 
SELECT scheddname, cluster_id, proc, globaljobid
FROM Jobs_Horizontal_History
WHERE enteredhistorytable < 
      (current_timestamp - 
       to_dsinterval(jobHistoryDuration || ' 00:00:00'));

-- purge vertical attributes for jobs older than certain days
DELETE FROM Jobs_Vertical_History V
WHERE exists (SELECT * 
              FROM History_Jobs_To_Purge H
              WHERE H.scheddname = V.scheddname AND
                    H.cluster_id = V.cluster_id AND
                    H.proc = V.proc);

-- purge classads for jobs older than certain days
DELETE FROM Jobs_Horizontal_History H
WHERE H.globaljobid IN (SELECT globaljobid 
                        FROM History_Jobs_To_Purge);


-- purge log thrown events older than jobHistoryDuration
-- The thrown table doesn't fall precisely into any of the categories 
-- but we don't want the table to grow unbounded either.
DELETE FROM throwns T
WHERE T.throwTime < 
     (current_timestamp - 
      to_dsinterval(jobHistoryDuration || ' 00:00:00'));

COMMIT;

/* lastly check if db size is above 75 percentage of specified limit */
-- one caveat: index size is not counted in the usage calculation
-- analyze tables first to have correct statistics 

execute immediate 'analyze table RUNS  compute statistics';
execute immediate 'analyze table REJECTS compute statistics';
execute immediate 'analyze table MATCHES compute statistics';
execute immediate 'analyze table L_JOBSTATUS compute statistics';
execute immediate 'analyze table THROWNs compute statistics';
execute immediate 'analyze table EVENTS compute statistics';
execute immediate 'analyze table L_EVENTTYPE compute statistics';
execute immediate 'analyze table GENERICMESSAGES compute statistics';
execute immediate 'analyze table JOBQUEUEPOLLINGINFO compute statistics';
execute immediate 'analyze table CURRENCIES compute statistics';
execute immediate 'analyze table DAEMONS_VERTICAL compute statistics';
execute immediate 'analyze table DAEMONS_HORIZONTAL_HISTORY compute statistics';
execute immediate 'analyze table DAEMONS_VERTICAL_HISTORY compute statistics';
execute immediate 'analyze table SCHEDDS_HORIZONTAL compute statistics';
execute immediate 'analyze table SCHEDDS_HORIZONTAL_HISTORY compute statistics';
execute immediate 'analyze table SCHEDDS_VERTICAL compute statistics';
execute immediate 'analyze table SCHEDDS_VERTICAL_HISTORY compute statistics';
execute immediate 'analyze table MASTERS_VERTICAL compute statistics';
execute immediate 'analyze table MASTERS_VERTICAL_HISTORY compute statistics';
execute immediate 'analyze table NEGOTIATORS_VERTICAL compute statistics';
execute immediate 'analyze table NEGOTIATORS_VERTICAL_HISTORY compute statistics';
execute immediate 'analyze table DUMMY_SINGLE_ROW_TABLE compute statistics';
execute immediate 'analyze table CDB_USERS compute statistics';
execute immediate 'analyze table TRANSFERS compute statistics';
execute immediate 'analyze table FILES compute statistics';
execute immediate 'analyze table FILEUSAGES compute statistics';
execute immediate 'analyze table MACHINES_VERTICAL compute statistics';
execute immediate 'analyze table MACHINES_VERTICAL_HISTORY compute statistics';
execute immediate 'analyze table MACHINES_VERTICAL_HISTORY compute statistics';
execute immediate 'analyze table CLUSTERADS_HORIZONTAL compute statistics';
execute immediate 'analyze table PROCADS_HORIZONTAL compute statistics';
execute immediate 'analyze table CLUSTERADS_VERTICAL compute statistics';
execute immediate 'analyze table PROCADS_VERTICAL compute statistics';
execute immediate 'analyze table JOBS_VERTICAL_HISTORY compute statistics';
execute immediate 'analyze table JOBS_HORIZONTAL_HISTORY compute statistics';
execute immediate 'analyze table MACHINES_HORIZONTAL compute statistics';
execute immediate 'analyze table DAEMONS_HORIZONTAL compute statistics';
execute immediate 'analyze table HISTORY_JOBS_TO_PURGE compute statistics';

SELECT ROUND(SUM(NUM_ROWS*AVG_ROW_LEN)/(1024*1024)) INTO totalUsedMB
FROM USER_TABLES;

DBMS_OUTPUT.PUT_LINE('totalUsedMB=' || totalUsedMB || ' MegaBytes');

UPDATE quillDBMonitor SET dbsize = totalUsedMB;

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
