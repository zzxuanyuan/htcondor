#include "condor_common.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "compat_classad.h"
#include "file_transfer.h"
#include "condor_version.h"
#include "classad_log.h"
#include "get_daemon_name.h"
#include "ipv6_hostname.h"
#include <list>
#include <map>
#include "basename.h"

#include "storage_optimizer_server.h"
#include "dc_storage_optimizer.h"

#include <sstream>

static int g_counter = 0;

int dummy_reaper(Service *, int pid, int) {
	dprintf(D_ALWAYS,"dummy-reaper: pid %d exited; ignored\n",pid);
	return TRUE;
}

StorageOptimizerServer::StorageOptimizerServer() {

	m_test_iterate_db_timer = daemonCore->Register_Timer (
			10,
			(TimerHandlercpp) &StorageOptimizerServer::TestIterateDB,
			"Test Iterate Database",
			(Service*)this );
	m_test_read_db_timer = daemonCore->Register_Timer (
			10,
			(TimerHandlercpp) &StorageOptimizerServer::TestReadDB,
			"Test Read Database",
			(Service*)this );
	m_test_write_db_timer = daemonCore->Register_Timer (
			5,
			(TimerHandlercpp) &StorageOptimizerServer::TestWriteDB,
			"Test Write Database",
			(Service*)this );
	// create a database for the first time
	InitializeDB();

	m_dummy_reaper = daemonCore->Register_Reaper("dummy_reaper",
			(ReaperHandler) &dummy_reaper,
			"dummy_reaper",NULL);

	ASSERT( m_dummy_reaper >= 0 );
}

StorageOptimizerServer::~StorageOptimizerServer() {
}

void StorageOptimizerServer::InitAndReconfig() {

	dprintf(D_FULLDEBUG, "enter StorageOptimizer::InitAndReconfig\n");
	m_db_fname = param("STORAGE_OPTIMIZER_DATABASE");
	dprintf(D_FULLDEBUG, "STORAGE_OPTIMIZER_DATABASE = %s\n", m_db_fname.c_str());
	m_log = new ClassAdLog<std::string,ClassAd*>(m_db_fname.c_str());
	InitializeDB();
	dprintf( D_FULLDEBUG, "exit StorageOptimizer::InitAndReconfig\n" );
}

void StorageOptimizerServer::InitializeDB() {
	dprintf(D_FULLDEBUG, "enter StorageOptimizer::InitializeDB\n");
	g_counter = 0;
	dprintf( D_FULLDEBUG, "exit StorageOptimizer::InitializeDB\n" );
}

void StorageOptimizerServer::SetAttributeString(const MyString& Key, const MyString& AttrName, const MyString& AttrValue)
{
	if (m_log->AdExistsInTableOrTransaction(Key.Value()) == false) {
		LogNewClassAd* log = new LogNewClassAd(Key.Value(), "*", "*");
		m_log->AppendLog(log);
	}

	MyString value;
	value.formatstr("\"%s\"",AttrValue.Value());
	LogSetAttribute* log = new LogSetAttribute(Key.Value(), AttrName.Value(), value.Value());
	m_log->AppendLog(log);
}

bool StorageOptimizerServer::GetAttributeString(const MyString& Key, const MyString& AttrName, MyString& AttrValue)
{
	ClassAd* ad;
	if (m_log->table.lookup(Key.Value(), ad) == -1) return false;

	if (ad->LookupString(AttrName.Value(), AttrValue) == 0) return false;
	return true;
}

void StorageOptimizerServer::TestIterateDB() {

	dprintf(D_FULLDEBUG, "enter StorageOptimizer::TestIterateDB\n");
	std::string HK;
	ClassAd* ad;

	m_log->BeginTransaction();
	const MyString attr_value = "University of Nebraska";
	m_log->table.startIterations();
	while (m_log->table.iterate(HK, ad)) {
		char *str;
		if (ad->LookupString("Sites", &str) && !strncmp(attr_value.Value(), str, attr_value.Length())) {
			dprintf(D_FULLDEBUG, "key is %s\n", HK.c_str());
			dPrintAd(D_FULLDEBUG, *ad);
			free(str);
		} else if (str) {
			dprintf(D_FULLDEBUG, "cluster is %s\n", str);
			free(str);
		} else {
			dprintf(D_FULLDEBUG, "cluster is empty\n");
		}
	}
	m_log->CommitTransaction();
	daemonCore->Reset_Timer(m_test_iterate_db_timer, 10);
	dprintf( D_FULLDEBUG, "exit StorageOptimizer::TestIterateDB\n" );
}

void StorageOptimizerServer::TestReadDB() {

	dprintf(D_FULLDEBUG, "enter StorageOptimizer::TestReadDB\n");
	MyString time_stamp = "time1";
	m_log->BeginTransaction();
	const MyString attr_name1 = "Sites";
	const MyString attr_value1 = "University of Nebraska";
	const MyString attr_name2 = "Clusters";
	const MyString attr_value2 = "Crane";

	MyString attr_read;	
	GetAttributeString(time_stamp, attr_name1, attr_read);
	if(attr_value1 != attr_read) {
		dprintf(D_FULLDEBUG, "they are not equal: attr_value1 = %s, attr_read = %s\n", attr_value1.Value(), attr_read.Value());
	}
	GetAttributeString(time_stamp, attr_name2, attr_read);
	if(attr_value2 != attr_read) {
		dprintf(D_FULLDEBUG, "they are not equal: attr_value1 = %s, attr_read = %s\n", attr_value2.Value(), attr_read.Value());
	}
	m_log->CommitTransaction();
	daemonCore->Reset_Timer(m_test_read_db_timer, 10);
	dprintf( D_FULLDEBUG, "exit StorageOptimizer::TestReadDB\n" );
}

void StorageOptimizerServer::TestWriteDB() {

	dprintf(D_FULLDEBUG, "enter StorageOptimizer::TestWriteDB\n");
	g_counter++;
	MyString time_stamp = "time" + std::to_string(g_counter);
	m_log->BeginTransaction();
	if (!m_log->AdExistsInTableOrTransaction(time_stamp.c_str())) {
		LogNewClassAd* new_log = new LogNewClassAd(time_stamp.c_str(), "*", "*");
		m_log->AppendLog(new_log);
	}
	const MyString attr_name1 = "Sites";
	const MyString attr_name2 = "Clusters";
	MyString attr_tmp1;
	MyString attr_tmp2;
	if(g_counter%3 == 0) {
		attr_tmp1 = "University of Nebraska";
		attr_tmp2 = "Crane" + std::to_string(g_counter);
	} else if(g_counter%3 == 1) {
		attr_tmp1 = "University of Wisconsin";
		attr_tmp2 = "MWT2" + std::to_string(g_counter);
	} else if(g_counter%3 == 2) {
		attr_tmp1 = "Syracuse University";
		attr_tmp2 = "SU-OG-CE" + std::to_string(g_counter);
	}
	const MyString attr_value1 = attr_tmp1;
	const MyString attr_value2 = attr_tmp2;

	SetAttributeString(time_stamp, attr_name1, attr_value1);
	SetAttributeString(time_stamp, attr_name2, attr_value2);
	m_log->CommitTransaction();
	daemonCore->Reset_Timer(m_test_write_db_timer, 10);
	dprintf( D_FULLDEBUG, "exit StorageOptimizer::TestWriteDB\n" );
}
