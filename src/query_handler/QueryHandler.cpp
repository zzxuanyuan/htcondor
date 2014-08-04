
#include "QueryHandler.h"

#include "condor_common.h"
#include "condor_config.h"
#include "../condor_schedd.V6/query_protocol.h"


QueryHandler::QueryHandler()
	: m_scheduler(NULL),
	  m_register_sock(NULL),
	  m_register_errstack(NULL)
{
}

void
QueryHandler::config()
{
	m_scheduler->config();

	int poll_interval = param_integer("QUERY_HANDLER_POLLLING_PERIOD", 300);
	ASSERT(poll_interval > 0);
	if( m_periodic_timer_id >= 0 ) {
		daemonCore->Cancel_Timer(m_periodic_timer_id);
		m_periodic_timer_id = -1;
	}
	m_periodic_timer_id =
		daemonCore->Register_Timer(
			poll_interval,
			poll_interval,
			(TimerHandlercpp)&QueryHandler::poll,
			"QueryHandler::poll", this);

	poll();
}


void
QueryHandler::init()
{
	int rc = daemonCore->Register_CommandWithPayload(QUERY_JOB_ADS, "QUERY_JOB_ADS",
		(CommandHandlercpp)&QueryHandler::handle,
		"QueryHandler::handle", this, READ);
	ASSERT(rc >= 0)

	ClassAdJobLogConsumer *log_consumer = new ClassAdJobLogConsumer();
	ASSERT(log_consumer);
	m_scheduler = new Scheduler(log_consumer, "QUERY_HANDLER_SCHEDD_SPOOL");
	ASSERT(m_scheduler);
	m_scheduler->init();

	config();
}


void
QueryHandler::stop()
{
	if (m_scheduler) {m_scheduler->stop();}
}

int
QueryHandler::handle(int /* Command Int */, Stream * stream)
{
	return condor_schedd::QueryProtocol::execute(stream);
}

int
QueryHandler::poll()
{
	m_register_ad.Clear();
	classad::Value value;
	std::string name = param("QUERY_HANDLER_NAME");
	value.SetStringValue(name);
	classad::ExprTree *lit = classad::Literal::MakeLiteral(value);
	m_register_ad.Insert(ATTR_NAME, lit);
	value.SetStringValue(name);

	if (!daemonCore->sharedPortId(name))
	{
		dprintf(D_ALWAYS, "Shared port is not in use.\n");
		return 1;
	}
	value.SetStringValue(name);
	lit = classad::Literal::MakeLiteral(value);
	m_register_ad.Insert("SharedPortId", lit);

	int priority = param_integer("QUERY_HANDLER_PRIORITY", 1);
	value.SetIntegerValue(priority);
	lit = classad::Literal::MakeLiteral(value);
	m_register_ad.Insert(ATTR_PRIO, lit);

	dprintf(D_FULLDEBUG, "Registration ad:\n");
	dPrintAd(D_FULLDEBUG, m_register_ad);

	Daemon schedd(DT_SCHEDD, NULL, NULL);
	if (!schedd.locate())
	{
		dprintf(D_ALWAYS, "Unable to locate local schedd.\n");
		return 1;
	}
	dprintf(D_FULLDEBUG, "Will handle queries for schedd ad %s.\n", schedd.addr());

	if (!m_register_errstack && !m_register_sock)
	{
		m_register_errstack = new CondorError();
		StartCommandResult result = schedd.startCommand_nonblocking(REGISTER_QUERY_HANDLER, Stream::reli_sock, 20, m_register_errstack,
			QueryHandler::RegisterStartCallback, this, "QueryHandler::RegisterStartCallback");
		if (result == StartCommandFailed)
		{
			delete m_register_errstack; m_register_errstack = NULL;
			dprintf(D_ALWAYS, "Unable to start registration command to local schedd.\n");
			return 1;
		}
	}
	return 0;
}

void
QueryHandler::RegisterStart(bool success)
{
	int errorCode;
	std::string errorMsg;
	classad::ClassAd response_ad;

	if (!success)
	{
		dprintf(D_ALWAYS, "Failed to start registration with local schedd: %s\n", m_register_errstack->getFullText().c_str());
		goto cleanup;
	}

	if (!putClassAd(m_register_sock, m_register_ad) || !m_register_sock->end_of_message())
	{
		dprintf(D_ALWAYS, "Failed to send registration ad to local schedd.\n");
		goto cleanup;
	}

	m_register_sock->decode();
	if (!getClassAd(m_register_sock, response_ad) || !m_register_sock->end_of_message())
	{
		dprintf(D_ALWAYS, "Failed to receive registration response.\n");
		goto cleanup;
	}
	if (response_ad.EvaluateAttrInt(ATTR_ERROR_CODE, errorCode) && 
		errorCode && response_ad.EvaluateAttrString(ATTR_ERROR_STRING, errorMsg))
	{
		dprintf(D_ALWAYS, "Error in registration: %s (error code=%d).\n", errorMsg.c_str(), errorCode);
	}

cleanup:
	delete m_register_errstack;
	m_register_errstack = NULL;
	delete m_register_sock;
	m_register_sock = NULL;
}

