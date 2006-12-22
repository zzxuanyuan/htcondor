#ifndef TRANSFERD_H
#define TRANSFERD_H

#include "extArray.h"
#include "MyString.h"
#include "file_transfer.h"
#include "condor_transfer_request.h"

// How successful was I in registering myself to a schedd, if available?
enum RegisterResult {
	REG_RESULT_FAILED = 0,
	REG_RESULT_SUCCESS,
	REG_RESULT_NO_SCHEDD,
};

// This class hold features and info that have been specified on the 
// command line.
class Features
{
	public:
		Features()
		{
			m_uses_stdin = FALSE;
			m_schedd_sinful = "N/A";
		}

		~Features() { }
		
		void set_schedd_sinful(char *sinful)
		{
			m_schedd_sinful = sinful;
		}

		void set_schedd_sinful(MyString sinful)
		{
			m_schedd_sinful = sinful;
		}

		MyString get_schedd_sinful(void)
		{
			return m_schedd_sinful;
		}
		
		// XXX DEMO hacking
		void set_shadow_direction(MyString direction)
		{
			m_shad_dir = direction;
		}

		// XXX DEMO hacking
		MyString get_shadow_direction(void)
		{
			return m_shad_dir;
		}

		void set_id(char *id)
		{
			m_id = id;
		}

		void set_id(MyString id)
		{
			m_id = id;
		}

		MyString get_id(void)
		{
			return m_id;
		}

		void set_uses_stdin(int b)
		{
			m_uses_stdin = b;
		}

		int get_uses_stdin(void)
		{
			return m_uses_stdin;
		}

	private:
		// --schedd <sinfulstring>
		// The schedd with which the transferd registers itself.
		MyString m_schedd_sinful;

		// --stdin
		// Presence of this flag says that the transferd will *also* grab
		// TransferObject requests from stdin. Otherwise, the default method
		// of the schedd having to contact the transferd via command port
		// is the only method.
		int m_uses_stdin;

		// --id <ascii_key>
		// The identity that the schedd will use to match this transferd
		// with the request for it.
		MyString m_id;

		// XXX DEMO hacking
		// --shadow <upload|download>
		// In this mode, the transferd must get an active request on stdin.
		// Then the transferd will simply init a file transfer object for
		// connecting to the shadow and pass it the job ad supplied.
		MyString m_shad_dir;
};

class TransferD : public Service
{
	public:
		TransferD();
		~TransferD();

		// located in the initialization implementation file

		// parse the ocmmand line arguments, soak any TransferRequests 
		// that are immediately available, call up the schedd and register
		// if needed.
		void init(int argc, char *argv[]);

		// The transferd will read what work it needs to perform from
		// either stdin, if asked for, or a command handler.
		int accept_transfer_request(FILE *fin);
		int accept_transfer_request_handler(int cmd, Stream *fin);

		void register_handlers(void);
		void register_timers(void);

		// located in the maintenance implementation file
		void reconfig(void);
		void shutdown_fast(void);
		void shutdown_graceful(void);
		void transferd_exit(void);

		// This function digs through the TransferRequest list and finds all
		// TransferRequests where the transferd itself has to initiate a
		// movement of file from the transferd's storage to/from another
		// transferd, or to some initial directory. 
		int initiate_active_transfer_requests(void);

		// in response to a condor_squawk command, dump a classad of the
		// internal state.
		int dump_state_handler(int cmd, Stream *sock);

		// how files get into and out of the transferd's storage space
		int write_files_handler(int cmd, Stream *sock);
		int write_files_reaper(int tid, int exit_status);
		int read_files_handler(int cmd, Stream *sock);
		int read_files_reaper(int tid, int exit_status);

		// a periodic timer to process any active requests
		int process_active_requests_timer();

		// handler for any exiting process.
		int reaper_handler(int pid, int exit_status);

		// Look through all of my TransferRequests for one that contains
		// a particular job id. 
		TransferRequest* find_transfer_request(int c, int p);

		// a set of features dictated by command line which the transferd 
		// will consult when wishing to do various things.
		Features m_features;

	private:
		////////////////////////////////////////////////////////////////////
		// methods

		// read the information using the old classads encapsulation
		int accept_transfer_request_encapsulation_old_classads(FILE *fin);
		int accept_transfer_request_encapsulation_old_classads(int cmd, Stream *fin);

		// Call up the schedd I was informed about to tell them I'm alive.
		// the returned pointer (if valid) is the connection that the 
		// transferd will use to send status updates about completed transfers.
		RegisterResult register_to_schedd(ReliSock **regsock_ptr);

		// process a single active request
		int process_active_request(TransferRequest *treq);
		int process_active_shadow_request(TransferRequest *treq); // XXX demo
		int active_shadow_transfer_completed( FileTransfer *ftrans );

		////////////////////////////////////////////////////////////////////
		// variables

		// Has the init() call been called? 
		int m_initialized;

		// The list of transfers that have been requested of me to do when
		// someone contacts me.
		SimpleList<TransferRequest*> m_treqs;

		// XXX need two hash tables, one for writing thread reapers, one for
		// reading thread reapers.

		// After the transferd registers to the schedd, the connection to
		// the schedd is stored here. This allows the transferd to send
		// update messaged to the schedd about transfers that have completed.
		// This is NULL if there is no schedd to contact.
		ReliSock *m_update_sock;
};

extern TransferD g_td;

#endif




