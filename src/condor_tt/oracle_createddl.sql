CREATE TABLE Error_Sqllog (
LogName   varchar(100),
Host      varchar(50),
LastModified timestamp(3) with time zone,
ErrorSql  varchar(4000),
LogBody   clob
);

CREATE INDEX Error_Sqllog_idx ON Error_Sqllog (LogName, Host, LastModified);

CREATE VIEW AGG_User_Jobs_Fin_Last_Day AS
  SELECT h.owner, count(*) as jobs_completed 
    FROM history_horizontal h 
    WHERE h.jobstatus = 4 
      AND (to_timestamp_tz('01/01/1970 UTC', 'MM/DD/YYYY TZD') + to_dsinterval(floor(h.completiondate/86400) || ' ' || floor(mod(h.completiondate,86400)/3600) || ':' || floor(mod(h.completiondate, 3600)/60) || ':' || mod(h.completiondate, 60))) >= (current_timestamp - to_dsinterval('1 00:00:00'))
    GROUP BY h.owner;

-- Jobs that have historically flocked in to this pool for execution
-- (an anti-join between machine_classad history and jobs)
CREATE VIEW HISTORY_JOBS_FLOCKED_IN AS 
SELECT DISTINCT globaljobid
FROM machine_classad_history 
WHERE SUBSTR(globaljobid, 1, (INSTR('#', globaljobid)-1)) 
      NOT IN (SELECT DISTINCT scheddname 
              FROM history_horizontal UNION 
              SELECT DISTINCT scheddname 
              FROM clusterads_horizontal);

-- Jobs that are currently flocking in to this pool for execution 
-- (an anti-join between machine_classad and jobs)
CREATE VIEW CURRENT_JOBS_FLOCKED_IN AS 
SELECT DISTINCT globaljobid 
FROM machine_classad
WHERE SUBSTR(globaljobid, 1, (INSTR('#', globaljobid)-1)) 
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
  (SELECT DISTINCT SUBSTR(M.machine_id, (INSTR('@', M.machine_id)+1)) FROM machine_classad M);

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
  (SELECT DISTINCT SUBSTR(M.machine_id, (INSTR('@', M.machine_id)+1)) FROM machine_classad M where M.lastheardfrom >= current_timestamp - to_dsinterval('0 00:10:00'))  AND R.scheddname = C.scheddname AND R.cluster_id = C.cluster_id;

