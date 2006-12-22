#include "condor_common.h"
#include "condor_debug.h"
#include "MyString.h"
#include "extArray.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_transfer_request.h"

const char ATTR_IP_PROTOCOL_VERSION[] = "ProtocolVersion";
const char ATTR_IP_NUM_TRANSFERS[] = "NumTransfers";
const char ATTR_IP_TRANSFER_SERVICE[] = "TransferService";
const char ATTR_IP_PEER_VERSION[] = "PeerVersion";

// This function assumes ownership of the pointer 
TransferRequest::TransferRequest(ClassAd *ip)
{
	ASSERT(ip != NULL);

	m_ip = ip;

	/* Since this schema check happens here I don't need to check the
		existance of these attributes when I use them. */
	ASSERT(check_schema() == INFO_PACKET_SCHEMA_OK);
}

TransferRequest::TransferRequest()
{
	m_ip = new ClassAd();
}

TransferRequest::~TransferRequest()
{
	delete m_ip;
	m_ip = NULL;
}

SchemaCheck
TransferRequest::check_schema(void)
{
	int version;

	ASSERT(m_ip != NULL);

	/* ALL info packets MUST have a protocol version number */

	/* Check to make sure it exists */
	if (m_ip->Lookup(ATTR_IP_PROTOCOL_VERSION) == NULL) {
		EXCEPT("TransferRequest::check_schema() Failed due to missing %d attribute",
			ATTR_IP_PROTOCOL_VERSION);
	}

	/* for now, this assumes version 0 of the protocol. */

	/* Check to make sure it resolves to an int, and determine it */
	if (m_ip->LookupInteger(ATTR_IP_PROTOCOL_VERSION, version) == 0) {
		EXCEPT("TransferRequest::check_schema() Failed. ATTR_IP_PROTOCOL_VERSION "
				"must be an integer.");
	}

	/* for now, just check existance of attribute, but not type */
	if (m_ip->Lookup(ATTR_IP_NUM_TRANSFERS) == NULL) {
		EXCEPT("TransferRequest::check_schema() Failed due to missing %d "
			"attribute", ATTR_IP_NUM_TRANSFERS);
	}

	if (m_ip->Lookup(ATTR_IP_TRANSFER_SERVICE) == NULL) {
		EXCEPT("TransferRequest::check_schema() Failed due to missing %d "
			"attribute", ATTR_IP_TRANSFER_SERVICE);
	}

	if (m_ip->Lookup(ATTR_IP_PEER_VERSION) == NULL) {
		EXCEPT("TransferRequest::check_schema() Failed due to missing %d "
			"attribute", ATTR_IP_PEER_VERSION);
	}


	// currently, this either excepts, or returns ok.
	return INFO_PACKET_SCHEMA_OK;
}

void
TransferRequest::append_task(ClassAd *ad)
{
	ASSERT(m_ip != NULL);
	m_todo_ads.Append(ad);
}

void
TransferRequest::dprintf(unsigned int lvl)
{
	MyString pv;

	ASSERT(m_ip != NULL);

	pv = get_peer_version();

	::dprintf(lvl, "TransferRequest Dump:\n");
	::dprintf(lvl, "\tProtocol Version: %d\n", get_protocol_version());
	::dprintf(lvl, "\tServer Mode: %u\n", get_transfer_service());
	::dprintf(lvl, "\tNum Transfers: %d\n", get_num_transfers());
	::dprintf(lvl, "\tPeer Version: %s\n", pv.Value());
}

void
TransferRequest::set_num_transfers(int nt)
{
	int num;
	MyString str;

	ASSERT(m_ip != NULL);

	str += ATTR_IP_NUM_TRANSFERS;
	str += " = ";
	str += nt;

	m_ip->InsertOrUpdate(str.Value());
}

int
TransferRequest::get_num_transfers(void)
{
	int num;

	ASSERT(m_ip != NULL);

	m_ip->LookupInteger(ATTR_IP_NUM_TRANSFERS, num);

	return num;
}

void
TransferRequest::set_transfer_service(MyString &mode)
{
	ASSERT(m_ip != NULL);

	set_transfer_service(mode.Value());
}

void
TransferRequest::set_transfer_service(const char *mode)
{
	MyString str;

	ASSERT(m_ip != NULL);

	str += ATTR_IP_TRANSFER_SERVICE;
	str += " = \"";
	str += mode;
	str += "\"";

	m_ip->InsertOrUpdate(str.Value());
}

void
TransferRequest::set_transfer_service(TreqMode mode)
{
	// XXX TODO
}


TreqMode
TransferRequest::get_transfer_service(void)
{
	MyString mode;
	MyString tmp;

	ASSERT(m_ip != NULL);

	m_ip->LookupString(ATTR_IP_TRANSFER_SERVICE, mode);

	return ::transfer_mode(mode);
}

void
TransferRequest::set_protocol_version(int pv)
{
	ASSERT(m_ip != NULL);

	MyString str;

	str += ATTR_IP_PROTOCOL_VERSION;
	str += " = ";
	str += pv;

	m_ip->InsertOrUpdate(str.Value());
}

int
TransferRequest::get_protocol_version(void)
{
	int version; 

	ASSERT(m_ip != NULL);

	m_ip->LookupInteger(ATTR_IP_PROTOCOL_VERSION, version);

	return version;
}

void
TransferRequest::set_peer_version(MyString &pv)
{
	MyString str;

	ASSERT(m_ip != NULL);

	str += ATTR_IP_PEER_VERSION;
	str += " = \"";
	str += pv;
	str += "\"";

	m_ip->InsertOrUpdate(str.Value());
}

void
TransferRequest::set_peer_version(char *pv)
{
	MyString str;
	ASSERT(m_ip != NULL);

	str = pv;

	set_peer_version(pv);
}

// This will make a copy when you assign the return value to something.
MyString
TransferRequest::get_peer_version(void)
{
	MyString pv;

	ASSERT(m_ip != NULL);

	m_ip->LookupString(ATTR_IP_PEER_VERSION, pv);

	return pv;
}

// do NOT delete this pointer, it is an alias pointer into the transfer request
SimpleList<ClassAd*>*
TransferRequest::todo_tasks(void)
{
	ASSERT(m_ip != NULL);

	return &m_todo_ads;
}

EncapMethod encap_method(MyString &line)
{
	if (line == "ENCAPSULATION_METHOD_OLD_CLASSADS") {
		return ENCAP_METHOD_OLD_CLASSADS;
	}

	return ENCAP_METHOD_UNKNOWN;
}

TreqMode transfer_mode(MyString mode)
{
	return transfer_mode(mode.Value());
}

TreqMode transfer_mode(const char *mode)
{
	if (mode == "Active") {
		return TREQ_MODE_ACTIVE;
	}

	if (mode == "ActiveShadow") {
		return TREQ_MODE_ACTIVE_SHADOW; /* XXX DEMO mode */
	}

	if (mode == "Passive") {
		return TREQ_MODE_PASSIVE;
	}

	return TREQ_MODE_UNKNOWN;
}
