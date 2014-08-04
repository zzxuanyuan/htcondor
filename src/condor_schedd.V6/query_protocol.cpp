
#include "condor_common.h"

#include <string>

#include "condor_debug.h"
#include "classad_log.h"
#include "classad/classad.h"
#include "condor_attributes.h"
#include "reli_sock.h"
#include "condor_daemon_core.h"

#include "query_protocol.h"

using namespace condor_schedd;

bool
QueryProtocol::sendJobErrorAd(int errorCode, const std::string &errorString)
{
        classad::ClassAd ad;
        ad.InsertAttr(ATTR_OWNER, 0);
        ad.InsertAttr(ATTR_ERROR_STRING, errorString);
        ad.InsertAttr(ATTR_ERROR_CODE, errorCode);

        m_stream->encode();
        if (!putClassAd(m_stream, ad) || !m_stream->end_of_message())
        {
                dprintf(D_ALWAYS, "Failed to send error ad for job ads query\n");
        }
        return false;
}

bool
QueryProtocol::sendDone()
{
        classad::ClassAd ad;
        ad.InsertAttr(ATTR_OWNER, 0);
        ad.InsertAttr(ATTR_ERROR_CODE, 0);
        m_stream->encode();
        if (!putClassAd(m_stream, ad) || !m_stream->end_of_message())
        {
                dprintf(D_ALWAYS, "Failed to send done message to job query.\n");
                return false;
        }
	dprintf(D_FULLDEBUG, "Total of %d ads matched.\n", m_matched);
        return true;
}

QueryProtocol::QueryProtocol(Stream *stream, int timeslice_ms)
	: m_it(EndIterator()),
	  m_stream(stream),
	  m_timeslice_ms(timeslice_ms),
          m_unfinished_eom(false),
          m_registered_socket(false),
	  m_matched(0)
{
}


int
QueryProtocol::execute(Stream *stream)
{
	QueryProtocol *qp = new QueryProtocol(stream);
	return qp->execute_internal();
}


int QueryProtocol::execute_internal()
{
        ClassAd queryAd;

        m_stream->decode();
        m_stream->timeout(15);
        if( !getClassAd(m_stream, queryAd) || !m_stream->end_of_message()) {
                dprintf(D_ALWAYS, "Failed to receive query on TCP: aborting\n");
                return FALSE;
        }

        classad::ExprTree *requirements = queryAd.Lookup(ATTR_REQUIREMENTS);
        if (!requirements) {
                classad::Value val; val.SetBooleanValue(true);
                requirements = classad::Literal::MakeLiteral(val);
                if (!requirements) return sendJobErrorAd(1, "Failed to create default requirements");
		queryAd.Insert(ATTR_REQUIREMENTS, requirements);
        }
        classad_shared_ptr<classad::ExprTree> requirements_ptr(requirements->Copy());

	m_it = BeginIterator(*requirements_ptr.get(), m_timeslice_ms);

        int proj_err = mergeProjectionFromQueryAd(queryAd, ATTR_PROJECTION, m_projection, true);
        if (proj_err < 0) {
                delete this;
                if (proj_err == -1) {
                        return sendJobErrorAd(2, "Unable to evaluate projection list");
                }
                return sendJobErrorAd(3, "Unable to convert projection list to string list");
        }

	classad::ClassAdUnParser unparser;
	std::string projection;
	const classad::ExprTree *expr = queryAd.Lookup(ATTR_PROJECTION);
	if (expr) {unparser.Unparse(projection, expr);}
	std::string requirements_str;
	expr = queryAd.Lookup(ATTR_REQUIREMENTS);
	if (expr) {unparser.Unparse(requirements_str, expr);}
	dprintf(D_FULLDEBUG, "Query info: requirements={%s}; projection={%s}\n", requirements_str.c_str(), projection.c_str());

        return finish(m_stream);
}

int QueryProtocol::finish(Stream *stream)
{
	ReliSock *sock = static_cast<ReliSock*>(m_stream);
	ClassAdLog::filter_iterator end = EndIterator();
	bool has_backlog = false;

	if (m_unfinished_eom)
	{
		int retval = sock->finish_end_of_message();
		if (sock->clear_backlog_flag())
		{
			return KEEP_STREAM;
		}
		else if (retval == 1)
		{
			m_unfinished_eom = false;
		}
		else if (!retval)
		{
			delete this;
			return sendJobErrorAd(5, "Failed to write EOM to wire");
		}
	}
	while ((m_it != end) && !has_backlog)
	{
		ClassAd* tmp_ad = *m_it++;
		if (!tmp_ad)
		{
				// Return to DC in case if our time ran out.
			has_backlog = true;
			break;
		}
		m_matched += 1;
		int retval = putClassAd(sock, *tmp_ad,
			PUT_CLASSAD_NON_BLOCKING | PUT_CLASSAD_NO_PRIVATE,
			m_projection.empty() ? NULL : &m_projection);
		if (retval == 2) {
			has_backlog = true;
		} else if (!retval) {
			delete this;
			return sendJobErrorAd(4, "Failed to write ClassAd to wire");
		}
		retval = sock->end_of_message_nonblocking();
		if (sock->clear_backlog_flag()) {
			m_unfinished_eom = true;
			has_backlog = true;
		}
	}
	if (has_backlog && !m_registered_socket) {
		int retval = daemonCore->Register_Socket(stream, "Client Response",
			(SocketHandlercpp)&QueryProtocol::finish,
			"Query Job Ads Continuation", this, ALLOW, HANDLE_WRITE);
		if (retval < 0) {
			delete this;
			return sendJobErrorAd(4, "Failed to write ClassAd to wire");
		}
		m_registered_socket = true;
	} else if (!has_backlog) {
		bool retval = sendDone();
		delete this;
			// TODO: this leaks m_stream if we haven't hit DC. FIX
		return retval;
	}
	return KEEP_STREAM;
}

