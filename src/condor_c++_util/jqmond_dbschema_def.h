#ifndef _JQMOND_SCHEMA_DEF_H_
#define _JQMOND_SCHEMA_DEF_H_

#define SCHEMA_CHECK_STR "SELECT relname FROM pg_class WHERE relname = 'clusterads_vertical' OR relname = 'clusterads_horizontal' OR relname = 'history_vertical' OR relname = 'history_horizontal' OR relname = 'procads_vertical' OR relname = 'procads_horizontal';"

#define SCHEMA_SYS_TABLE_NUM 	6

/* 
	Defintion of Delete String 

	DELETE FROM ProcAds_Str;
	DELETE FROM ProcAds_Num; 
	DELETE FROM ClusterAds_Str;
	DELETE FROM ClusterAds_Num; 
	DELETE FROM History_Str; 
	DELETE FROM History_Num; 
	DELETE FROM JobQueuePollingInfo;
*/

#define SCHEMA_DELETE_STR "DELETE FROM ProcAds_Vertical; DELETE FROM ProcAds_Horizontal; DELETE FROM ClusterAds_Vertical; DELETE FROM ClusterAds_Horizontal; DELETE FROM History_Vertical; DELETE FROM History_Horizontal;"


/*
	Defintion of Drop Table String 

	DROP TABLE ProcAds_Str;
	DROP TABLE ProcAds_Num;
	DROP TABLE ClusterAds_Str;
	DROP TABLE ClusterAds_Num;
	DROP TABLE History_Str;
	DROP TABLE History_Num;
	DROP TABLE JobQueuePollingInfo;
*/

#define SCHEMA_DROP_TABLE_STR "DROP TABLE ProcAds_Horizontal; DROP TABLE ProcAds_Vertical; DROP TABLE ClusterAds_Horizontal; DROP TABLE ClusterAds_Vertical; DROP TABLE History_Vertical; DROP TABLE History_Horizontal;"

/*
	Defintion of Create ProcAds_Vertical Table

        CREATE TABLE ClusterAds_Horizontal(
           cid text,
           owner varchar(50),
           jobstatus integer,
           jobprio integer,
           imagesize integer,
           qdate timestamp(0) with time zone,
           remoteusercpu double precision,
           cmd text,
           args text,
           primary key(cid)
        );
        CREATE TABLE ProcAds_Horizontal(
           cid text,
           pid text,
           jobstatus integer,
           imagesize integer,
           remoteusercpu double precision,
           primary key(cid,pid)
        );

        CREATE TABLE ClusterAds_Vertical (
           cid          text,
           attr         text,
           val          text,
           primary key (cid, attr)
        );

        CREATE INDEX ClusterAds_Vertical_I_cid ON ClusterAds_Vertical (cid);
        CREATE INDEX ClusterAds_Vertical_I_attr ON ClusterAds_Vertical (attr);
        CREATE INDEX ClusterAds_Vertical_I_val ON ClusterAds_Vertical (val);
        CREATE INDEX ClusterAds_Vertical_I_attr_val ON ClusterAds_Vertical (attr, val);

        CREATE TABLE ProcAds_Vertical (
           cid          text,
           pid          text,
           attr         text,
           val          double precision,
           primary key (cid, pid, attr)
        );

        CREATE INDEX ProcAds_Vertical_I_cid ON ProcAds_Vertical (cid);
	CREATE INDEX ProcAds_Vertical_I_pid ON ProcAds_Vertical (pid);
        CREATE INDEX ProcAds_Vertical_I_attr ON ProcAds_Vertical (attr);
        CREATE INDEX ProcAds_Vertical_I_val ON ProcAds_Vertical (val);
        CREATE INDEX ProcAds_Vertical_I_attr_val ON ProcAds_Vertical (attr, val);
*/

#define SCHEMA_CREATE_PROCADS_TABLE_STR "CREATE TABLE ProcAds_Vertical (cid text, pid text, attr text, val text, primary key (cid, pid, attr)); CREATE INDEX ProcAds_Vertical_I_cid ON ProcAds_Vertical (cid); CREATE INDEX ProcAds_Vertical_I_pid ON ProcAds_Vertical (pid); CREATE INDEX ProcAds_Vertical_I_attr ON ProcAds_Vertical (attr); CREATE INDEX ProcAds_Vertical_I_val ON ProcAds_Vertical (val); CREATE INDEX ProcAds_Vertical_I_attr_val ON ProcAds_Vertical (attr, val); CREATE TABLE ProcAds_Horizontal(cid text, pid text, jobstatus integer, imagesize integer, remoteusercpu double precision, primary key(cid,pid));"

#define SCHEMA_CREATE_CLUSTERADS_TABLE_STR "CREATE TABLE ClusterAds_Vertical (cid text, attr text, val text, primary key (cid, attr)); CREATE INDEX ClusterAds_Vertical_I_cid ON ClusterAds_Vertical (cid); CREATE INDEX ClusterAds_Vertical_I_attr ON ClusterAds_Vertical (attr); CREATE INDEX ClusterAds_Vertical_I_val ON ClusterAds_Vertical (val); CREATE INDEX ClusterAds_Vertical_I_attr_val ON ClusterAds_Vertical (attr, val); CREATE TABLE ClusterAds_Horizontal(cid text, owner varchar(50), jobstatus integer, jobprio integer, imagesize integer, qdate timestamp(0) with time zone, remoteusercpu double precision, cmd text, args text, primary key(cid));"

/*
	Defintion of Create History Table String 

CREATE TABLE History_Str (
	id		text,
	attr	text,
	val		text,
	primary key (id, attr)
);

CREATE INDEX History_Str_I_id ON History_Str (id);
CREATE INDEX History_Str_I_attr ON History_Str (attr);
CREATE INDEX History_Str_I_val ON History_Str (val);
CREATE INDEX History_Str_I_attr_val ON History_Str (attr, val);

CREATE TABLE History_Num (
	id		text,
	attr	text,
	val		double precision,
	primary key (id, attr)
);

CREATE INDEX History_Num_I_id ON History_Num (id);
CREATE INDEX History_Num_I_attr ON History_Num (attr);
CREATE INDEX History_Num_I_val ON History_Num (val);
CREATE INDEX History_Num_I_attr_val ON History_Num (attr, val);
*/

#define SCHEMA_CREATE_HISTORY_TABLE_STR "CREATE TABLE History_Vertical (cid text, pid text, attr text, val text, primary key (cid, pid, attr)); CREATE INDEX History_Vertical_I_cid ON History_Vertical (cid); CREATE INDEX History_Vertical_I_pid ON ProcAds_Vertical (pid); CREATE INDEX History_Vertical_I_attr ON History_Vertical (attr); CREATE INDEX History_Vertical_I_val ON History_Vertical (val); CREATE INDEX History_Vertical_I_attr_val ON History_Vertical (attr, val); CREATE TABLE History_Horizontal (cid text, pid text, qdate timestamp(0) with time zone, owner varchar(20), numckpts integer, numrestarts integer, numsystemholds integer, condorversion text, condorplatform text, rootdir text, iwd text, jobuniverse integer, cmd text, minhosts integer, maxhosts integer, jobprio integer, user_j text, env text, userlog text, coresize integer, killsig varchar(20), rank double precision, in_j text, transferin boolean, out text, transferout boolean, err text, transfererr boolean, shouldtransferfiles varchar(10), transferfiles varchar(20), executablesize integer, diskusage integer, requirements text, filesystemdomain text, args text, lastmatchtime timestamp(0) with time zone, numjobmatches integer, jobstartdate timestamp(0) with time zone, jobcurrentstartdate timestamp(0) with time zone, jobruncount integer, filereadcount double precision, filereadbytes double precision, filewritecount double precision, filewritebytes double precision, fileseekcount double precision, totalsuspensions integer, imagesize integer, exitstatus integer, localusercpu double precision, localsyscpu double precision, remoteusercpu double precision, remotesyscpu double precision, bytessent double precision, bytesrecvd double precision, rscbytessent double precision, rscbytesrecvd double precision, exitcode integer, jobstatus integer, enteredcurrentstatus timestamp(0) with time zone, remotewallclocktime double precision, lastremotehost text, completiondate timestamp(0) with time zone, primary key(cid, pid)); CREATE INDEX History_Horizontal_I_cid ON History_Horizontal (cid); CREATE INDEX History_Horizontal_I_pid ON History_Horizontal (pid); CREATE INDEX History_Horizontal_I_owner ON History_Horizontal (owner);"

bool isHorizontalClusterAttribute(const char *attr) {
  if((strcasecmp(attr, "owner") == 0) ||
     (strcasecmp(attr, "jobstatus") == 0) ||
     (strcasecmp(attr, "jobprio") == 0) ||
     (strcasecmp(attr, "imagesize") == 0) ||
     (strcasecmp(attr, "qdate") == 0) ||
     (strcasecmp(attr, "remoteusercpu") == 0) ||
     (strcasecmp(attr, "cmd") == 0) ||
     (strcasecmp(attr, "args") == 0)) {
    return true;
  }
  return false;
  
}

bool isHorizontalProcAttribute(const char *attr) {
  if((strcasecmp(attr, "jobstatus") == 0) ||
     (strcasecmp(attr, "imagesize") == 0) ||
     (strcasecmp(attr, "remoteusercpu") == 0)) {
    return true;
  }     
  return false;
}

bool isHorizontalHistoryAttribute(const char *attr) {
  if((strcasecmp(attr, "qdate") == 0) || 
     (strcasecmp(attr, "owner") == 0) ||
     (strcasecmp(attr, "numckpts") == 0) ||
     (strcasecmp(attr, "numrestarts") == 0) ||
     (strcasecmp(attr, "numsystemholds") == 0) ||
     (strcasecmp(attr, "condorversion") == 0) ||
     (strcasecmp(attr, "condorplatform") == 0) ||
     (strcasecmp(attr, "rootdir") == 0) ||
     (strcasecmp(attr, "iwd") == 0) ||
     (strcasecmp(attr, "jobuniverse") == 0) ||
     (strcasecmp(attr, "cmd") == 0) ||
     (strcasecmp(attr, "minhosts") == 0) ||
     (strcasecmp(attr, "maxhosts") == 0) ||
     (strcasecmp(attr, "jobprio") == 0) ||
     (strcasecmp(attr, "user") == 0) ||     
     (strcasecmp(attr, "env") == 0) ||
     (strcasecmp(attr, "userlog") == 0) ||
     (strcasecmp(attr, "coresize") == 0) ||
     (strcasecmp(attr, "killsig") == 0) ||
     (strcasecmp(attr, "rank") == 0) ||
     (strcasecmp(attr, "in") == 0) ||
     (strcasecmp(attr, "transferin") == 0) ||
     (strcasecmp(attr, "out") == 0) ||
     (strcasecmp(attr, "transferout") == 0) ||
     (strcasecmp(attr, "err") == 0) ||
     (strcasecmp(attr, "transfererr") == 0) ||
     (strcasecmp(attr, "shouldtransferfiles") == 0) ||
     (strcasecmp(attr, "transferfiles") == 0) ||
     (strcasecmp(attr, "executablesize") == 0) ||
     (strcasecmp(attr, "diskusage") == 0) ||
     (strcasecmp(attr, "requirements") == 0) ||
     (strcasecmp(attr, "filesystemdomain") == 0) ||
     (strcasecmp(attr, "args") == 0) ||
     (strcasecmp(attr, "lastmatchtime") == 0) ||
     (strcasecmp(attr, "numjobmatches") == 0) ||
     (strcasecmp(attr, "jobstartdate") == 0) ||
     (strcasecmp(attr, "jobcurrentstartdate") == 0) ||
     (strcasecmp(attr, "jobruncount") == 0) ||
     (strcasecmp(attr, "filereadcount") == 0) ||
     (strcasecmp(attr, "filereadbytes") == 0) ||
     (strcasecmp(attr, "filewritecount") == 0) ||
     (strcasecmp(attr, "filewritebytes") == 0) ||
     (strcasecmp(attr, "fileseekcount") == 0) ||
     (strcasecmp(attr, "totalsuspensions") == 0) ||
     (strcasecmp(attr, "imagesize") == 0) ||
     (strcasecmp(attr, "exitstatus") == 0) ||
     (strcasecmp(attr, "localusercpu") == 0) ||
     (strcasecmp(attr, "localsyscpu") == 0) ||
     (strcasecmp(attr, "remoteusercpu") == 0) ||
     (strcasecmp(attr, "remotesyscpu") == 0) ||
     (strcasecmp(attr, "bytessent") == 0) ||
     (strcasecmp(attr, "bytesrecvd") == 0) ||
     (strcasecmp(attr, "rscbytessent") == 0) ||
     (strcasecmp(attr, "rscbytesrecvd") == 0) ||
     (strcasecmp(attr, "exitcode") == 0) ||
     (strcasecmp(attr, "jobstatus") == 0) ||
     (strcasecmp(attr, "enteredcurrentstatus") == 0) ||
     (strcasecmp(attr, "remotewallclocktime") == 0) ||
     (strcasecmp(attr, "lastremotehost") == 0) ||
     (strcasecmp(attr, "completiondate") == 0)) 

  {
    return true;
  }
  return false;
}

#endif
