#ifndef TRANSFER_REQUEST_H
#define TRANSFER_REQUEST_H

#include "extArray.h"
#include "MyString.h"
#include "file_transfer.h"

// Used to determine the type of protocol encapsulation the transferd desires.
enum EncapMethod {
	ENCAP_METHOD_UNKNOWN = 0,
	ENCAP_METHOD_OLD_CLASSADS,
};

// This enum is mostly usd by the TransferRequest objects methods 
enum SchemaCheck {
	INFO_PACKET_SCHEMA_UNKNOWN = 0,
	INFO_PACKET_SCHEMA_OK,
	INFO_PACKET_SCHEMA_NOT_OK,
};

/* This describes who is supposed to initiate the processing of a specific
	transfer request. Either some other client program, which is passive,
	or the transferd itself, which is active. */
enum TreqMode {
	TREQ_MODE_UNKNOWN = 0,
	TREQ_MODE_ACTIVE,
	TREQ_MODE_PASSIVE,
	TREQ_MODE_ACTIVE_SHADOW, /* XXX DEMO mode */
};

// Move to a different header file when done!
extern const char ATTR_IP_PROTOCOL_VERSION[];
extern const char ATTR_IP_NUM_TRANSFERS[];
extern const char ATTR_IP_TRANSFER_SERVICE[];
extern const char ATTR_IP_PEER_VERSION[];

// This class is a delegation class the represents a particular request from
// anyone (usually the schedd) to transfer some files associated with a set of
// jobs. Later, someone will come by and whatever they say must match a
// previous request. 
class TransferRequest
{
	public:
		// I can initialize all of my internal variables via a classad with
		// a special schema.
		TransferRequest(ClassAd *ip); // assume ownership of pointer
		TransferRequest(); // init with empty classad
		~TransferRequest();

		/////////////////////////////////////////////////////////////////////
		// this is stuff that is supplied in the initializing classad.
		/////////////////////////////////////////////////////////////////////

		// What is the version string of the peer I'm talking to?
		// This could be the empty string if there is no version.
		// this will make a copy when you assign it to something.
		void set_peer_version(MyString &pv);
		void set_peer_version(char *pv);
		MyString get_peer_version(void);

		// what version is the info packet
		void set_protocol_version(int);
		int get_protocol_version(void);

		// Should this request be handled Passively, Actively, or Active Shadow
		void set_transfer_service(TreqMode mode);
		void set_transfer_service(MyString &str);
		void set_transfer_service(const char *str);
		TreqMode get_transfer_service(void);

		// How many transfers am I going to process? Each transfer is on
		// behalf of a job
		void set_num_transfers(int);
		int get_num_transfers(void);

		/////////////////////////////////////////////////////////////////////
		// This deals with manipulating the payload of ads(tasks) to work on
		/////////////////////////////////////////////////////////////////////

		// add a jobad to the transfer request, this accepts ownership
		// of the memory passed to it.
		void append_task(ClassAd *jobad);

		// return the todo list for processing. Kinda of a bad break of
		// encapsulation, but it makes iterating over this thing so much
		// easier.
		SimpleList<ClassAd *>* todo_tasks(void);

		/////////////////////////////////////////////////////////////////////
		// Utility functions
		/////////////////////////////////////////////////////////////////////

		// Dump the packet to specified debug level
		void dprintf(unsigned int lvl);

	private:
		// Inspect the information packet during construction and verify the 
		// schemas I am prepared to handle.
		SchemaCheck check_schema(void);

		// this is the information packet this work packet has.
		ClassAd *m_ip;

		// Here is the list of jobads associated with this transfer request.
		SimpleList<ClassAd *> m_todo_ads;
};

/* converts a protcol ASCII line to an enum which represents and encapsulation
	method for the protocol. */
EncapMethod encap_method(MyString &line);

/* converts an ASCII representation of a transfer request mode into the enum */
TreqMode transfer_mode(MyString mode);
TreqMode transfer_mode(const char *mode);


#endif




