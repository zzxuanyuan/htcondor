#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_td.h"
#include "list.h"
#include "condor_classad.h"
#include "daemon.h"
#include "dc_schedd.h"
#include "MyString.h"

static void usage(void);

TransferD::TransferD()
{
	m_initialized = FALSE;
	m_update_sock = NULL;
}

TransferD::~TransferD()
{	
	TransferRequest *treq;

	// walk through all of my transfers and simply get rid of them. Maybe
	// later I'll have to move this to the part where I do fast cleanup
	if (m_initialized == TRUE) {
		m_treqs.Rewind();
		while(m_treqs.Next(treq)) {
			m_treqs.DeleteCurrent();
			delete treq;
			treq = NULL;
		}
	}
}


/* I was told on the command line who my schedd is, so contact them
	and tell them I'm alive and ready for work. */
void
TransferD::init(int argc, char *argv[])
{
	RegisterResult ret;
	int i;
	ReliSock *usock = NULL;

	// XXX no error checking, assume arguments are correct.
	for (i = 1; i < argc; i++) {
		// if --schedd is given, take the sinful string of the schedd.
		if (strcmp(argv[i], "--schedd") == MATCH) {
			if (i+1 < argc) {
				g_td.m_features.set_schedd_sinful(argv[i+1]);
				i++;
			}
		}

		// if --stdin is specified, then there will be a TransferRequest 
		// supplied via stdin, otherwise, get it from the schedd.
		if (strcmp(argv[i], "--stdin") == MATCH) {
			g_td.m_features.set_uses_stdin(TRUE);
		}

		// if --id is specified, then there will be an ascii identification
		// string that the schedd will use to match this transferd with its
		// request.
		if (strcmp(argv[i], "--id") == MATCH) {
			g_td.m_features.set_id(argv[i+1]);
			i++;
		}

		// if --shadow is specified, then there will be an ascii direction
		// string that the transferd will use to determine the active file
		// transfer direction. The argument should be 'upload' or 'download'.
		if (strcmp(argv[i], "--shadow") == MATCH) {
			g_td.m_features.set_shadow_direction(argv[i+1]);
			i++;
		}

		// if --help, print a usage
		if (strcmp(argv[i], "--help") == MATCH) {
			usage();
		}
	}

	////////////////////////////////////////////////////////////////////////
	// start the initialization process

	// does there happen to be any work on the stdin port? If so, suck it into
	// the transfer collection

	if (g_td.m_features.get_uses_stdin() == TRUE) {
		dprintf(D_ALWAYS, "Loading transfer request from stdin...\n");
		g_td.accept_transfer_request(stdin);
	}

	// contact the schedd, if applicable, and tell it I'm alive.
	ret = g_td.register_to_schedd(&usock);

	switch(ret) {
		case REG_RESULT_NO_SCHEDD:
			// A schedd was not specified on the command line, so a finished
			// transfer will just be logged.
			break;
		case REG_RESULT_FAILED:
			// Failed to contact the schedd... Maybe bad sinful string?
			// For now, I'll mark it as an exceptional case, since it is 
			// either programmer error, or the schedd had gone away after
			// the request to start the transferd.
			EXCEPT("Failed to register to schedd! Aborting.");
			break;

		case REG_RESULT_SUCCESS:
			// stash the socket for later use.
			m_update_sock = usock;
			break;

		default:
			EXCEPT("TransferD::init() Programmer error!");
			break;
	}
}

/* stdin holds the work for this daemon to do, so let's read it */
int
TransferD::accept_transfer_request(FILE *fin)
{
	MyString encapsulation_method_line;
	MyString encap_end_line;
	EncapMethod em;
	int rval;

	/* The first line of stdin represents an encapsulation method. The
		encapsulation method I'm using at the time of writing is old classads.
		In the future, there might be new classads which have a different
		format, or something entirely different */

	if (encapsulation_method_line.readLine(fin) == FALSE) {
		EXCEPT("Failed to read encapsulation method line!");
	}
	encapsulation_method_line.trim();

	em = encap_method(encapsulation_method_line);

	/* now, call the right initialization function based upon the encapsulation
		method */
	switch (em) {

		case ENCAP_METHOD_UNKNOWN:
			EXCEPT("I don't understand the encapsulation method of the "
					"protocol: %s\n", encapsulation_method_line.Value());
			break;

		case ENCAP_METHOD_OLD_CLASSADS:
			rval = accept_transfer_request_encapsulation_old_classads(fin);
			break;

		default:
			EXCEPT("TransferD::init(): Programmer error! encap unhandled!");
			break;
	}

	m_initialized = TRUE;

	return rval;
}

/* Continue reading from stdin the rest of the protocol for this encapsulation
	method */
int
TransferD::accept_transfer_request_encapsulation_old_classads(FILE *fin)
{
	int i;
	int eof, error, empty;
	char *classad_delimitor = "---\n";
	ClassAd *ad;
	TransferRequest *treq = NULL;

	/* read the transfer request header packet upon construction */
	ad = new ClassAd(fin, classad_delimitor, eof, error, empty);
	if (empty == TRUE) {
		EXCEPT("Protocol faliure, can't read initial Info Packet");
	}

	// initialize the header information of the TransferRequest object.
	treq = new TransferRequest(ad);
	if (treq == NULL) {
		EXCEPT("Out of memory!");
	}

	treq->dprintf(D_ALWAYS);
	
	/* read the information packet which describes the rest of the protocol */
	if (treq->get_num_transfers() <= 0) {
		EXCEPT("Protocol error!");
	}

	// read all the work ads associated with this TransferRequest
	for (i = 0; i < treq->get_num_transfers(); i++) {
		ad = new ClassAd(fin, classad_delimitor, eof, error, empty);
		if (empty == TRUE) {
			EXCEPT("Expected %d transfer job ads, got %d instead.", 
				treq->get_num_transfers(), i);
		}
		ad->dPrint(D_ALWAYS);
		treq->append_task(ad);
	}

	/* now append the work request into the transferd's request structure, 
		assuming ownership of the pointer */
	if (m_treqs.Append(treq) == false) {
		EXCEPT("punk.");
	}

	return TRUE;
}

int
TransferD::accept_transfer_request_handler(int cmd, Stream *sock)
{
	MyString encapsulation_method_line;
	MyString encap_end_line;
	EncapMethod em;
	char *str = NULL;
	int rval;

	/* The first line of protocol represents an encapsulation method. The
		encapsulation method I'm using at the time of writing is old classads.
		In the future, there might be new classads which have a different
		format, or something entirely different */

	sock->decode();

	sock->code(str); // must free str...
	encapsulation_method_line = str; // makes a copy
	free(str);

	if (encapsulation_method_line.Value() == FALSE) {
		EXCEPT("Failed to read encapsulation method line!");
	}
	encapsulation_method_line.trim();

	em = encap_method(encapsulation_method_line);

	/* now, call the right initialization function based upon the encapsulation
		method */
	switch (em) {

		case ENCAP_METHOD_UNKNOWN:
			EXCEPT("I don't understand the encapsulation method of the "
					"protocol: %s\n", encapsulation_method_line.Value());
			break;

		case ENCAP_METHOD_OLD_CLASSADS:
			rval = accept_transfer_request_encapsulation_old_classads(cmd, sock);
			break;

		default:
			EXCEPT("TransferD::init(): Programmer error! encap unhandled!");
			break;
	}

	m_initialized = TRUE;

	// XXX This needs to be changed to keep the stream open.
	return CLOSE_STREAM;
}

/* Continue reading from rsock the rest of the protcol for this encapsulation
	method */
int
TransferD::accept_transfer_request_encapsulation_old_classads(int cmd, Stream *sock)
{
	int i;
	ClassAd *ad;
	TransferRequest *treq = NULL;

	/* read the transfer request header packet upon construction */
	ad = new ClassAd();
	if (ad->initFromStream(*sock) == 0) {
		EXCEPT("XXX Couldn't init initial ad from stream!");
	}

	// initialize the header information of the TransferRequest object.
	treq = new TransferRequest(ad);
	if (treq == NULL) {
		EXCEPT("Out of memory!");
	}

	treq->dprintf(D_ALWAYS);
	
	/* read the information packet which describes the rest of the protocol */
	if (treq->get_num_transfers() <= 0) {
		EXCEPT("Protocol error!");
	}

	// read all the work ads associated with this TransferRequest
	for (i = 0; i < treq->get_num_transfers(); i++) {
		ad = new ClassAd();
		if (ad == NULL) {
			EXCEPT("Out of memory!");
		}
		if (ad->initFromStream(*sock) == 0) {
			EXCEPT("Expected %d transfer job ads, got %d instead.", 
				treq->get_num_transfers(), i);
		}
		ad->dPrint(D_ALWAYS);
		treq->append_task(ad);
	}

	/* now append the work request into the transferd's request structure, 
		assuming ownership of the pointer */
	m_treqs.Append(treq);

	// Hmm...
	sock->eom();

	return CLOSE_STREAM;
}

// This function calls up the schedd passed in on the command line and 
// registers the transferd as being available for the schedd's use.
RegisterResult
TransferD::register_to_schedd(ReliSock **regsock_ptr)
{
	CondorError errstack;
	MyString sname;
	MyString id;
	MyString sinful;
	bool rval;
	ClassAd XXX_test;
	
	if (*regsock_ptr != NULL) {
		*regsock_ptr = NULL;
	}

	sname = m_features.get_schedd_sinful();
	id = m_features.get_id();

	if (sname == "N/A") {
		// no schedd supplied with which to register
		dprintf(D_ALWAYS, "No schedd specified to which to register.\n");
		return REG_RESULT_NO_SCHEDD;
	}
	
	// what is my sinful string?
	sinful = daemonCore->InfoCommandSinfulString(-1);

	dprintf(D_FULLDEBUG, "About to register my self(%s) to schedd(%s)\n",
		sinful.Value(), sname.Value());

	// hook up to the schedd.
	DCSchedd schedd(sname.Value(), NULL);

	// register myself, give myself 1 minute to connect.
	rval = schedd.register_transferd(sinful, id, 20*3, regsock_ptr, &errstack);

	if (rval == false) {
		// emit why 
		dprintf(D_ALWAYS, "TransferRequest::register_to_schedd(): Failed to "
			"register. Schedd gave reason '%s'\n", errstack.getFullText());
		return REG_RESULT_FAILED;
	}

	// ok, let's send an ad over just so see if the schedd can see the update...
	XXX_test.Insert("Wallaby = TRUE");
	XXX_test.Insert("OakTree = FALSE");
	XXX_test.put(*(Stream*)(*regsock_ptr));
	(*regsock_ptr)->eom();

	return REG_RESULT_SUCCESS;
}

void
TransferD::register_handlers(void)
{
	// for condor squawk.
	daemonCore->Register_Command(DUMP_STATE,
			"DUMP_STATE",
			(CommandHandlercpp)&TransferD::dump_state_handler,
			"dump_state_handler", this, READ);

	// The schedd will open a permanent connection to the transferd via this
	// particular handler and periodically give file transfer requests to the
	// transferd for subsequent processing.
	daemonCore->Register_Command(TRANSFERD_TRANSFER_REQUEST,
			"TRANSFERD_TRANSFER_REQUEST",
			(CommandHandlercpp)&TransferD::accept_transfer_request_handler,
			"accept_transfer_request_handler", this, WRITE);

	// write files into the storage area the transferd is responsible for, this
	// could be spool, or the initial dir.
	daemonCore->Register_Command(TRANSFERD_WRITE_FILES,
			"TRANSFERD_WRITE_FILES",
			(CommandHandlercpp)&TransferD::write_files_handler,
			"write_files_handler", this, WRITE);

	// read files from the storage area the transferd is responsible for, this
	// could be spool, or the initial dir.
	daemonCore->Register_Command(TRANSFERD_READ_FILES,
			"TRANSFERD_READ_FILES",
			(CommandHandlercpp)&TransferD::read_files_handler,
			"read_files_handler", this, READ);
	
	// register the reaper
	daemonCore->Register_Reaper("Reaper", 
		(ReaperHandlercpp)&TransferD::reaper_handler, "Reaper", this);
}

void
TransferD::register_timers(void)
{
	// begin processing any active requests, if there was information passed in
	// via stdin, then this'll get acted on very quickly
	daemonCore->Register_Timer( 0, 20,
		(TimerHandlercpp)&TransferD::process_active_requests_timer,
		"TransferD::process_active_requests_timer", this );
}


void usage(void)
{
	dprintf(D_ALWAYS, 
		"Usage info:\n"
		"--schedd <sinful>: Address of the schedd the transferd will contact\n"
		"--stdin:           Accept a transfer request on stdin\n"
		"--id <ascii>:      Used by the schedd to pair transferds to requests\n"
		"--shadow <upload|download>:\n"
		"                   Used with --stdin, transferd connects to shadow.\n"
		"                   This is demo mode with the starter.\n");

	DC_Exit(0);
}
