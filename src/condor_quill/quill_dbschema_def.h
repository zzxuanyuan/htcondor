/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef _QUILL_SCHEMA_DEF_H_
#define _QUILL_SCHEMA_DEF_H_

// the following schema check sql is for postgresql
/*
#define SCHEMA_CHECK_STR "SELECT relname FROM pg_class WHERE relname = 'clusterads_num' OR relname = 'clusterads_str' OR relname = 'history_horizontal' OR relname = 'history_vertical' OR relname = 'procads_num' OR relname = 'procads_str' OR relname = 'jobqueuepollinginfo' OR relname='procads' OR relname='clusterads';"
*/

// the following schema check sql is for oracle
#define SCHEMA_CHECK_STR "SELECT object_name FROM user_objects WHERE object_name = 'CLUSTERADS_NUM' OR object_name = 'CLUSTERADS_STR' OR object_name = 'HISTORY_HORIZONTAL' OR object_name = 'HISTORY_VERTICAL' OR object_name = 'PROCADS_NUM' OR object_name = 'PROCADS_STR' OR object_name = 'JOBQUEUEPOLLINGINFO' OR object_name='PROCADS' OR object_name='CLUSTERADS'"

//this includes the 2 views
#define SCHEMA_SYS_TABLE_NUM 	9

// the following sql is for postgresql
/*
#define SCHEMA_VERSION_STR "SELECT major, minor, back_to_major, back_to_minor FROM quill_schema_version LIMIT 1;"
*/

// the following sql is for oracle

#define SCHEMA_VERSION_STR "SELECT major, minor, back_to_major, back_to_minor FROM quill_schema_version WHERE rownum <=1"

#define SCHEMA_VERSION_COUNT 	1

#define SCHEMA_CREATE_SCHEMA_VERSION_TABLE_STR "CREATE TABLE quill_schema_version (major int, minor int, back_to_major int, back_to_minor int)"

#define SCHEMA_INSERT_SCHEMA_VERSION_TABLE_STR "INSERT INTO quill_schema_version (major, minor, back_to_major, back_to_minor) VALUES (1,0,1,0)"

/* 
	Definition of Delete String 

	DELETE FROM ProcAds_Str;
	DELETE FROM ProcAds_Num; 
	DELETE FROM ClusterAds_Str;
	DELETE FROM ClusterAds_Num; 
	DELETE FROM JobQueuePollingInfo;
*/

/* the following sql is for postgresql, it has to be broken up to individual
   sqls for oracle if it will be used 
*/
#define SCHEMA_DELETE_STR "DELETE FROM ProcAds_Str; DELETE FROM ProcAds_Num; DELETE FROM ClusterAds_Str; DELETE FROM ClusterAds_Num; DELETE FROM JobQueuePollingInfo;"


/*
	Definition of Drop Table String 

	DROP TABLE ProcAds_Str;
	DROP TABLE ProcAds_Num;
	DROP TABLE ClusterAds_Str;
	DROP TABLE ClusterAds_Num;
	DROP TABLE History_Horizontal;
	DROP TABLE History_Vertical;
	DROP TABLE JobQueuePollingInfo;
        DROP VIEW ProcAds;
        DROP VIEW ClusterAds;
*/

/* the following sql is for postgresql, it has to be broken up to individual
   sqls for oracle if it will be used 
*/
#define SCHEMA_DROP_TABLE_STR "DROP TABLE ProcAds_Str; DROP TABLE ProcAds_Num; DROP TABLE ClusterAds_Str; DROP TABLE ClusterAds_Num; DROP TABLE History_Horizontal; DROP TABLE History_Vertical; DROP TABLE JobQueuePollingInfo;DROP VIEW ProcAds; DROP VIEW ClusterAds;"

/*
	Definition of Create ProcAds Table String 

	CREATE TABLE ProcAds_Str (
		cid		int,
		pid		int,
		attr		varchar2(4000),
		val		varchar2(4000),
		primary key (cid, pid, attr)
	);

#	CREATE INDEX ProcAds_Str_I_cid ON ProcAds_Str (cid);
#	CREATE INDEX ProcAds_Str_I_pid ON ProcAds_Str (pid);
#	CREATE INDEX ProcAds_Str_I_attr ON ProcAds_Str (attr);
#	CREATE INDEX ProcAds_Str_I_val ON ProcAds_Str (val);
#	CREATE INDEX ProcAds_Str_I_attr_val ON ProcAds_Str (attr, val);
all indices are removed.  Only the pkey index created by default exists

	CREATE TABLE ProcAds_Num (
		cid		int,
		pid		int,
		attr		varchar2(4000),
		val		double precision,
		primary key (cid, pid, attr)
	);

#	CREATE INDEX ProcAds_Num_I_cid ON ProcAds_Num (cid);
#	CREATE INDEX ProcAds_Num_I_pid ON ProcAds_Num (pid);
#	CREATE INDEX ProcAds_Num_I_attr ON ProcAds_Num (attr);
#	CREATE INDEX ProcAds_Num_I_val ON ProcAds_Num (val);
#	CREATE INDEX ProcAds_Num_I_attr_val ON ProcAds_Num (attr, val);
all indices are removed.  Only the pkey index created by default exists

    Definition of ProcAds view
    CREATE VIEW ProcAds as 
	select cid, pid, attr, val from ProcAds_Str UNION ALL
	select cid, pid, attr, cast(val as varchar2(4000)) from ProcAds_Num;
*/

#define SCHEMA_CREATE_PROCADS_STR "CREATE TABLE ProcAds_Str (cid int, pid int, attr varchar2(4000), val varchar2(4000), primary key(cid, pid, attr))"

#define SCHEMA_CREATE_PROCADS_NUM "CREATE TABLE ProcAds_Num (cid int, pid int, attr varchar2(4000), val double precision, primary key(cid,pid,attr))"

#define SCHEMA_CREATE_PROCADS_VIEW "CREATE VIEW ProcAds as select cid, pid, attr, val from ProcAds_Str UNION ALL select cid, pid, attr, cast(val as varchar2(4000)) from ProcAds_Num"

#define SCHEMA_GRANT_PROCADS_STR "GRANT SELECT ON ProcAds_Str TO quillreader"

#define SCHEMA_GRANT_PROCADS_NUM "GRANT SELECT ON ProcAds_Num TO quillreader"

#define SCHEMA_GRANT_PROCADS_VIEW "GRANT SELECT ON ProcAds TO quillreader"

/*
	Definition of Create ClusterAds Table String 

CREATE TABLE ClusterAds_Str (
	cid		int,
	attr		varchar2(4000),
	val		varchar2(4000),
	primary key (cid, attr)
);

#CREATE INDEX ClusterAds_Str_I_cid ON ClusterAds_Str (cid);
#CREATE INDEX ClusterAds_Str_I_attr ON ClusterAds_Str (attr);
#CREATE INDEX ClusterAds_Str_I_val ON ClusterAds_Str (val);
#CREATE INDEX ClusterAds_Str_I_attr_val ON ClusterAds_Str (attr, val);
all indices are removed.  Only the pkey index created by default exists

CREATE TABLE ClusterAds_Num (
	cid		int,
	attr		varchar2(4000),
	val		double precision,
	primary key (cid, attr)
);

#CREATE INDEX ClusterAds_Num_I_cid ON ClusterAds_Num (cid);
#CREATE INDEX ClusterAds_Num_I_attr ON ClusterAds_Num (attr);
#CREATE INDEX ClusterAds_Num_I_val ON ClusterAds_Num (val);
#CREATE INDEX ClusterAds_Num_I_attr_val ON ClusterAds_Num (attr, val);
all indices are removed.  Only the pkey index created by default exists


    Definition of ClusterAds view
    CREATE VIEW ClusterAds as 
	select cid, attr, val from ClusterAds_Str UNION ALL
	select cid, attr, cast(val as varchar2(4000)) from ClusterAds_Num;
*/

#define SCHEMA_CREATE_CLUSTERADS_STR "CREATE TABLE ClusterAds_Str (cid int, attr varchar2(4000), val varchar2(4000), primary key(cid, attr))"

#define SCHEMA_CREATE_CLUSTERADS_NUM "CREATE TABLE ClusterAds_Num (cid int, attr varchar2(4000), val double precision, primary key(cid, attr))"

#define SCHEMA_CREATE_CLUSTERADS_VIEW "CREATE VIEW ClusterAds as select cid, attr, val from ClusterAds_Str UNION ALL select cid, attr, cast(val as varchar2(4000)) from ClusterAds_Num"

#define SCHEMA_GRANT_CLUSTERADS_STR "GRANT SELECT ON ClusterAds_Str TO quillreader"

#define SCHEMA_GRANT_CLUSTERADS_NUM "GRANT SELECT ON ClusterAds_Num TO quillreader"

#define SCHEMA_GRANT_CLUSTERADS_VIEW "GRANT SELECT ON ClusterAds TO quillreader"

/*
	Definition of Create JobQueuePollingInfo Table String 

CREATE TABLE JobQueuePollingInfo (
	last_file_mtime		BIGINT,
	last_file_size		BIGINT,
	last_next_cmd_offset	BIGINT,
	last_cmd_offset		BIGINT,
	last_cmd_type		SMALLINT,
	last_cmd_key		varchar2(4000),
	last_cmd_mytype 	varchar2(4000),
	last_cmd_targettype 	varchar2(4000),
	last_cmd_name		varchar2(4000),
	last_cmd_value		varchar2(4000)
	log_seq_num		BIGINT,
	log_creation_time	BIGINT
);

INSERT INTO JobQueuePollingInfo (last_file_mtime, last_file_size,log_seq_num,log_creation_time) VALUES (0,0,0,0);
*/

#define SCHEMA_CREATE_JOBQUEUEPOLLINGINFO_TABLE_STR "CREATE TABLE JobQueuePollingInfo (last_file_mtime INT, last_file_size INT, last_next_cmd_offset INT, last_cmd_offset INT, last_cmd_type SMALLINT, last_cmd_key varchar2(4000), last_cmd_mytype varchar2(4000), last_cmd_targettype varchar2(4000), last_cmd_name varchar2(4000), last_cmd_value varchar2(4000), log_seq_num INT, log_creation_time INT)"

#define SCHEMA_INSERT_JOBQUEUEPOLLINGINFO_TABLE_STR "INSERT INTO JobQueuePollingInfo (last_file_mtime, last_file_size, log_seq_num, log_creation_time) VALUES (0,0,0,0)"

/*
        Definition of Create History Tables String
  
CREATE TABLE History_Vertical (
        cid     int,
	pid     int,
        attr    varchar2(4000),
        val     varchar2(4000),
        primary key (cid, pid, attr)
);
  
CREATE INDEX History_Vertical_I_attr_val ON History_Vertical (attr, val);

CREATE TABLE History_Horizontal(
        cid                  int, 
        pid                  int, 
	EnteredHistoryTable  timestamp with time zone
        Owner                varchar2(4000), 
        QDate                int, 
        RemoteWallClockTime  int, 
        RemoteUserCpu        float, 
        RemoteSysCpu         float, 
        ImageSize            int, 
        JobStatus            int, 
        JobPrio              int, 
        Cmd                  varchar2(4000), 
        CompletionDate       int, 
        LastRemoteHost       varchar2(4000), 
        primary key(cid,pid)
); 

CREATE INDEX History_Horizontal_I_Owner on History_Horizontal(Owner);      
CREATE INDEX History_Horizontal_I_CompletionDate on History_Horizontal(CompletionDate);      
CREATE INDEX History_Horizontal_I_EnteredHistoryTable on History_Horizontal(EnteredHistoryTable);      

Note that some column names in the horizontal table are surrounded
by quotes in order to make them case-sensitive.  This is so that when
we convert the column names to their corresponding classad attribute name,
they look the same as the corresponding ad stored in the history file
*/

#define SCHEMA_CREATE_HISTORY_V_TABLE_STR "CREATE TABLE History_Vertical (cid int, pid int, attr varchar2(4000), val varchar2(4000), primary key (cid, pid, attr))"

#define SCHEMA_CREATE_HISTORY_IDX1_STR "CREATE INDEX History_Vertical_I_attr_val ON History_Vertical (attr)"

#define SCHEMA_CREATE_HISTORY_H_TABLE_STR "CREATE TABLE History_Horizontal(cid int, pid int, \"EnteredHistoryTable\" timestamp with time zone, \"Owner\" varchar2(4000), \"QDate\" int, \"RemoteWallClockTime\" int, \"RemoteUserCpu\" float, \"RemoteSysCpu\" float, \"ImageSize\" int, \"JobStatus\" int, \"JobPrio\" int, \"Cmd\" varchar2(4000), \"CompletionDate\" int, \"LastRemoteHost\" varchar2(4000), primary key(cid,pid))"

#define SCHEMA_CREATE_HISTORY_IDX2_STR "CREATE INDEX History_H_I_Owner on History_Horizontal(\"Owner\")"

#define SCHEMA_CREATE_HISTORY_IDX3_STR "CREATE INDEX History_H_I_CompletionDate on History_Horizontal(\"CompletionDate\")"

#define SCHEMA_CREATE_HISTORY_IDX4_STR "CREATE INDEX History_H_I_EHT on History_Horizontal(\"EnteredHistoryTable\")"

#define SCHEMA_GRANT_HISTORY_H_STR "GRANT SELECT ON History_Horizontal TO quillreader"

#define SCHEMA_GRANT_HISTORY_V_STR "GRANT SELECT ON History_Vertical TO quillreader"

#endif

